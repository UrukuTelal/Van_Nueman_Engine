// audio_system.cpp - Audio system implementation with miniaudio + WHT optimization

#include "audio_system.h"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "wht.h"
#include "../include/Entity.h"
#include "../scale/SemanticProjection.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>

struct AudioSystemImpl {
    ma_engine engine;
    ma_sound music_sound;
    bool music_playing;
    float master_volume;
    float music_volume;
    float sfx_volume;
    std::vector<ma_sound*> active_sfx;
};

AudioSystem audio_init(float master_volume) {
    AudioSystemImpl* ctx = new AudioSystemImpl();
    memset(ctx, 0, sizeof(AudioSystemImpl));
    
    if (ma_engine_init(nullptr, &ctx->engine) != MA_SUCCESS) {
        delete ctx;
        return nullptr;
    }
    
    ctx->music_playing = false;
    ctx->master_volume = master_volume;
    ctx->music_volume = 0.5f;
    ctx->sfx_volume = 0.7f;
    
    ma_engine_set_volume(&ctx->engine, ctx->master_volume);
    
    return ctx;
}

void audio_shutdown(AudioSystem ctx) {
    if (!ctx) return;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    if (impl->music_playing) {
        ma_sound_uninit(&impl->music_sound);
    }
    for (auto sfx : impl->active_sfx) {
        ma_sound_uninit(sfx);
        delete sfx;
    }
    impl->active_sfx.clear();
    ma_engine_uninit(&impl->engine);
    delete impl;
}

int32_t audio_play_music(AudioSystem ctx, const char* file_path, bool loop) {
    if (!ctx) return -1;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    
    if (impl->music_playing) {
        ma_sound_uninit(&impl->music_sound);
        impl->music_playing = false;
    }
    
    if (ma_sound_init_from_file(&impl->engine, file_path, 0, nullptr, nullptr, &impl->music_sound) != MA_SUCCESS) {
        return -1;
    }
    
    ma_sound_set_volume(&impl->music_sound, impl->music_volume);
    ma_sound_set_looping(&impl->music_sound, loop);
    ma_sound_start(&impl->music_sound);
    impl->music_playing = true;
    
    return 0;
}

int32_t audio_play_sfx(AudioSystem ctx, const char* file_path, float volume) {
    if (!ctx) return -1;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    
    ma_sound* sfx = new ma_sound();
    if (ma_sound_init_from_file(&impl->engine, file_path, 0, nullptr, nullptr, sfx) != MA_SUCCESS) {
        delete sfx;
        return -1;
    }
    
    ma_sound_set_volume(sfx, volume * impl->sfx_volume);
    ma_sound_start(sfx);
    impl->active_sfx.push_back(sfx);
    return 0;
}

void audio_stop_music(AudioSystem ctx) {
    if (!ctx) return;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    if (impl->music_playing) {
        ma_sound_stop(&impl->music_sound);
        ma_sound_uninit(&impl->music_sound);
        impl->music_playing = false;
    }
}

void audio_set_master_volume(AudioSystem ctx, float volume) {
    if (!ctx) return;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    impl->master_volume = volume;
    ma_engine_set_volume(&impl->engine, volume);
}

void audio_set_music_volume(AudioSystem ctx, float volume) {
    if (!ctx) return;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    impl->music_volume = volume;
    if (impl->music_playing) {
        ma_sound_set_volume(&impl->music_sound, volume);
    }
}

void audio_set_sfx_volume(AudioSystem ctx, float volume) {
    if (!ctx) return;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    impl->sfx_volume = volume;
}

// Voice system integration (NEW)
static VoiceSystemHandle voice_ctx = nullptr;

struct VoiceSystem {
    int placeholder;
};

VoiceSystemHandle audio_init_voice(const char* whisper_model_path) {
    printf("[Audio] Initializing voice system with model: %s\n", whisper_model_path ? whisper_model_path : "(none)");
    
    // In real implementation:
     // 1. Initialize Whisper.cpp for STT (local, ~1GB model)
     // 2. Initialize Piper TTS for TTS (local, lightweight)
     
    VoiceSystemHandle ctx = (VoiceSystemHandle)malloc(sizeof(struct VoiceSystem));
    
    voice_ctx = ctx;
    return ctx;
}

void audio_shutdown_voice(VoiceSystemHandle voice_ctx) {
    if (!voice_ctx) return;
    printf("[Audio] Shutting down voice system\n");
    free(voice_ctx);
    voice_ctx = nullptr;
}

char* audio_recognize_speech(VoiceSystemHandle voice_ctx, int max_len) {
    if (!voice_ctx) return nullptr;
    printf("[Audio] Recognizing speech (Whisper)...\n");
    
    // Placeholder: In real implementation, capture from microphone
    // and call Whisper.cpp for recognition
    const char* dummy = "Voice recognized (Whisper placeholder)";
    char* result = (char*)malloc(strlen(dummy) + 1);
    strcpy(result, dummy);
    return result;
}

void audio_speak(VoiceSystemHandle voice_ctx, const char* text, float volume) {
    if (!voice_ctx || !text) return;
    printf("[Audio] Speaking (Piper TTS): '%s' (volume: %.1f)\n", text, volume);
    
    // Placeholder: In real implementation, call espeak-ng or Piper
    // system("espeak-ng \"" + text + "\" --volume=" + to_string(volume));
}

void audio_stop_speaking(VoiceSystemHandle voice_ctx) {
    printf("[Audio] Stopped speaking\n");
}

void audio_apply_voice_pillar_effects(VoiceSystemHandle voice_ctx, const float pillars[NUM_PILLARS]) {
    if (!voice_ctx || !pillars) return;
    
    // Build PillarStateVector from raw array and project to cognitive layer
    PillarStateVector psv;
    for (int i = 0; i < NUM_PILLARS; i++) psv.pillars[i] = vn::fp20_t(pillars[i]);
    CognitiveProjection cp = CognitiveProjection::project(psv);
    
    // High cognitive load (Harm projection): robotic/glitchy voice
    if (cp.cognitive_load > 0.7f) {
        printf("[Voice] Cognitive load > 0.7 - Robotic/glitchy voice\n");
        return;
    }
    
    // High comfort (Warmth projection): smoother/warmer voice
    if (cp.comfort > 0.7f) {
        printf("[Voice] Comfort > 0.7 - Smooth/calm voice\n");
        audio_speak(voice_ctx, "Voice is now smoother", cp.comfort * 0.8f);
    }
    
    // Effective outward awareness modulated by imaginativeness (Distortion projection)
    float outward = 1.0f - cp.focus_inward;
    float effective_awareness = outward * (1.0f - cp.imaginativeness);
    printf("[Voice] Effective Awareness: %.2f (outward=%.2f, imaginativeness=%.2f)\n", 
           effective_awareness, outward, cp.imaginativeness);
}

// Pillar-driven audio effects — uses CognitiveProjection for emotional mapping
void audio_apply_pillar_effects(AudioSystem ctx, const float pillars[NUM_PILLARS]) {
    if (!ctx || !pillars) return;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    
    PillarStateVector psv;
    for (int i = 0; i < NUM_PILLARS; i++) psv.pillars[i] = vn::fp20_t(pillars[i]);
    CognitiveProjection cp = CognitiveProjection::project(psv);
    
    // High cognitive load (Harm projection): tense music, reduced volume
    if (cp.cognitive_load > 0.7f) {
        ma_engine_set_volume(&impl->engine, impl->master_volume * (1.0f - cp.cognitive_load * 0.3f));
    }
    
    // High comfort (Warmth projection): calm music
    if (cp.comfort > 0.7f) {
        ma_engine_set_volume(&impl->engine, impl->master_volume * (0.8f + cp.comfort * 0.2f));
    }
    
    // Agency (Force + Willpower projection): tempo/intensity
    // Would control tempo/pitch in real implementation
}

void audio_update(AudioSystem ctx, float dt) {
    if (!ctx) return;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    (void)dt;
    for (auto it = impl->active_sfx.begin(); it != impl->active_sfx.end(); ) {
        if (!ma_sound_is_playing(*it)) {
            ma_sound_uninit(*it);
            delete *it;
            it = impl->active_sfx.erase(it);
        } else {
            ++it;
        }
    }
}

// WHT-based audio processing for 32-channel mixing
static float s_wht_weights[WHT_N][WHT_N];
static bool s_wht_initialized = false;

// Initialize WHT weights from standard weights matrix
void audio_init_wht_weights(float weights[WHT_N][WHT_N]) {
    precompute_wht_weights(weights, s_wht_weights);
    s_wht_initialized = true;
}

// Process audio block using WHT (O(n log n) instead of O(n²))
void audio_process_wht_block(float* input_block, float* output_block, int block_size) {
    if (block_size != WHT_N) {
        for (int i = 0; i < block_size && i < WHT_N; i++) {
            output_block[i] = input_block[i];
        }
        return;
    }
    
    if (!s_wht_initialized) {
        for (int i = 0; i < WHT_N; i++) {
            output_block[i] = input_block[i];
        }
        return;
    }
    
    // WHT convolution: y = ifwht( fwht(x) * fwht(h) ) / N
    // Transform input to Hadamard domain
    float x_wht[WHT_N];
    for (int i = 0; i < WHT_N; i++) {
        x_wht[i] = input_block[i];
    }
    fwht(x_wht, WHT_LOG2_N);
    
    // For each output channel: element-wise multiply in Hadamard domain, invert, scale
    for (int out = 0; out < WHT_N; out++) {
        float y_wht[WHT_N];
        for (int i = 0; i < WHT_N; i++) {
            y_wht[i] = x_wht[i] * s_wht_weights[out][i];
        }
        ifwht(y_wht, WHT_LOG2_N);
        output_block[out] = y_wht[0] / WHT_N;
    }
}
