// main_menu.cpp - Onboarding menu with federation/server options

#include "imgui.h"
#include "../include/Entity.h"

// Menu state
struct MainMenuState {
    bool show_single_player = false;
    bool show_federation = false;
    bool show_server_config = false;
    bool show_settings = false;
    int selected_menu = 0;
};

// Draw a pillar gauge (horizontal bar)
void draw_pillar_gauge(const char* name, float value, const ImVec4& color) {
    ImGui::Text("%s", name);
    ImGui::SameLine(60);
    ImVec4 bar_color = color;
    bar_color.w = value;
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, bar_color);
    ImGui::ProgressBar(value, ImVec2(200, 20));
    ImGui::PopStyleColor();
}

void render_main_menu(MainMenuState& state, const float pillar_values[NumPillars]) {
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_Always);
    ImGui::Begin("Van Nueman - Main Menu", nullptr, ImGuiWindowFlags_NoCollapse);
    
    // Title
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.6f, 1.0f), "VAN N U E M A N");
    ImGui::TextDisabled("Pillar-Constrained Multi-Scale Representative Model");
    ImGui::Separator();
    
    // Onboarding menu buttons
    if (ImGui::Button("Start Single Player", ImVec2(-1, 50))) {
        state.show_single_player = true;
    }
    if (ImGui::Button("Federation (Start Server)", ImVec2(-1, 50))) {
        state.show_federation = true;
    }
    if (ImGui::Button("Configure Server", ImVec2(-1, 50))) {
        state.show_server_config = true;
    }
    if (ImGui::Button("Game Settings", ImVec2(-1, 50))) {
        state.show_settings = true;
    }
    if (ImGui::Button("Quit", ImVec2(-1, 50))) {
        // Signal quit via global flag
    }
    
    ImGui::Separator();
    
    // Pillar state visualization
    ImGui::Text("System Pillar State:");
    if (pillar_values) {
        const char* pillar_names[] = {"Aw", "Wp", "F", "In", "Rs", "It", "Ch", 
                                       "Rl", "Pr", "Wm", "Me", "At", "H", "Ds"};
        const ImVec4 pillar_colors[] = {
            ImVec4(0.2f, 0.8f, 0.2f, 1.0f), ImVec4(0.8f, 0.2f, 0.2f, 1.0f),
            ImVec4(0.9f, 0.5f, 0.1f, 1.0f), ImVec4(0.5f, 0.3f, 0.8f, 1.0f),
            ImVec4(0.2f, 0.2f, 0.8f, 1.0f), ImVec4(0.8f, 0.8f, 0.2f, 1.0f),
            ImVec4(0.2f, 0.8f, 0.8f, 1.0f), ImVec4(0.8f, 0.2f, 0.8f, 1.0f),
            ImVec4(0.9f, 0.9f, 0.9f, 1.0f), ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
            ImVec4(0.5f, 0.5f, 0.5f, 1.0f), ImVec4(0.1f, 0.9f, 0.3f, 1.0f),
            ImVec4(0.9f, 0.1f, 0.1f, 1.0f), ImVec4(0.6f, 0.6f, 0.6f, 0.5f)
        };
        
        for (int i = 0; i < NumPillars; i++) {
            ImVec4 color = pillar_colors[i];
            color.w = pillar_values[i];
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
            ImGui::ProgressBar(pillar_values[i], ImVec2(100, 15));
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::Text("%s", pillar_names[i]);
            if ((i + 1) % 4 == 0) ImGui::NewLine();
        }
    }
    
    ImGui::End();
}
