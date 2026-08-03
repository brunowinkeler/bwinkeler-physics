# Performance baseline

## Current Pendulum WebAssembly build

- Date: 2026-08-03
- Emscripten: 6.0.5
- raylib: 5.5
- Dear ImGui: 1.92.6
- Build mode: release, `-O3`, LTO, single-threaded, memory growth enabled
- JavaScript glue: 169,552 bytes uncompressed
- WebAssembly module: 520,645 bytes uncompressed
- Complete static output: 71 files and 1,944,392 bytes uncompressed
- Canvas sizing: one drawing coordinate per CSS pixel
- Dear ImGui font size: 18 pixels
- Initial heap: 64 MiB
- Maximum heap: 256 MiB

The baseline was reproduced from a fresh CMake cache with the pinned native
source revision `7ec34954b274857ec94889e6e59a66c7f5316c8e`.

Both generated files are substantially below the Cloudflare Pages 25 MiB
per-file limit. Compressed transfer measurements and frame-time percentiles must
be collected from the production-like HTTPS preview before the first public
release. Viewports below 1024 × 640 receive guidance without downloading the
runtime bundle.

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
