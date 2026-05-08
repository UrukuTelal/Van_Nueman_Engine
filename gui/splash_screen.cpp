// splash_screen.cpp - Game splash screen with Pillar visualization

#include <iostream>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "../include/Entity.h"

// Splash screen state
struct SplashState {
    bool show_splash = true;
    float progress = 0.0f;
    const char* loading_text = "Initializing Pillars...";
};

void render_splash_screen(SplashState& state, float dt) {
    if (!state.show_splash) return;
    
    // Centered splash window
    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::Begin("Van Nueman", nullptr, 
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
    
    // Title
    ImVec2 window_size = ImGui::GetWindowSize();
    ImGui::SetCursorPos(ImVec2(window_size.x * 0.5f - 150.0f, window_size.y * 0.5f - 100.0f));
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.6f, 1.0f), "VAN N U E M A N");
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 50);
    ImGui::TextDisabled("Pillar-Constrained Multi-Scale Representative Model");
    
    // Progress bar
    ImGui::SetCursorPos(ImVec2(100.0f, window_size.y - 150.0f));
    ImGui::ProgressBar(state.progress, ImVec2(window_size.x - 200.0f, 30.0f));
    ImGui::SetCursorPosX(100.0f);
    ImGui::Text("%s", state.loading_text);
    
    // 14 Pillar indicators (small colored dots)
    ImGui::SetCursorPos(ImVec2(100.0f, window_size.y - 100.0f));
    const char* pillar_names[] = {"Aw", "Wp", "F", "In", "Rs", "It", "Ch", 
                                   "Rl", "Pr", "Wm", "Me", "At", "H", "Ds"};
    for (int i = 0; i < NUM_PILLARS; i++) {
        float hue = (float)i / NUM_PILLARS;
        ImVec4 color;
        ImGui::ColorConvertHSVtoRGB(hue, 0.8f, 0.9f, color.x, color.y, color.z);
        color.w = state.progress;
        ImGui::ColorButton(pillar_names[i], color, 0, ImVec2(20, 20));
        if (i < NUM_PILLARS - 1) ImGui::SameLine();
    }
    
    ImGui::End();
    
    // Update progress
    state.progress += dt * 0.5f;
    if (state.progress > 1.0f) {
        state.progress = 1.0f;
        state.loading_text = "Complete!";
        if (state.progress >= 1.0f && ImGui::GetTime() > 3.0) {
            state.show_splash = false;
        }
    }
}
