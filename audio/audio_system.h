// audio_system.h - Audio system using miniaudio

#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NUM_PILLARS
#define NUM_PILLARS 16
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
void audio_apply_voice_pillar_effects(VoiceSystemHandle voice_ctx, const float pillars[NUM_PILLARS]);

// Pillar-driven generative audio (existing)
// Harm pillar increases dissonance
// Warmth pillar increases soothing tones
void audio_apply_pillar_effects(AudioSystem ctx, const float pillars[NUM_PILLARS]);

// Update (call each frame)
void audio_update(AudioSystem ctx, float dt);

#ifdef __cplusplus
}
#endif
