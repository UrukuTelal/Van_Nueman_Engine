// settings.cpp - Settings menu for Van Nueman

#include "imgui.h"
#include "../include/Entity.h"

// Settings state
struct SettingsState {
    bool show_settings = false;
    float master_volume = 0.8f;
    float music_volume = 0.5f;
    float sfx_volume = 0.7f;
    int resolution_idx = 1;
    bool fullscreen = false;
    bool vsync = true;
    int pillar_display_mode = 0;
    bool show_fps = true;
    bool ephemeral_tutorial = true;
    // Graphics settings
    int graphics_quality = 1; // 0=Low, 1=Medium, 2=High
    float render_distance = 500.0f;
    bool enable_svo = true;
    bool enable_shadows = true;
    float brightness = 1.0f;
    float gamma = 1.0f;
};

void render_settings(SettingsState& state) {
    if (!state.show_settings) return;
    
    ImGui::SetNextWindowSize(ImVec2(550, 700), ImGuiCond_FirstUseEver);
    ImGui::Begin("Settings", &state.show_settings);
    
    // Audio settings
    if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Master Volume", &state.master_volume, 0.0f, 1.0f);
        ImGui::SliderFloat("Music Volume", &state.music_volume, 0.0f, 1.0f);
        ImGui::SliderFloat("SFX Volume", &state.sfx_volume, 0.0f, 1.0f);
    }
    
    // Display settings
    if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Fullscreen", &state.fullscreen);
        ImGui::Checkbox("V-Sync", &state.vsync);
        ImGui::Checkbox("Show FPS", &state.show_fps);
        const char* resolutions[] = {"1280x720", "1920x1080", "2560x1440", "3840x2160"};
        ImGui::Combo("Resolution", &state.resolution_idx, resolutions, 4);
    }
    
    // Graphics settings
    if (ImGui::CollapsingHeader("Graphics", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* quality_levels[] = {"Low", "Medium", "High"};
        ImGui::Combo("Graphics Quality", &state.graphics_quality, quality_levels, 3);
        ImGui::SliderFloat("Render Distance", &state.render_distance, 100.0f, 2000.0f);
        ImGui::Checkbox("Enable SVO Rendering", &state.enable_svo);
        ImGui::Checkbox("Enable Shadows", &state.enable_shadows);
        ImGui::SliderFloat("Brightness", &state.brightness, 0.5f, 2.0f);
        ImGui::SliderFloat("Gamma", &state.gamma, 0.5f, 3.0f);
    }
    
    // Pillar display settings
    if (ImGui::CollapsingHeader("Pillars", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* display_modes[] = {"Off", "Mini Overlay", "Full Overlay"};
        ImGui::Combo("Pillar Display", &state.pillar_display_mode, display_modes, 3);
        ImGui::Text("Harm Threshold: 0.7 (Rejection)");
        ImGui::Text("Distortion SNR Threshold: 0.5");
        ImGui::Text("Transformation Threshold: 0.7");
    }
    
    // Gameplay settings
    if (ImGui::CollapsingHeader("Gameplay", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Ephemeral Tutorial (no persistence)", &state.ephemeral_tutorial);
        ImGui::Text("Tutorial steps will not persist to multiplayer.");
    }
    
    // Apply button
    if (ImGui::Button("Apply Settings", ImVec2(-1, 40))) {
        // Apply settings to engine
    }
    
    ImGui::End();
}

// Global shortcut to toggle settings
void check_settings_toggle(SettingsState& state) {
    if (ImGui::IsKeyPressed(ImGuiKey_F10)) {
        state.show_settings = !state.show_settings;
    }
}
