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
#include <cmath>
#include <new>
#include <vector>

static constexpr size_t MAX_ACTIVE_SFX = 64;

#define WHT_AUDIO_BLOCK_SIZE WHT_N  // 32
#define WHT_AUDIO_SINE_COUNT WHT_N  // one oscillator per virtual channel

// Per-channel sine oscillator frequencies (Hz) for WHT audio source
static const float s_wht_sine_freqs[WHT_AUDIO_SINE_COUNT] = {
    110.0f, 146.83f, 185.0f, 220.0f, 261.63f, 311.13f, 349.23f, 392.0f,    // C2–G4 range
    440.0f, 523.25f, 587.33f, 659.25f, 698.46f, 783.99f, 880.0f, 1046.5f,  // A4–C6
    165.0f, 196.0f, 247.0f, 294.0f, 370.0f, 494.0f, 554.0f, 622.0f,       // microtonal fill
    740.0f, 831.0f, 932.0f, 1109.0f, 1245.0f, 1397.0f, 1568.0f, 1760.0f   // upper extension
};

// Forward declaration
struct AudioSystemImpl;

// Custom ma_data_source for real-time WHT-processed audio
typedef struct WHTAudioSource {
    ma_data_source_base base;
    float phases[WHT_AUDIO_SINE_COUNT];
    float weights[WHT_N][WHT_N];       // Current time-domain mixing matrix (updated by pillar state)
    float wht_weights[WHT_N][WHT_N];   // Pre-transformed WHT-domain version
    bool weights_dirty;
    float gain;
    ma_uint32 sample_rate;
    struct AudioSystemImpl* owner;     // Back-pointer to parent for shared-weight sync
} WHTAudioSource;

static ma_result wht_ds_on_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
static ma_result wht_ds_on_seek(ma_data_source* pDataSource, ma_uint64 frameIndex);
static ma_result wht_ds_on_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
static ma_result wht_ds_on_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor);
static ma_result wht_ds_on_get_length(ma_data_source* pDataSource, ma_uint64* pLength);
static ma_result wht_ds_on_set_looping(ma_data_source* pDataSource, ma_bool32 isLooping);

static ma_data_source_vtable g_wht_ds_vtable = {
    wht_ds_on_read,
    wht_ds_on_seek,
    wht_ds_on_get_data_format,
    wht_ds_on_get_cursor,
    wht_ds_on_get_length,
    wht_ds_on_set_looping,
    0  // flags
};

struct AudioSystemImpl {
    ma_engine engine;
    ma_sound music_sound;
    bool music_playing;
    float master_volume;
    float music_volume;
    float sfx_volume;
    std::vector<ma_sound*> active_sfx;
    // WHT audio processor
    WHTAudioSource wht_source;
    ma_sound wht_sound;
    bool wht_active;
};

AudioSystem audio_init(float master_volume) {
    AudioSystemImpl* ctx = new AudioSystemImpl{};
    
    if (ma_engine_init(nullptr, &ctx->engine) != MA_SUCCESS) {
        delete ctx;
        return nullptr;
    }
    
    ctx->music_playing = false;
    ctx->master_volume = master_volume;
    ctx->music_volume = 0.5f;
    ctx->sfx_volume = 0.7f;
    ctx->active_sfx.reserve(MAX_ACTIVE_SFX);
    
    ma_engine_set_volume(&ctx->engine, ctx->master_volume);

    // Initialize WHT audio source (inactive by default)
    memset(&ctx->wht_source, 0, sizeof(ctx->wht_source));
    ctx->wht_source.gain = 0.05f;
    ctx->wht_source.weights_dirty = true;
    ctx->wht_source.owner = ctx;
    ctx->wht_active = false;
    for (int i = 0; i < WHT_N; i++) {
        ctx->wht_source.phases[i] = 0.0f;
        // Default: identity mixing (both time-domain and pre-transformed)
        for (int j = 0; j < WHT_N; j++) {
            ctx->wht_source.weights[i][j] = (i == j) ? 1.0f : 0.0f;
            ctx->wht_source.wht_weights[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }

    return ctx;
}

void audio_shutdown(AudioSystem ctx) {
    if (!ctx) return;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    if (impl->music_playing) {
        ma_sound_uninit(&impl->music_sound);
    }
    if (impl->wht_active) {
        ma_sound_uninit(&impl->wht_sound);
        impl->wht_active = false;
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
    if (ma_sound_start(&impl->music_sound) != MA_SUCCESS) {
        ma_sound_uninit(&impl->music_sound);
        return -1;
    }
    impl->music_playing = true;
    
    return 0;
}

int32_t audio_play_sfx(AudioSystem ctx, const char* file_path, float volume) {
    if (!ctx) return -1;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    
    if (impl->active_sfx.size() >= MAX_ACTIVE_SFX) {
        for (auto it = impl->active_sfx.begin(); it != impl->active_sfx.end(); ) {
            if (ma_sound_at_end(*it)) {
                ma_sound_uninit(*it);
                delete *it;
                it = impl->active_sfx.erase(it);
            } else {
                ++it;
            }
        }
        if (impl->active_sfx.size() >= MAX_ACTIVE_SFX) {
            return -1;
        }
    }
    
    ma_sound* sfx = new (std::nothrow) ma_sound();
    if (!sfx) return -1;
    
    if (ma_sound_init_from_file(&impl->engine, file_path, 0, nullptr, nullptr, sfx) != MA_SUCCESS) {
        delete sfx;
        return -1;
    }
    
    ma_sound_set_volume(sfx, volume * impl->sfx_volume);
    if (ma_sound_start(sfx) != MA_SUCCESS) {
        ma_sound_uninit(sfx);
        delete sfx;
        return -1;
    }
    impl->active_sfx.push_back(sfx);

    // Opportunistic cleanup: sweep finished sounds after each play
    audio_update(ctx, 0.0f);

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

// Voice system integration (stub — real impl is in voice_system.cpp)
static VoiceSystemHandle voice_ctx = nullptr;

// Opaque stub placeholder (will be replaced by real VoiceSystemImpl)
#define STUB_VOICE_SYSTEM_SIZE 1024
struct VoiceSystem {
    char _opaque[STUB_VOICE_SYSTEM_SIZE];
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
    if (!result) return nullptr;
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

void audio_apply_voice_pillar_effects(VoiceSystemHandle voice_ctx, const float pillars[NumPillars]) {
    if (!voice_ctx || !pillars) return;
    
    // Build PillarStateVector from raw array and project to cognitive layer
    PillarStateVector psv;
    for (int i = 0; i < NumPillars; i++) psv.pillars[i] = vn::fp20_t(pillars[i]);
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

// Generate a WHT mixing matrix from a cognitive projection.
// The diagonal maps pillar i → channel i; off-diagonal terms create cross-talk.
// - cognitive_load  → increases off-diagonal mixing (spectral diffusion/dissonance)
// - comfort         → sharpens diagonal dominance (pure tones)
// - agency          → boosts high-frequency gain
static void generate_mix_weights_from_cognitive(float weights[WHT_N][WHT_N], const CognitiveProjection& cp) {
    // Start with identity
    for (int r = 0; r < WHT_N; r++)
        for (int c = 0; c < WHT_N; c++)
            weights[r][c] = (r == c) ? 1.0f : 0.0f;

    // Cognitive load > 0.7: spread energy to adjacent channels (diffusion)
    float load = (cp.cognitive_load > 0.7f) ? (cp.cognitive_load - 0.7f) * 2.0f : 0.0f;
    if (load > 0.0f) {
        for (int r = 0; r < WHT_N; r++) {
            weights[r][r] = 1.0f - load * 0.3f;
            weights[r][(r + 1) % WHT_N] += load * 0.2f;
            weights[r][(r + 7) % WHT_N] += load * 0.1f;
            weights[r][(r + 13) % WHT_N] += load * 0.1f;  // dissonant intervals
        }
    }

    // Comfort > 0.7: reinforce diagonal (cleaner)
    float comfort = (cp.comfort > 0.7f) ? (cp.comfort - 0.7f) * 2.0f : 0.0f;
    if (comfort > 0.0f) {
        for (int r = 0; r < WHT_N; r++)
            weights[r][r] *= (1.0f + comfort * 0.5f);
    }

    // Agency: boost high-frequency channels
    float agency = (cp.agency > 0.7f) ? (cp.agency - 0.7f) * 2.0f : 0.0f;
    if (agency > 0.0f) {
        for (int r = 16; r < WHT_N; r++) {
            for (int c = 0; c < WHT_N; c++)
                weights[r][c] *= (1.0f + agency * 0.3f);
        }
    }
}

// Pillar-driven audio effects — uses CognitiveProjection for emotional mapping
void audio_apply_pillar_effects(AudioSystem ctx, const float pillars[NumPillars]) {
    if (!ctx || !pillars) return;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    
    PillarStateVector psv;
    for (int i = 0; i < NumPillars; i++) psv.pillars[i] = vn::fp20_t(pillars[i]);
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

    // Auto-start WHT processor when cognitive state is non-trivial
    if (!impl->wht_active && (cp.cognitive_load > 0.3f || cp.agency > 0.3f || cp.comfort > 0.3f)) {
        audio_start_wht_processor(ctx, 0.04f);
    }

    // Update WHT mixing weights from cognitive state
    if (impl->wht_active) {
        float new_weights[WHT_N][WHT_N];
        generate_mix_weights_from_cognitive(new_weights, cp);
        audio_update_wht_weights(ctx, new_weights);
    }
}

void audio_update(AudioSystem ctx, float dt) {
    if (!ctx) return;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    (void)dt;
    for (auto it = impl->active_sfx.begin(); it != impl->active_sfx.end(); ) {
        if (ma_sound_at_end(*it)) {
            ma_sound_uninit(*it);
            delete *it;
            it = impl->active_sfx.erase(it);
        } else {
            ++it;
        }
    }
}

// Shared WHT weights matrix — updated by audio_update_wht_weights / data source callback.
// Used by both the data source callbacks and standalone audio_process_wht_block.
static float s_wht_weights[WHT_N][WHT_N];
static bool s_wht_initialized = false;

// ─── WHT Audio Source Callbacks ────────────────────────────────────────────

static ma_result wht_ds_on_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead) {
    WHTAudioSource* wht = (WHTAudioSource*)pDataSource;
    float* out = (float*)pFramesOut;
    ma_uint64 frames = (frameCount < 512) ? frameCount : 512;  // cap per-callback
    ma_uint64 framesGenerated = 0;
    float block_buf[WHT_AUDIO_BLOCK_SIZE];

    // Precompute WHT-domain weights when dirty
    if (wht->weights_dirty) {
        precompute_wht_weights(wht->weights, wht->wht_weights);
        // Also sync shared weights for standalone audio_process_wht_block
        memcpy(s_wht_weights, wht->wht_weights, sizeof(s_wht_weights));
        s_wht_initialized = true;
        wht->weights_dirty = false;
    }

    for (ma_uint64 f = 0; f < frames; f++) {
        // Generate WHT_N sine oscillators
        for (int ch = 0; ch < WHT_AUDIO_SINE_COUNT; ch++) {
            block_buf[ch] = sinf(wht->phases[ch]);
            wht->phases[ch] += 2.0f * 3.14159265f * s_wht_sine_freqs[ch] / (float)wht->sample_rate;
            // Wrap phase to prevent float drift
            if (wht->phases[ch] > 6.2831853f) wht->phases[ch] -= 6.2831853f;
        }

        // Transform to WHT domain
        fwht(block_buf, WHT_LOG2_N);

        // Mix through WHT weights: 32 virtual channels
        float mixed[WHT_AUDIO_BLOCK_SIZE];
        for (int out_ch = 0; out_ch < WHT_AUDIO_BLOCK_SIZE; out_ch++) {
            float sum = 0.0f;
            for (int i = 0; i < WHT_AUDIO_BLOCK_SIZE; i++)
                sum += block_buf[i] * wht->wht_weights[out_ch][i];
            mixed[out_ch] = sum * wht->gain;
        }

        // Stereo output: L = sum(ch0..ch15), R = sum(ch16..ch31)
        float L = 0.0f, R = 0.0f;
        for (int i = 0; i < 16; i++) L += mixed[i];
        for (int i = 16; i < 32; i++) R += mixed[i];
        out[f * 2 + 0] = L;
        out[f * 2 + 1] = R;

        framesGenerated++;
    }

    *pFramesRead = framesGenerated;
    return MA_SUCCESS;
}

static ma_result wht_ds_on_seek(ma_data_source* pDataSource, ma_uint64 frameIndex) {
    (void)pDataSource;
    (void)frameIndex;
    return MA_SUCCESS;  // infinite source, no seeking needed
}

static ma_result wht_ds_on_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap) {
    WHTAudioSource* wht = (WHTAudioSource*)pDataSource;
    if (pFormat)      *pFormat = ma_format_f32;
    if (pChannels)    *pChannels = 2;
    if (pSampleRate)  *pSampleRate = wht->sample_rate;
    if (pChannelMap && channelMapCap >= 2) {
        pChannelMap[0] = MA_CHANNEL_FRONT_LEFT;
        pChannelMap[1] = MA_CHANNEL_FRONT_RIGHT;
    }
    return MA_SUCCESS;
}

static ma_result wht_ds_on_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor) {
    (void)pDataSource;
    if (pCursor) *pCursor = 0;
    return MA_SUCCESS;
}

static ma_result wht_ds_on_get_length(ma_data_source* pDataSource, ma_uint64* pLength) {
    (void)pDataSource;
    if (pLength) *pLength = MA_UINT64_MAX;  // infinite
    return MA_SUCCESS;
}

static ma_result wht_ds_on_set_looping(ma_data_source* pDataSource, ma_bool32 isLooping) {
    (void)pDataSource;
    (void)isLooping;
    return MA_SUCCESS;  // always "looping" (infinite)
}

// ─── Public WHT Audio API ──────────────────────────────────────────────────

// Start continuous WHT-processed procedural audio
int32_t audio_start_wht_processor(AudioSystem ctx, float gain) {
    if (!ctx) return -1;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    if (impl->wht_active) return 0;  // already running

    impl->wht_source.gain = gain;
    impl->wht_source.sample_rate = ma_engine_get_sample_rate(&impl->engine);
    impl->wht_source.weights_dirty = true;

    ma_data_source_config ds_config = ma_data_source_config_init();
    ds_config.vtable = &g_wht_ds_vtable;

    if (ma_data_source_init(&ds_config, (ma_data_source*)&impl->wht_source) != MA_SUCCESS)
        return -1;

    ma_uint32 flags = MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_LOOPING;
    if (ma_sound_init_from_data_source(&impl->engine, (ma_data_source*)&impl->wht_source,
                                       flags, nullptr, &impl->wht_sound) != MA_SUCCESS) {
        return -1;
    }

    ma_sound_set_volume(&impl->wht_sound, gain);
    if (ma_sound_start(&impl->wht_sound) != MA_SUCCESS) {
        ma_sound_uninit(&impl->wht_sound);
        return -1;
    }

    impl->wht_active = true;
    return 0;
}

// Stop WHT processor
void audio_stop_wht_processor(AudioSystem ctx) {
    if (!ctx) return;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    if (!impl->wht_active) return;
    ma_sound_stop(&impl->wht_sound);
    ma_sound_uninit(&impl->wht_sound);
    impl->wht_active = false;
}

// Update WHT mixing weights from pillar cognitive state
void audio_update_wht_weights(AudioSystem ctx, const float weights[WHT_N][WHT_N]) {
    if (!ctx || !weights) return;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;
    memcpy(impl->wht_source.weights, weights, sizeof(impl->wht_source.weights));
    impl->wht_source.weights_dirty = true;
}

// Generate WHT mixing weights from pillar cognitive projection.
// The weights matrix maps 16 pillar sine tones → 32 output channels via WHT.
// - cognitive_load > 0.7 → off-diagonal cross-terms increase (dissonance/mixing)
// - comfort      > 0.7 → diagonal dominance (clean/pure tones)
// - agency       > 0.7 → higher gain on high-frequency channels
void audio_generate_pillar_mix_weights(AudioSystem ctx) {
    if (!ctx) return;
    AudioSystemImpl* impl = (AudioSystemImpl*)ctx;

    // For headless mode (no active simulation pillars), keep identity
    if (!impl->wht_active) return;

    // We don't have access to current pillars here directly (they come via
    // audio_apply_pillar_effects), so keep the current weights unless explicitly updated.
    // This function is a hook for future integration.
}

// Process audio block: 32 input samples → 32 output channels via dot-product mixing.
// The weights are stored pre-transformed in the WHT domain, so we fwht once and
// compute all dot products at O(N log N + N^2) instead of O(N^3) for per-channel ifwht.
// By Parseval's theorem: fwht(input) · fwht(weights[out]) = input · weights[out].
void audio_process_wht_block(float* input_block, float* output_block, int block_size) {
    if (block_size != WHT_N || !s_wht_initialized) {
        for (int i = 0; i < block_size && i < WHT_N; i++)
            output_block[i] = (i < block_size) ? input_block[i] : 0.0f;
        return;
    }

    float x_wht[WHT_N];
    memcpy(x_wht, input_block, sizeof(x_wht));
    fwht(x_wht, WHT_LOG2_N);

    for (int out = 0; out < WHT_N; out++) {
        float sum = 0.0f;
        for (int i = 0; i < WHT_N; i++)
            sum += x_wht[i] * s_wht_weights[out][i];
        output_block[out] = sum;
    }
}

// Initialize shared WHT weights
void audio_init_wht_weights(float weights[WHT_N][WHT_N]) {
    precompute_wht_weights(weights, s_wht_weights);
    s_wht_initialized = true;
}
