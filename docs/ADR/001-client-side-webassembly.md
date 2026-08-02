# ADR 001: Run physics in client-side WebAssembly

- Status: accepted
- Date: 2026-08-02

## Context

The existing simulations are C++20 desktop applications using Raylib and Dear
ImGui. Visitors should run them from a URL without installing native binaries.
The shared VPS already serves Listly and should not receive avoidable compute
load.

## Decision

Compile each simulation to an independent WebAssembly bundle with Emscripten.
Run numerical integration and rendering in the visitor's browser. Deploy the
Astro site and runtime bundles as static Cloudflare Pages assets.

The initial build is single-threaded, uses a returning browser animation loop,
and does not use Asyncify. Threads, Workers, SIMD, and server-side compute are
introduced only after profiling demonstrates a requirement.

## Consequences

- The VPS receives no Physics Lab runtime load.
- Distribution and rollback are static and inexpensive.
- Performance depends on visitor hardware and browser behavior.
- Browser sandbox, memory, background-tab, and WebGL constraints apply.
- Native remains the performance and compatibility ceiling.
- Heavy simulations need explicit work budgets and adaptive behavior.
