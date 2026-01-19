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

// Implement expf for WASM module
f32 w2c_env_0x5Fexpf(struct w2c_env* env, f32 x)
{
    (void)env; // Unused parameter
    return std::expf(x);
}

// Implement fmodf for WASM module
f32 w2c_env_0x5Ffmodf(struct w2c_env* env, f32 x, f32 y)
{
    (void)env; // Unused parameter
    return std::fmodf(x, y);
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

// Implement roundf for WASM module
f32 w2c_env_0x5Froundf(struct w2c_env* env, f32 x)
{
    (void)env; // Unused parameter
    return std::roundf(x);
}
