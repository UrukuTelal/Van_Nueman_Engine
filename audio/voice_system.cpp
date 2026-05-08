#include "voice_system.h"
#include "wht.h"
#include "../include/Entity.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <whisper.h>

struct VoiceSystemImpl {
    whisper_context* whisper_ctx;
    char* whisper_model_path;
    bool streaming_active;
    float tts_volume;
    float mic_volume;
    float current_pillars[NUM_PILLARS];
};

VoiceSystem voice_init(const char* model_path, const char* device_name) {
    VoiceSystemImpl* ctx = new VoiceSystemImpl();
    memset(ctx, 0, sizeof(VoiceSystemImpl));

    if (model_path) {
        ctx->whisper_model_path = new char[strlen(model_path) + 1];
        strcpy(ctx->whisper_model_path, model_path);
        ctx->whisper_ctx = whisper_init_from_file(ctx->whisper_model_path);
    } else {
        ctx->whisper_ctx = nullptr;
    }

    ctx->tts_volume = 0.8f;
    ctx->mic_volume = 0.7f;
    ctx->streaming_active = false;

    for (int i = 0; i < NUM_PILLARS; i++) {
        ctx->current_pillars[i] = 0.5f;
    }

    printf("[Voice] Initialized with model: %s\n", model_path ? model_path : "(none)");
    return ctx;
}

void voice_shutdown(VoiceSystem ctx) {
    if (!ctx) return;
    VoiceSystemImpl* impl = (VoiceSystemImpl*)ctx;

    if (impl->whisper_ctx) {
        whisper_free(impl->whisper_ctx);
    }
    if (impl->whisper_model_path) {
        delete[] impl->whisper_model_path;
    }

    delete impl;
    printf("[Voice] Shutdown complete\n");
}

char* voice_recognize_speech(VoiceSystem ctx, int max_len) {
    if (!ctx) return nullptr;
    VoiceSystemImpl* impl = (VoiceSystemImpl*)ctx;

    if (!impl->whisper_ctx) {
        printf("[Voice] Whisper not initialized\n");
        return nullptr;
    }

    printf("[Voice] Recognizing speech (Whisper)...\n");

    char* text_buffer = new char[max_len];
    memset(text_buffer, 0, max_len);

    float audio_data[16000];
    memset(audio_data, 0, sizeof(audio_data));

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;

    if (whisper_full(impl->whisper_ctx, params, audio_data, 16000) != 0) {
        printf("[Voice] Whisper processing failed\n");
        delete[] text_buffer;
        return nullptr;
    }

    int n_segments = whisper_full_n_segments(impl->whisper_ctx);
    int offset = 0;
    for (int i = 0; i < n_segments && offset < max_len - 1; i++) {
        const char* segment_text = whisper_full_get_segment_text(impl->whisper_ctx, i);
        int len = strlen(segment_text);
        if (offset + len < max_len - 1) {
            memcpy(text_buffer + offset, segment_text, len);
            offset += len;
        }
    }

    char* result = new char[offset + 1];
    memcpy(result, text_buffer, offset + 1);
    delete[] text_buffer;

    return result;
}

void voice_speak(VoiceSystem ctx, const char* text) {
    if (!ctx || !text) return;
    VoiceSystemImpl* impl = (VoiceSystemImpl*)ctx;

    printf("[Voice] Synthesizing speech: %s\n", text);

    float warmth = impl->current_pillars[PILLAR_WARMTH];
    float harm = impl->current_pillars[PILLAR_HARM];

    char command[4096];
    snprintf(command, sizeof(command), "piper --volume %.1f --model %s --text \"%s\"",
             impl->tts_volume * (1.0f - harm * 0.5f) * (0.5f + warmth * 0.5f),
             impl->whisper_model_path ? impl->whisper_model_path : "default",
             text);

#ifdef _WIN32
    system(command);
#else
    system(command);
#endif

    printf("[Voice] Speech synthesized\n");
}

void voice_encode_wht(VoiceSystem ctx, float* out_wht, int wht_n) {
    if (!ctx || !out_wht) return;
    VoiceSystemImpl* impl = (VoiceSystemImpl*)ctx;

    for (int i = 0; i < wht_n && i < NUM_PILLARS; i++) {
        out_wht[i] = impl->current_pillars[i];
    }
    for (int i = NUM_PILLARS; i < wht_n; i++) {
        out_wht[i] = 0.0f;
    }
}

void voice_set_pillars(VoiceSystem ctx, const float* pillars, int count) {
    if (!ctx || !pillars) return;
    VoiceSystemImpl* impl = (VoiceSystemImpl*)ctx;

    int copy_count = count < NUM_PILLARS ? count : NUM_PILLARS;
    for (int i = 0; i < copy_count; i++) {
        impl->current_pillars[i] = pillars[i];
    }
}
