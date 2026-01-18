#include "wasm-app.h"
#include <cmath>

/**
 * Implementation of WASM environment functions
 * These are imported by the WebAssembly module for math operations
 */

// Implement powf for WASM module
f32 w2c_env_0x5Fpowf(struct w2c_env* env, f32 base, f32 exponent)
{
    (void)env; // Unused parameter
    return std::powf(base, exponent);
}

// Implement tanf for WASM module
f32 w2c_env_0x5Ftanf(struct w2c_env* env, f32 angle)
{
    (void)env; // Unused parameter
    return std::tanf(angle);
}

// Implement cosf for WASM module
f32 w2c_env_0x5Fcosf(struct w2c_env* env, f32 angle)
{
    (void)env; // Unused parameter
    return std::cosf(angle);
}

// Implement sinf for WASM module
f32 w2c_env_0x5Fsinf(struct w2c_env* env, f32 angle)
{
    (void)env; // Unused parameter
    return std::sinf(angle);
}
