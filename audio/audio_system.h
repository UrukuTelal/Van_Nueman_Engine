// audio_system.h - Audio system using miniaudio

#pragma once

#include <cstdint>
#include <cstddef>
#include <PillarEnum.h>
#include "../audio/wht.h"

#ifdef __cplusplus
extern "C" {
#endif


// Opaque handle
typedef struct AudioSystemImpl* AudioSystem;

// Initialize audio system
AudioSystem audio_init(float master_volume);

// Shutdown
void audio_shutdown(AudioSystem ctx);

// Play music (streaming from file)
int32_t audio_play_music(AudioSystem ctx, const char* file_path, bool loop);

// Play SFX (one-shot)
int32_t audio_play_sfx(AudioSystem ctx, const char* file_path, float volume);

// Stop music
void audio_stop_music(AudioSystem ctx);

// Set volumes
void audio_set_master_volume(AudioSystem ctx, float volume);
void audio_set_music_volume(AudioSystem ctx, float volume);
void audio_set_sfx_volume(AudioSystem ctx, float volume);

// WHT-based audio processing — uses wht.h constants (WHT_N=32, WHT_LOG2_N=5)
// Start continuous WHT-processed procedural audio (driven by pillar state)
int32_t audio_start_wht_processor(AudioSystem ctx, float gain);
// Stop WHT processor
void audio_stop_wht_processor(AudioSystem ctx);
// Update WHT mixing weights matrix
void audio_update_wht_weights(AudioSystem ctx, const float weights[WHT_N][WHT_N]);
// Generate WHT mixing weights from current pillar cognitive projection
void audio_generate_pillar_mix_weights(AudioSystem ctx);
// Process 32 samples through WHT mixing matrix
void audio_process_wht_block(float* input_block, float* output_block, int block_size);
// Initialize WHT weights from standard time-domain matrix
void audio_init_wht_weights(float weights[WHT_N][WHT_N]);

// Voice system integration (NEW)
typedef struct VoiceSystem* VoiceSystemHandle;

// Initialize voice system (Whisper for STT, Piper for TTS)
VoiceSystemHandle audio_init_voice(const char* whisper_model_path);
void audio_shutdown_voice(VoiceSystemHandle voice_ctx);

// Speech-to-Text (STT) - Returns malloc'd string
char* audio_recognize_speech(VoiceSystemHandle voice_ctx, int max_len);

// Text-to-Speech (TTS)
void audio_speak(VoiceSystemHandle voice_ctx, const char* text, float volume);
void audio_stop_speaking(VoiceSystemHandle voice_ctx);

// Voice pillar effects (16 pillars)
void audio_apply_voice_pillar_effects(VoiceSystemHandle voice_ctx, const float pillars[NumPillars]);

// Pillar-driven generative audio (existing)
// Harm pillar increases dissonance
// Warmth pillar increases soothing tones
void audio_apply_pillar_effects(AudioSystem ctx, const float pillars[NumPillars]);

// Update (call each frame)
void audio_update(AudioSystem ctx, float dt);

#ifdef __cplusplus
}
#endif
