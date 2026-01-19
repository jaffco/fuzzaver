#!/bin/bash
set -e

# Create build directory if it doesn't exist
mkdir -p build

cp DualPitchShifter.wasm build/

# Optimize (if available)
if command -v wasm-opt &> /dev/null; then
    wasm-opt -O3 build/DualPitchShifter.wasm -o build/DualPitchShifter.wasm
    wasm-strip build/DualPitchShifter.wasm
fi

# Convert to WAT for inspection (optional)
if command -v wasm2wat &> /dev/null; then
    wasm2wat --generate-names build/DualPitchShifter.wasm -o build/DualPitchShifter.wat
fi

echo "Built build/DualPitchShifter.wasm successfully!"

cd ..

wasm2c --no-debug-names --module-name="muff" --num-outputs=8 wasm-modules/build/DualPitchShifter.wasm -o build/wasm-app.c