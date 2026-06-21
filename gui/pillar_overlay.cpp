#include "pillar_overlay.h"
#include "../scale/SemanticProjection.h"
#include <cmath>

PillarOverlay::PillarOverlay()
    : visible_(true), pos_x_(10.0f), pos_y_(10.0f),
      width_(300.0f), height_(550.0f), alpha_(0.8f),
      has_wrapper_(false) {
    active_wrapper_.init();
}

void PillarOverlay::render(const PillarVector& pillars, float fps, int entity_count) {
    if (!visible_) return;
    
    ImGui::SetNextWindowPos(ImVec2(pos_x_, pos_y_), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(width_, height_), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(alpha_);
    
    if (ImGui::Begin("Pillar HUD", &visible_, 
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        
        // Title
        ImGui::Text("PCMSRM State");
        ImGui::Separator();
        
        // FPS and entity count
        ImGui::Text("FPS: %.1f | Entities: %d", fps, entity_count);
        ImGui::Separator();
        
        // Render each pillar
for (int i = 0; i < NumPillars; i++) {
            render_pillar_bar(i, pillars[i], get_pillar_name(i));
        }
        
        ImGui::Separator();
        ImGui::TextDisabled("Press 'P' to toggle");
    }
    ImGui::End();
}

void PillarOverlay::render_pillar_bar(int idx, float value, const char* name) {
    float val = value;
    if (val < 0.0f) val = 0.0f;
    if (val > 1.0f) val = 1.0f;
    
    ImVec4 color = get_pillar_color(val);
    
    ImGui::BeginGroup();
    ImGui::Text("%s", name);
    ImGui::SameLine(100.0f);
    
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::ProgressBar(val, ImVec2(150.0f, 16.0f));
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    ImGui::Text("%.2f", val);
    ImGui::EndGroup();
    
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s: %.3f", name, val);
        switch (idx) {
            case 0: ImGui::Text("Biological: awareness_orientation"); ImGui::Text("Cognitive: focus_inward=%.2f", 1.0f-val); ImGui::Text("Civil: surveillance"); ImGui::Text("Symbolic: —"); break;
            case 1: ImGui::Text("Biological: willpower_stamina"); ImGui::Text("Cognitive: agency"); ImGui::Text("Civil: sovereignty"); ImGui::Text("Symbolic: heroic_will"); break;
            case 2: ImGui::Text("Biological: force_output"); ImGui::Text("Cognitive: agency"); ImGui::Text("Civil: production_force"); ImGui::Text("Symbolic: —"); break;
            case 3: ImGui::Text("Biological: —"); ImGui::Text("Cognitive: —"); ImGui::Text("Civil: cultural_influence"); ImGui::Text("Symbolic: presence_mana"); break;
            case 4: ImGui::Text("Biological: —"); ImGui::Text("Cognitive: —"); ImGui::Text("Civil: institutional_stability"); ImGui::Text("Symbolic: sacred_boundary"); break;
            case 5: ImGui::Text("Biological: membrane_integrity"); ImGui::Text("Cognitive: identity_coherence"); ImGui::Text("Civil: legal_integrity"); ImGui::Text("Symbolic: sacred_boundary"); break;
            case 6: ImGui::Text("Biological: —"); ImGui::Text("Cognitive: —"); ImGui::Text("Civil: social_cohesion"); ImGui::Text("Symbolic: —"); break;
            case 7: ImGui::Text("Biological: —"); ImGui::Text("Cognitive: social_memory"); ImGui::Text("Civil: diplomatic_relation"); ImGui::Text("Symbolic: ancestor_memory"); break;
            case 8: ImGui::Text("Biological: —"); ImGui::Text("Cognitive: —"); ImGui::Text("Civil: —"); ImGui::Text("Symbolic: presence_mana"); break;
            case 9: ImGui::Text("Biological: metabolic_rate"); ImGui::Text("Cognitive: comfort"); ImGui::Text("Civil: —"); ImGui::Text("Symbolic: numinous_warmth"); break;
            case 10: ImGui::Text("Biological: depth_reservoir"); ImGui::Text("Cognitive: experiential_memory"); ImGui::Text("Civil: archival_depth"); ImGui::Text("Symbolic: ancestor_memory"); break;
            case 11: ImGui::Text("Biological: resource_affinity"); ImGui::Text("Cognitive: curiosity"); ImGui::Text("Civil: —"); ImGui::Text("Symbolic: gravitational_love"); break;
            case 12:
                ImGui::Text("Biological: stress_level"); ImGui::Text("Cognitive: cognitive_load"); ImGui::Text("Civil: —"); ImGui::Text("Symbolic: transformational_harm");
                if (val > 0.7f) ImGui::Text("HIGH STRESS!");
                break;
            case 13:
                ImGui::Text("Biological: sensory_noise"); ImGui::Text("Cognitive: imaginativeness"); ImGui::Text("Civil: —"); ImGui::Text("Symbolic: trickster_phase");
                if (val > 0.5f) ImGui::Text("High sensory noise!");
                break;
            case 14:
                ImGui::Text("Biological: flux_metabolism"); ImGui::Text("Cognitive: —"); ImGui::Text("Civil: economic_velocity"); ImGui::Text("Symbolic: flux_weaving");
                break;
            case 15:
                ImGui::Text("Biological: depth_reservoir"); ImGui::Text("Cognitive: depth_capacity"); ImGui::Text("Civil: —"); ImGui::Text("Symbolic: abyss_depth");
                break;
        }
        ImGui::EndTooltip();
    }
}

const char* PillarOverlay::get_pillar_name(int idx) {
    static const char* names[NumPillars] = {
        "Awareness", "Willpower", "Force", "Influence", "Resistance",
        "Integrity", "Cohesion", "Relation", "Presence", "Warmth",
        "Memory", "Attraction", "Harm", "Distortion", "Flux", "Depth"
    };
    return (idx >= 0 && idx < NumPillars) ? names[idx] : "Unknown";
}

ImVec4 PillarOverlay::get_pillar_color(float value) {
    if (value > 0.7f) {
        return ImVec4(0.2f, 0.8f, 0.2f, 1.0f);  // Green
    } else if (value >= 0.4f) {
        return ImVec4(0.8f, 0.8f, 0.2f, 1.0f);  // Yellow
    } else {
        return ImVec4(0.8f, 0.2f, 0.2f, 1.0f);  // Red
    }
}
