#ifndef VOICE_SYSTEM_H
#define VOICE_SYSTEM_H

// Compile-time guard: define VOICE_ENABLED to enable whisper.cpp integration.
// When disabled, all functions return null/zero stubs.
#ifndef VOICE_ENABLED
#define VOICE_ENABLED 0
#endif

#include "../include/Entity.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* VoiceSystem;

#if VOICE_ENABLED

VoiceSystem voice_init(const char* model_path, const char* device_name);
void voice_shutdown(VoiceSystem ctx);

char* voice_recognize_speech(VoiceSystem ctx, int max_len);
void voice_speak(VoiceSystem ctx, const char* text);

void voice_encode_wht(VoiceSystem ctx, float* out_wht, int wht_n);
void voice_set_pillars(VoiceSystem ctx, const float* pillars, int count);

#else

static inline VoiceSystem voice_init(const char* model_path, const char* device_name) { (void)model_path; (void)device_name; return 0; }
static inline void voice_shutdown(VoiceSystem ctx) { (void)ctx; }
static inline char* voice_recognize_speech(VoiceSystem ctx, int max_len) { (void)ctx; (void)max_len; return 0; }
static inline void voice_speak(VoiceSystem ctx, const char* text) { (void)ctx; (void)text; }
static inline void voice_encode_wht(VoiceSystem ctx, float* out_wht, int wht_n) { (void)ctx; (void)out_wht; (void)wht_n; }
static inline void voice_set_pillars(VoiceSystem ctx, const float* pillars, int count) { (void)ctx; (void)pillars; (void)count; }

#endif

#ifdef __cplusplus
}
#endif

#endif // VOICE_SYSTEM_H
