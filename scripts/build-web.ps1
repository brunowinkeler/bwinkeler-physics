$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$emscriptenImage = 'emscripten/emsdk:6.0.5'

if (-not (Test-Path (Join-Path $repositoryRoot 'vendor/cpp-physics-simulation/project2_pendulum'))) {
    throw 'Physics source submodule is missing. Run: git submodule update --init --recursive'
}

$mount = "${repositoryRoot}:/src"

docker run --rm --volume $mount --workdir /src $emscriptenImage `
    emcmake cmake --fresh -S . -B build/emscripten -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) {
    throw "Emscripten CMake configuration failed with exit code $LASTEXITCODE."
}

docker run --rm --volume $mount --workdir /src $emscriptenImage `
    cmake --build build/emscripten --parallel
if ($LASTEXITCODE -ne 0) {
    throw "Emscripten build failed with exit code $LASTEXITCODE."
}
