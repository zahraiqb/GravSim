#!/usr/bin/env bash
# Build the C++ core to WebAssembly and place it where the web app expects it.
# Requires the Emscripten SDK (https://emscripten.org/docs/getting_started/downloads.html), activated.
set -euo pipefail
cd "$(dirname "$0")/.."
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm -j
mkdir -p web/public/wasm
cp build-wasm/gravsim.js build-wasm/gravsim.wasm web/public/wasm/
echo "WASM written to web/public/wasm/"
