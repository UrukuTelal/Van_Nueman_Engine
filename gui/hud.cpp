// hud.cpp - In-game HUD with pillar state overlay

#include "imgui.h"
#include "../include/Entity.h"
#include "../include/SkellyInstance.h"
#include "../scale/SemanticProjection.h"

// HUD state
struct HUDState {
    bool show_pillar_overlay = true;
    bool show_fps = true;
    bool show_minimap = false;
    float pillar_overlay_alpha = 0.8f;
};

void render_hud(HUDState& state, const float pillar_values[NumPillars], 
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
        for (int i = 0; i < NumPillars; i++) {
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

// Pillar tooltip for detailed view — uses CognitiveProjection (Layer 2)
void render_pillar_tooltip(int pillar_idx, float value, const char* name) {
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s: %.2f", name, value);
        
        // Project to cognitive and biological semantics
        switch (pillar_idx) {
            case Awareness: ImGui::Text("Cognitive: focus_inward=%.2f", 1.0f-value); break;
            case Willpower: ImGui::Text("Biological: willpower_stamina | Cognitive: agency"); break;
            case Force: ImGui::Text("Biological: force_output | Cognitive: agency"); break;
            case Harm: 
                if (value > 0.7f) ImGui::TextColored(ImVec4(1,0,0,1), "Cognitive: high cognitive_load");
                break;
            case Distortion: 
                if (value > 0.5f) ImGui::TextColored(ImVec4(1,0.5f,0,1), "Biological: high sensory_noise");
                break;
            case Integrity: ImGui::Text("Biological: membrane_integrity | Cognitive: identity_coherence"); break;
            case Memory: ImGui::Text("Cognitive: experiential_memory"); break;
            case Attraction: ImGui::Text("Biological: resource_affinity | Cognitive: curiosity"); break;
        }
        ImGui::EndTooltip();
    }
}
