// splash_screen.h - Splash screen header
#pragma once

#include <imgui.h>

struct SplashState {
    bool show_splash = true;
    float progress = 0.0f;
    const char* loading_text = "Initializing Pillars...";
};

void render_splash_screen(SplashState& state, float dt);
