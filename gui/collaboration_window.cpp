#include "collaboration_window.h"
#include <imgui.h>
#include <ctime>
#include <cstdio>

void render_collaboration_window(std::vector<CollaborationMsg>& messages) {
    ImGui::SetNextWindowSize(ImVec2(500,400), ImGuiCond_FirstUseEver);
    ImGui::Begin("CrowsNest AI Collaboration");

    if(ImGui::BeginChild("ScrollRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        for(auto& msg : messages) {
            ImGui::TextColored(ImVec4(0.2f,0.8f,1.0f,1.0f), "%s", msg.agent.c_str());
            ImGui::SameLine();
            ImGui::Text("%s", msg.timestamp.c_str());
            ImGui::TextWrapped("%s", msg.message.c_str());
        }
    }

    ImGui::EndChild();
    ImGui::End();
}
