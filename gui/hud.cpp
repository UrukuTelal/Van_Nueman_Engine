// hud.cpp - In-game HUD with pillar state overlay

#include "imgui.h"
#include "../include/Entity.h"
#include "../include/SkellyInstance.h"

// HUD state
struct HUDState {
    bool show_pillar_overlay = true;
    bool show_fps = true;
    bool show_minimap = false;
    float pillar_overlay_alpha = 0.8f;
};

void render_hud(HUDState& state, const float pillar_values[NUM_PILLARS], 
                 float fps, int entity_count, int server_count) {
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(300, 150));
    ImGui::Begin("HUD", nullptr, 
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
    
    // FPS counter
    if (state.show_fps) {
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Separator();
    }
    
    // Entity/Server count
    ImGui::Text("Entities: %d", entity_count);
    ImGui::Text("Servers: %d", server_count);
    ImGui::Separator();
    
    // Pillar overlay (mini)
    if (state.show_pillar_overlay) {
        ImGui::Text("Pillars:");
        for (int i = 0; i < NUM_PILLARS; i++) {
            float val = pillar_values ? pillar_values[i] : 0.0f;
            ImGui::PushID(i);
            ImGui::ProgressBar(val, ImVec2(200, 10));
            ImGui::SameLine();
            ImGui::Text("%d", i);
            ImGui::PopID();
        }
    }
    
    ImGui::End();
}

// Pillar tooltip for detailed view
void render_pillar_tooltip(int pillar_idx, float value, const char* name) {
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s: %.2f", name, value);
        
        // Show meaning
        switch (pillar_idx) {
            case PILLAR_AWARENESS: ImGui::Text("Intelligence/Analytics"); break;
            case PILLAR_WILLPOWER: ImGui::Text("Strategic Persistence"); break;
            case PILLAR_FORCE: ImGui::Text("Execution/Production"); break;
            case PILLAR_HARM: 
                if (value > 0.7f) ImGui::TextColored(ImVec4(1,0,0,1), "HIGH HARM!"); 
                break;
            case PILLAR_DISTORTION: 
                if (value > 0.5f) ImGui::TextColored(ImVec4(1,0.5f,0,1), "High Distortion"); 
                break;
        }
        ImGui::EndTooltip();
    }
}
