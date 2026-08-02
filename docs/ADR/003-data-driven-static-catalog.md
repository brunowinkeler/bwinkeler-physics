# ADR 003: Use a data-driven static catalog

- Status: accepted
- Date: 2026-08-02

## Context

Physics Lab is expected to grow from three current C++ projects to many
simulations and potentially multiple directly supported Emscripten engines.
Navigation must remain usable and content pages must remain lightweight.

## Decision

Use Astro static output and a validated simulation registry to generate the home
catalog, collapsible categorized sidebar, execution routes, and theory routes.
Build theory from canonical Markdown with KaTeX. Keep one independently loaded
runtime bundle per simulation.

Astro ships only small progressive-enhancement scripts for navigation. It does
not run as a production server and no client UI framework is required.

## Consequences

- Catalog and theory pages do not download simulation runtimes.
- Adding a simulation does not require editing navigation templates.
- Routes are shareable, indexable static documents.
- KaTeX produces equations and MathML at build time without client math code.
- Each simulation can pin a different directly supported Emscripten stack while
  retaining independent cache and rollback behavior.
