#!/bin/bash
set -e

# Create build directory if it doesn't exist
mkdir -p build

cp GreyHole.wasm build/

# Optimize (if available)
if command -v wasm-opt &> /dev/null; then
    wasm-opt -O3 build/GreyHole.wasm -o build/GreyHole.wasm
    wasm-strip build/GreyHole.wasm
fi

# Convert to WAT for inspection (optional)
if command -v wasm2wat &> /dev/null; then
    wasm2wat --generate-names build/GreyHole.wasm -o build/GreyHole.wat
fi

echo "Built build/GreyHole.wasm successfully!"

cd ..

wasm2c --no-debug-names --module-name="muff" --num-outputs=8 wasm-modules/build/GreyHole.wasm -o build/wasm-app.c