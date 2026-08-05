# Service Manifest: Physics Lab

## Identity

- Platform architecture version: 1.3
- Service ID: `physics`
- Product name: Physics Lab
- Repository: https://github.com/brunowinkeler/bwinkeler-physics
- Native source repository: https://github.com/brunowinkeler/cpp-physics-simulation
- Owner: Bruno Winkeler
- Criticality: experimental

## Publication

- Production origin: https://physics.bwinkeler.com
- Hosting: Cloudflare Pages Direct Upload
- Pages project: `bwinkeler-physics`
- Output type: static site plus independently loaded WebAssembly bundles
- Build output: `dist`
- VPS dependency: none
- Caddy route: none
- Public API: none
- WebSocket: none

## Runtime resources

- Containers: none
- Database: none
- Redis or queue: none
- Object storage: none initially
- Persistent runtime data: none
- Runtime secrets: none
- Physics execution: visitor browser CPU
- Rendering: visitor browser GPU through WebGL
- Initial WebAssembly heap: 64 MiB
- Maximum WebAssembly heap: 256 MiB
- Threading: single-threaded

## Build contract

- Node.js: 22.14.0 or a compatible newer version
- Package manager: npm
- Emscripten: 6.0.5
- CMake: 3.24 or newer
- Native source: exact Git submodule revision
- Install command: `npm ci`
- WebAssembly build on Windows: `scripts/build-web.ps1`- Validation command: `npm run validate`
- Static output: `dist`

## Content contract

- Simulation registry: `src/data/simulations.ts`
- Canonical theory: `vendor/cpp-physics-simulation/project*/PHYSICS_THEORY.md`
- Published simulations: `ballistics`, `pendulum`, `gas-brownian`
- Runtime bundles: one independently loaded bundle per simulation
- Theory rendering: build-time KaTeX HTML and MathML

## Health and operations

- Liveness: `GET /`
- Simulation routes: `GET /simulations/{ballistics,pendulum,gas-brownian}/`
- Theory routes: `GET /simulations/{ballistics,pendulum,gas-brownian}/theory/`
- Smoke test: browser load, runtime readiness, canvas, controls, and no console errors
- Expected server RAM and CPU: not applicable to static Pages assets
- Client performance target: responsive 60 Hz at default settings on a representative desktop
- Canvas sizing: one Raylib coordinate per canvas CSS pixel
- Browser UI: 18-pixel Dear ImGui font with scaled controls
- Focus mode: Fullscreen API with an in-page enter/exit control and `Escape` support
- Individual asset hard limit: 25 MiB
- Log format: Cloudflare platform logs; browser failures appear in developer tools

## Security and privacy

- Authentication: none
- Registration: disabled
- Cookies: none
- Local storage: sidebar display preference only
- User uploads: none
- Sensitive data: none
- Cross-origin isolation: disabled in the single-threaded v1
- Content Security Policy: same-origin assets, inline Astro scripts, and WebAssembly execution only
- Minimum interactive viewport: 1024 × 640; smaller viewports do not download the runtime

## Deploy

- CI workflow: `.github/workflows/ci.yaml`
- Deployment workflow: `.github/workflows/deploy.yaml`
- Release identifier: web Git SHA plus native submodule SHA
- Database migration: not applicable
- Rollback: redeploy the previous successful exact Git/submodule pair
- Required deployment secrets: `CLOUDFLARE_ACCOUNT_ID`, `CLOUDFLARE_API_TOKEN`

## Dependencies

- External services: GitHub Actions, Cloudflare Pages, Cloudflare DNS
- Infrastructure changes required: Pages custom domain and inventory entry only
- Shared host changes: none
- Portability: `dist` can be served by another static host that provides the correct `.wasm` MIME type

## Open decisions

- Establish measured transfer, startup, memory, and frame-time budgets before the first public release.
- Decide whether immutable runtime assets should use content-hashed names before enabling long-lived browser caching.
- Add Firefox and WebKit automation after the Chromium smoke test is stable.
