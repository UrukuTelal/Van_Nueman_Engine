#include "voice_system.h"
#include "wht.h"
#include "../include/Entity.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#if VOICE_ENABLED
#include <whisper.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif
#endif // VOICE_ENABLED

#if VOICE_ENABLED

struct VoiceSystemImpl {
    whisper_context* whisper_ctx;
    char* whisper_model_path;
    bool streaming_active;
    float tts_volume;
    float mic_volume;
    float current_pillars[NumPillars];
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

    for (int i = 0; i < NumPillars; i++) {
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

    if (max_len <= 0) return nullptr;

    char* text_buffer = new char[max_len];
    memset(text_buffer, 0, max_len);

    float* audio_data = new float[16000]();

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;

    if (whisper_full(impl->whisper_ctx, params, audio_data, 16000) != 0) {
        printf("[Voice] Whisper processing failed\n");
        delete[] text_buffer;
        delete[] audio_data;
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
    delete[] audio_data;

    return result;
}

static bool is_safe_path(const char* path) {
    if (!path || !path[0]) return false;
    // Only allow alphanumeric, underscore, hyphen, dot, and path separators
    for (const char* p = path; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' ||
              *p == '.' || *p == '/' || *p == '\\')) {
            return false;
        }
    }
    return true;
}

static bool is_safe_text(const char* text) {
    if (!text || !text[0]) return false;
    for (const char* p = text; *p; p++) {
        char c = *p;
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') return false;
        if (c == '"' || c == '\'' || c == '`' || c == ';' || c == '|' ||
            c == '&' || c == '$' || c == '<' || c == '>' || c == '(' || c == ')' ||
            c == '[' || c == ']' || c == '{' || c == '}' || c == '!' || c == '~') return false;
    }
    return true;
}

void voice_speak(VoiceSystem ctx, const char* text) {
    if (!ctx || !text) return;
    VoiceSystemImpl* impl = (VoiceSystemImpl*)ctx;

    printf("[Voice] Synthesizing speech: %s\n", text);

    float warmth = impl->current_pillars[Warmth];
    float harm = impl->current_pillars[Harm];

    float volume = impl->tts_volume * (1.0f - harm * 0.5f) * (0.5f + warmth * 0.5f);

    // Validate model path before use
    const char* model_path = impl->whisper_model_path;
    if (model_path && !is_safe_path(model_path)) {
        printf("[Voice] Error: Invalid model path rejected (contains unsafe characters)\n");
        return;
    }
    if (!model_path) model_path = "default";

    // Validate speech text to prevent command injection
    if (!is_safe_text(text)) {
        printf("[Voice] Error: Invalid speech text rejected (contains unsafe characters)\n");
        return;
    }

    char vol_arg[32];
    snprintf(vol_arg, sizeof(vol_arg), "%.1f", volume);

    size_t text_len = strlen(text);
    if (text_len > 4096) {
        printf("[Voice] Error: Speech text too long (%zu chars, max 4096)\n", text_len);
        return;
    }

#ifdef _WIN32
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    // Use separate argv-style: pass --text via stdin pipe to avoid arg injection
    // Build command with validated components only
    char model_arg[512];
    snprintf(model_arg, sizeof(model_arg), "--model=%s", model_path);
    // Write text to temp file for piper to read (avoid cmdline injection entirely)
    char temp_path[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, temp_path)) {
        printf("[Voice] Error: GetTempPath failed\n");
        return;
    }
    char temp_file[MAX_PATH];
    if (GetTempFileNameA(temp_path, "vnv", 0, temp_file) == 0) {
        printf("[Voice] Error: GetTempFileNameA failed (error %lu)\n", GetLastError());
        return;
    }
    // Change extension from .tmp to .txt
    size_t tlen = strlen(temp_file);
    if (tlen > 4) {
        temp_file[tlen - 4] = '\0';
        strncat(temp_file, ".txt", MAX_PATH - tlen + 3);
    }
    HANDLE hFile = CreateFileA(temp_file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("[Voice] Error: Failed to create temp file (error %lu)\n", GetLastError());
        return;
    }
    DWORD written;
    WriteFile(hFile, text, (DWORD)text_len, &written, NULL);
    CloseHandle(hFile);

    char cmdline[8192];
    snprintf(cmdline, sizeof(cmdline), "piper --volume %s --model \"%s\" --input-file \"%s\"",
             vol_arg, model_path, temp_file);
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("[Voice] Error: Failed to execute piper (error %lu)\n", GetLastError());
    } else {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    DeleteFileA(temp_file);
#else
    // Write text to a temp file to avoid cmdline injection
    char temp_file[] = "/tmp/vn_voice_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd < 0) {
        printf("[Voice] Error: Failed to create temp file\n");
        return;
    }
    write(fd, text, text_len);
    close(fd);

    pid_t pid = fork();
    if (pid == 0) {
        execlp("piper", "piper", "--volume", vol_arg, "--model", model_path,
               "--input-file", temp_file, (char*)NULL);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        printf("[Voice] Error: fork failed\n");
    }
    unlink(temp_file);
#endif

    printf("[Voice] Speech synthesized\n");
}

void voice_encode_wht(VoiceSystem ctx, float* out_wht, int wht_n) {
    if (!ctx || !out_wht) return;
    VoiceSystemImpl* impl = (VoiceSystemImpl*)ctx;

    for (int i = 0; i < wht_n && i < NumPillars; i++) {
        out_wht[i] = impl->current_pillars[i];
    }
    for (int i = NumPillars; i < wht_n; i++) {
        out_wht[i] = 0.0f;
    }
}

void voice_set_pillars(VoiceSystem ctx, const float* pillars, int count) {
    if (!ctx || !pillars) return;
    VoiceSystemImpl* impl = (VoiceSystemImpl*)ctx;

    int copy_count = count < NumPillars ? count : NumPillars;
    for (int i = 0; i < copy_count; i++) {
        impl->current_pillars[i] = pillars[i];
    }
}

#endif // VOICE_ENABLED
