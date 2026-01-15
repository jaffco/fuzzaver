#ifndef audio_wasm_api_h
#define audio_wasm_api_h

#ifdef __cplusplus
#define WASM_EXTERN_C  extern "C"
#else
#define WASM_EXTERN_C
#endif

#define WASM_EXPORT                   WASM_EXTERN_C __attribute__((used)) __attribute__((visibility ("default")))
#define WASM_EXPORT_AS(NAME)          WASM_EXPORT __attribute__((export_name(NAME)))

#ifdef __cplusplus
extern "C" {
#endif

// Export audio processing function
WASM_EXPORT_AS("process") float process(float input);

#ifdef __cplusplus
}
#endif

#endif // audio_wasm_api_h
