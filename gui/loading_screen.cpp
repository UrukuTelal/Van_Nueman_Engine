// loading_screen.cpp - Loading screen with progress

#include "imgui.h"
#include "../include/Entity.h"

// Loading state
struct LoadingState {
    bool is_loading = false;
    float progress = 0.0f;
    const char* current_task = "Initializing...";
    const char* status_text = "";
    int current_pillar = 0;  // Which pillar is being loaded
};

void render_loading_screen(LoadingState& state) {
    if (!state.is_loading) return;
    
    // Fullscreen loading window
    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::Begin("Loading", nullptr, 
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
    
    // Centered content
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 center = ImVec2(window_size.x * 0.5f, window_size.y * 0.5f);
    ImGui::SetCursorPos(ImVec2(center.x - 150.0f, center.y - 100.0f));
    
    // Title
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.6f, 1.0f), "VAN N U E M A N");
    ImGui::SetCursorPosX(center.x - 100);
    ImGui::TextDisabled("Loading...");
    
    // Progress bar
    ImGui::SetCursorPos(ImVec2(center.x - 200, center.y));
    ImGui::ProgressBar(state.progress, ImVec2(400, 30));
    ImGui::SetCursorPosX(center.x - 200);
    ImGui::Text("%s", state.current_task);
    ImGui::SetCursorPosX(center.x - 200);
    ImGui::TextDisabled("%s", state.status_text);
    
    // Pillar loading indicators
    ImGui::SetCursorPos(ImVec2(center.x - 200, center.y + 80));
    ImGui::Text("Loading Pillars:");
    for (int i = 0; i < NumPillars; i++) {
        ImVec4 color;
        ImGui::ColorConvertHSVtoRGB((float)i / NumPillars, 0.8f, 0.9f, color.x, color.y, color.z);
        if (i < state.current_pillar) {
            color.w = 1.0f;  // Loaded
        } else if (i == state.current_pillar) {
            color.w = state.progress;  // Loading
        } else {
            color.w = 0.3f;  // Not started
        }
        
        ImGui::SameLine();
        ImGui::ColorButton("pillar", color, 0, ImVec2(20, 20));
    }
    
    ImGui::End();
}

// Update loading progress
void update_loading_progress(LoadingState& state, float dt, const char* task) {
    state.current_task = task;
    state.progress += dt * 0.5f;  // Simulate loading
    if (state.progress > 1.0f) {
        state.progress = 1.0f;
        state.status_text = "Complete!";
        state.is_loading = false;
    }
}
