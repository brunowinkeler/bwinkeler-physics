# ADR 004: Render simulation controls in the DOM

- Status: accepted
- Date: 2026-08-05
- Applies to: `ballistics`, `pendulum`, `gas-brownian`

## Context

The browser adapters reused the native Dear ImGui panels, so every parameter,
readout, and analysis plot was drawn inside the WebGL canvas. That surface
competed with the scene for space, could not be reached by assistive technology,
ignored the site's typography and theme, and forced the canvas to carry the cost
of the full Dear ImGui draw pipeline.

The native repository still needs those panels for its desktop applications, and
ADR 002 keeps `cpp-physics-simulation` free of browser concerns.

## Decision

Every browser simulation renders its controls as ordinary HTML beside the
canvas. A simulation:

- excludes its `*UiMenus.cpp` and canvas overlay renderer from the web target;
- adds an adapter-owned `extern "C"` bridge exposing commands, a validated
  `setParameter` / `getParameter` pair, a packed state snapshot, and downsampled
  analysis or record buffers the page reads from `HEAPF32`;
- mirrors in the bridge the clamp ranges that used to live in the native menus,
  because that translation unit is no longer compiled;
- declares its panel in `src/data/controls/`, which the Astro components and the
  runtime wiring both consume.

The exported names are the same across simulations, so one browser bridge and
one rendering pipeline serve all of them. `web/common/ControlBridge.h` carries
the shared validation and downsampling mechanics.

Dear ImGui stays linked, but the rlImGui backend is not built. The pinned native
camera classes read `ImGui::GetIO().WantCaptureMouse` and `WantCaptureKeyboard`
to decide whether input belongs to the scene, so the adapter creates a context
and never starts a frame: every capture flag stays false, which is the behavior a
DOM panel needs. Keyboard shortcuts are additionally gated by
`physicsLabSetKeyboardCaptured`, because Raylib listens for key events on the
document and would otherwise react while a visitor types into a control.

## Consequences

- The canvas shows only the simulation; controls gain labels, focus order, and
  keyboard support from the platform.
- Every `.wasm` module dropped from roughly 500 KiB to under 180 KiB, because the
  Dear ImGui draw, table, and widget code became unreachable.
- The parameter, state, and record enums are a cross-language contract; a Vitest
  guard fails the build when a C++ header and its descriptor drift apart.
- Analysis plots are reimplemented with Canvas 2D and must be kept faithful to
  the native panels by review rather than by shared code. Their range handling is
  relative rather than absolute, so magnitudes as small as gas pressure render
  correctly.
- Panels that gate edits on the simulation state describe those states in data,
  because the rules differ per simulation: ballistics and the pendulum only
  accept configuration while idle, whereas the gas bath also accepts it while
  paused.
