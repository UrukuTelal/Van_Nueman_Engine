#include "pillar_overlay.h"
#include <cmath>

PillarOverlay::PillarOverlay()
    : visible_(true), pos_x_(10.0f), pos_y_(10.0f),
      width_(300.0f), height_(500.0f), alpha_(0.8f) {
}

void PillarOverlay::render(const std::array<float, NUM_PILLARS>& pillars, float fps, int entity_count) {
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
for (int i = 0; i < NUM_PILLARS; i++) {
            render_pillar_bar(i, pillars[i], get_pillar_name(i));
        }
        
        ImGui::Separator();
        ImGui::TextDisabled("Press 'P' to toggle");
    }
    ImGui::End();
}

void PillarOverlay::render_pillar_bar(int idx, float value, const char* name) {
    // Clamp value
    float val = value;
    if (val < 0.0f) val = 0.0f;
    if (val > 1.0f) val = 1.0f;
    
    // Color based on value
    ImVec4 color = get_pillar_color(val);
    
    // Pillar name
    ImGui::BeginGroup();
    ImGui::Text("%s", name);
    ImGui::SameLine(100.0f);
    
    // Progress bar
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::ProgressBar(val, ImVec2(150.0f, 16.0f));
    ImGui::PopStyleColor();
    
    // Value text
    ImGui::SameLine();
    ImGui::Text("%.2f", val);
    ImGui::EndGroup();
    
    // Tooltip on hover
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s: %.3f", name, val);
        
        // Show meaning
        switch (idx) {
            case 0: ImGui::Text("Awareness - Intelligence/Analytics"); break;
            case 1: ImGui::Text("Willpower - Strategic Persistence"); break;
            case 2: ImGui::Text("Force - Execution/Production"); break;
            case 3: ImGui::Text("Influence - Communication/Culture"); break;
            case 4: ImGui::Text("Resistance - Risk/Compliance"); break;
            case 5: ImGui::Text("Integrity - Identity/Standards"); break;
            case 6: ImGui::Text("Cohesion - Team/Structure"); break;
            case 7: ImGui::Text("Relation - Network/Connect"); break;
            case 8: ImGui::Text("Presence - Priority/Visibility"); break;
            case 9: ImGui::Text("Warmth - HR/Support"); break;
            case 10: ImGui::Text("Memory - Knowledge/Learning"); break;
            case 11: ImGui::Text("Attraction - Incentives/Direction"); break;
            case 12: 
                ImGui::Text("Harm - Disruption/Damage");
                if (val > 0.7f) ImGui::Text("HIGH HARM!");
                break;
            case 13: 
                ImGui::Text("Distortion - Noise/Decay");
                if (val > 0.5f) ImGui::Text("High Distortion!");
                break;
            case 14:
                ImGui::Text("Flux - Flow/Dynamics");
                break;
            case 15:
                ImGui::Text("Depth - Stability/Resilience");
                break;
        }
        ImGui::EndTooltip();
    }
}

const char* PillarOverlay::get_pillar_name(int idx) {
    static const char* names[NUM_PILLARS] = {
        "Awareness", "Willpower", "Force", "Influence", "Resistance",
        "Integrity", "Cohesion", "Relation", "Presence", "Warmth",
        "Memory", "Attraction", "Harm", "Distortion", "Flux", "Depth"
    };
    return (idx >= 0 && idx < NUM_PILLARS) ? names[idx] : "Unknown";
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
