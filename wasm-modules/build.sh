#!/bin/bash
set -e

# Create build directory if it doesn't exist
mkdir -p build

cp springreverb.wasm build/

# Optimize (if available)
if command -v wasm-opt &> /dev/null; then
    wasm-opt -O3 build/springreverb.wasm -o build/springreverb.wasm
    wasm-strip build/springreverb.wasm
fi

# Convert to WAT for inspection (optional)
if command -v wasm2wat &> /dev/null; then
    wasm2wat --generate-names build/springreverb.wasm -o build/springreverb.wat
fi

echo "Built build/springreverb.wasm successfully!"

cd ..

wasm2c --no-debug-names --module-name="muff" --num-outputs=8 wasm-modules/build/springreverb.wasm -o build/wasm-app.c