#!/bin/bash
set -e

# Create build directory if it doesn't exist
mkdir -p build

cp BigMuff.wasm build/

# Optimize (if available)
if command -v wasm-opt &> /dev/null; then
    wasm-opt -O3 build/BigMuff.wasm -o build/BigMuff.wasm
    wasm-strip build/BigMuff.wasm
fi

# Convert to WAT for inspection (optional)
if command -v wasm2wat &> /dev/null; then
    wasm2wat --generate-names build/BigMuff.wasm -o build/BigMuff.wat
fi

echo "Built build/BigMuff.wasm successfully!"

cd ..

wasm2c --no-debug-names --module-name="muff" --num-outputs=8 wasm-modules/build/BigMuff.wasm -o build/wasm-app.c