// loading_screen.h - Loading screen header
#pragma once

#include <imgui.h>

struct LoadingState {
    bool is_loading = false;
    float progress = 0.0f;
    const char* current_task = "Initializing...";
    const char* status_text = "";
    int current_pillar = 0;
};

void render_loading_screen(LoadingState& state);
void update_loading_progress(LoadingState& state, float dt, const char* task);
