#!/bin/bash
set -e

# Create build directory if it doesn't exist
mkdir -p build

# Check if we have clang with wasm support, otherwise try common alternatives
if command -v emcc &> /dev/null; then
    echo "Using Emscripten..."
    emcc \
        -O3 \
        -s STANDALONE_WASM=1 \
        -s EXPORTED_FUNCTIONS='["_process"]' \
        -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
        -s INITIAL_MEMORY=65536 \
        -s MAXIMUM_MEMORY=65536 \
        -s ALLOW_MEMORY_GROWTH=0 \
        -s STACK_SIZE=8192 \
        --no-entry \
        app.cpp -o build/app.wasm
elif command -v /opt/homebrew/opt/llvm/bin/clang &> /dev/null; then
    echo "Using Homebrew LLVM clang..."
    /opt/homebrew/opt/llvm/bin/clang \
        --target=wasm32 \
        -nostdlib \
        -Wl,--no-entry \
        -Wl,--export-all \
        -Wl,--allow-undefined \
        -O3 \
        app.cpp -o build/app.wasm
elif [ -f "$HOME/.wasienv/wasienv" ]; then
    echo "Using wasi-sdk..."
    source "$HOME/.wasienv/wasienv"
    clang++ \
        --target=wasm32 \
        -nostdlib \
        -Wl,--no-entry \
        -Wl,--export-all \
        -Wl,--allow-undefined \
        -O3 \
        app.cpp -o build/app.wasm
else
    echo "Error: No WASM-capable compiler found!"
    echo "Please install one of:"
    echo "  - Emscripten: https://emscripten.org/docs/getting_started/downloads.html"
    echo "  - LLVM via Homebrew: brew install llvm"
    echo "  - wasi-sdk: https://github.com/WebAssembly/wasi-sdk"
    exit 1
fi

# Optimize (if available)
if command -v wasm-opt &> /dev/null; then
    wasm-opt -O3 build/app.wasm -o build/app.wasm
    wasm-strip build/app.wasm
fi

# Convert to WAT for inspection (optional)
if command -v wasm2wat &> /dev/null; then
    wasm2wat --generate-names build/app.wasm -o build/app.wat
fi

echo "Built build/app.wasm successfully!"

cd ..

wasm2c --no-debug-names --module-name="app" --num-outputs=8 app/build/app.wasm -o build/wasm-app.c
