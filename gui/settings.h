// settings.h - Settings menu header
#pragma once

#include <imgui.h>

struct SettingsState {
    bool show_settings = false;
    float master_volume = 0.8f;
    float music_volume = 0.5f;
    float sfx_volume = 0.7f;
    int resolution_idx = 1; // 1920x1080
    bool fullscreen = false;
    bool vsync = true;
    int pillar_display_mode = 0;
    bool show_fps = true;
    bool ephemeral_tutorial = true;
};

void render_settings(SettingsState& state);
void check_settings_toggle(SettingsState& state);
