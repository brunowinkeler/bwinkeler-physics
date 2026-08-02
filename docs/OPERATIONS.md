# Physics Lab operations

## Local setup

1. Initialize the pinned source revision:
   `git submodule update --init --recursive`.
2. Install JavaScript dependencies with `npm ci`.
3. Start Docker Desktop.
4. Build the Pendulum runtime with `scripts/build-web.ps1`.
5. Start the site with `npm run dev`.

The site intentionally builds without generated runtime artifacts and displays a
clear fallback. A release must include the runtime.

## Validation

Run `npm run validate` for formatting, lint, Astro/TypeScript checks, registry
tests, content checks, and a production build.

After any browser-adapter, source-selection, compiler, or C++ revision change:

1. rebuild WebAssembly from a clean Emscripten build directory;
2. confirm generated `.js` and `.wasm` files remain under 25 MiB each;
3. open the Pendulum route in supported browsers;
4. exercise start, pause, reset, simple/double mode, camera, controls, and plots;
5. inspect the browser console for errors and WebGL warnings;
6. verify the theory route does not request runtime files.

## Source update

The submodule never follows a moving branch in production.

1. Land and validate changes in `cpp-physics-simulation`.
2. Update `vendor/cpp-physics-simulation` to that exact commit.
3. Review the submodule SHA in the web pull request.
4. Run native tests, the Emscripten build, and browser tests.
5. Deploy the exact pair of revisions together.

Do not edit the submodule working tree from this repository.

## Cloudflare Pages setup

Create a Direct Upload project named `bwinkeler-physics`. GitHub Actions deploys
the prebuilt `dist` directory with Wrangler. Store these values in a protected
GitHub production environment:

- `CLOUDFLARE_ACCOUNT_ID`;
- `CLOUDFLARE_API_TOKEN`, scoped to Cloudflare Pages edit access.

Associate `physics.bwinkeler.com` through the Pages **Custom domains** flow. Do
not manually create a VPS A/AAAA route, Caddy site, application container, or
database.

## Production verification

- `https://physics.bwinkeler.com/` returns the catalog.
- The Pendulum runtime initializes without console errors.
- The theory route renders equations and MathML.
- `.wasm` responses use `Content-Type: application/wasm`.
- Brotli or gzip compression is available.
- Security headers from `public/_headers` are present.
- Preview deployments return `X-Robots-Tag: noindex`.
- An external uptime monitor checks the root route.

## Rollback

Every release records both repository SHAs. To roll back:

1. select the previous known-good web commit and submodule pointer;
2. rebuild rather than reusing an unverified local artifact;
3. deploy the resulting `dist` directory through the protected workflow;
4. repeat the catalog, runtime, theory, MIME, and browser smoke checks.

There is no database or persistent application state to migrate or restore.

## Scaling triggers

Review the hosting design if any artifact approaches 25 MiB, the project
approaches 20,000 files, CI duration becomes operationally excessive, or a
representative desktop cannot sustain the supported simulation settings.

The preferred static scaling path is selective builds, stronger caching, and R2
for oversized immutable runtime assets. The shared Listly VPS is not the default
fallback. Server-side simulation requires a separate API, abuse controls,
observability, cost model, and compute capacity decision.
