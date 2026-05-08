#ifndef VOICE_SYSTEM_H
#define VOICE_SYSTEM_H

#include "../include/Entity.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* VoiceSystem;

VoiceSystem voice_init(const char* model_path, const char* device_name);
void voice_shutdown(VoiceSystem ctx);

char* voice_recognize_speech(VoiceSystem ctx, int max_len);
void voice_speak(VoiceSystem ctx, const char* text);

void voice_encode_wht(VoiceSystem ctx, float* out_wht, int wht_n);
void voice_set_pillars(VoiceSystem ctx, const float* pillars, int count);

#ifdef __cplusplus
}
#endif

#endif
