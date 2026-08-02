# Agent instructions

Read these files before changing this repository:

1. `docs/ARCHITECTURE.md`
2. `docs/SERVICE_MANIFEST.md`
3. `docs/OPERATIONS.md`
4. the relevant ADRs in `docs/ADR/`

## Language

All repository content must be written in English, including code, UI text,
documentation, metadata, comments, commits, and domains.

## Repository boundaries

- This repository owns the Astro site, simulation catalog, browser adapters,
  Emscripten build, browser tests, and Cloudflare Pages deployment.
- `vendor/cpp-physics-simulation` is a pinned Git submodule and the canonical
  source for physics, rendering, Dear ImGui UI, native tests, and theory.
- Do not edit files inside the submodule from this repository. Land changes in
  the source repository first, then update the submodule pointer.
- Keep browser-specific policy outside the native source repository.
- Do not commit generated `build/`, `dist/`, or `public/runtime/` content.

## Architecture

- Keep Astro output fully static. Do not add SSR, Pages Functions, or a backend
  without an approved ADR and concrete requirement.
- Keep one independently loaded WebAssembly bundle per simulation.
- Generate navigation and routes from `src/data/simulations.ts`; do not add
  simulation links directly to sidebar templates.
- Every published simulation must have a canonical theory document and a
  browser runtime.
- Prefer libraries with direct, maintained Emscripten support. Qt is out of
  scope.
- Do not add pthreads, COOP/COEP, object storage, or server compute without
  profiling evidence and an ADR.

## Validation

Run `npm run validate` after site changes. When C++ sources, dependency pins, or
browser adapters change, also run `scripts/build-web.ps1` and exercise the
simulation in a browser. Preserve the existing native test baseline in the
source repository.
