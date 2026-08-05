# Performance baseline

## Current WebAssembly builds

- Date: 2026-08-05
- Emscripten: 6.0.5
- raylib: 5.5
- Dear ImGui: 1.92.6
- Build mode: release, `-O3`, LTO, single-threaded, memory growth enabled
- Ballistics: 169,458 bytes of glue and 524,145 bytes of WebAssembly
- Pendulum: 169,456 bytes of glue and 520,635 bytes of WebAssembly
- Gas and Brownian motion: 169,460 bytes of glue and 515,126 bytes of WebAssembly
- Complete static output: 79 files and 3,498,491 bytes uncompressed
- Canvas sizing: one drawing coordinate per CSS pixel
- Dear ImGui font size: 18 pixels
- Initial heap: 64 MiB
- Maximum heap: 256 MiB

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
