# Performance baseline

## Current WebAssembly builds

- Date: 2026-08-05
- Emscripten: 6.0.5
- raylib: 5.5
- Dear ImGui: 1.92.6
- Build mode: release, `-O3`, LTO, single-threaded, memory growth enabled
- Ballistics: 170,610 bytes of glue and 177,613 bytes of WebAssembly
- Pendulum: 170,461 bytes of glue and 178,368 bytes of WebAssembly
- Gas and Brownian motion: 170,253 bytes of glue and 172,535 bytes of WebAssembly
- Complete static output: 80 files and 2,520,310 bytes uncompressed
- Canvas sizing: one drawing coordinate per CSS pixel
- Analysis plots: at most 512 points per plot per frame, downsampled in C++
- Launch history records: at most 64 archived launches published to the page
- Initial heap: 64 MiB
- Maximum heap: 256 MiB

Every simulation moved its controls to the DOM under ADR 004, which made the
Dear ImGui draw, table, and widget code unreachable and cut each WebAssembly
module by about 66 percent.

The baseline was reproduced from a fresh CMake cache with the pinned native
source revision `3d21294bc54449546388a081f4493265c0507250`.

Every generated file is substantially below the Cloudflare Pages 25 MiB
per-file limit, and a visitor downloads only the opened laboratory. Compressed
transfer measurements and frame-time percentiles must be collected from the
production-like HTTPS preview before the first public release. Viewports below
1024 × 640 receive guidance without downloading the runtime bundle.

## Required release measurements

Record the following for latest Chrome, Edge, Firefox, and Safari on identified
hardware:

- compressed JavaScript and WebAssembly transfer sizes;
- time to first rendered simulation frame;
- Wasm heap and total page memory;
- median, p95, and p99 frame time;
- long tasks;
- default and maximum supported time scale/physics-step combinations;
- behavior after hiding and restoring the tab.

Do not convert one machine's result into a universal native-to-WebAssembly
ratio. Compare the same scenario, compiler optimization level, and render size,
and retain the native executable as the performance ceiling.
