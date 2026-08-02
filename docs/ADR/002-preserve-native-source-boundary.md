# ADR 002: Preserve the native source repository boundary

- Status: accepted
- Date: 2026-08-02

## Context

`cpp-physics-simulation` exists to develop, test, and run native C++ physics
applications. Making its build graph and application lifecycle web-aware would
mix product responsibilities. Project 2 already exposes enough public session,
renderer, overlay, and UI APIs for a separate entry point.

## Decision

Consume `cpp-physics-simulation` as an exact Git submodule revision. Keep the
Emscripten CMake graph, browser entry point, frame-work policy, and deployment in
`bwinkeler-physics`.

The web target compiles selected production sources and excludes native
`main.cpp` and `PendulumApplication.cpp`. Changes to the source repository for
v1 are limited to license, third-party notices, English theory, and a link to the
web consumer.

## Consequences

- Native code, build presets, and behavior remain unchanged.
- Theory and physics have one canonical source.
- The web repository duplicates a small source manifest and orchestration layer.
- CI must detect source-list and dependency-pin drift.
- Source changes must land first; the web repository then updates its submodule
  pointer and deploys the exact pair atomically.
