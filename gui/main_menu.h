// main_menu.h - Main menu header
#pragma once

#include <imgui.h>
#include "../include/Entity.h"

struct MainMenuState {
    bool show_single_player = false;
    bool show_multiplayer = false;
    bool show_federation = false;
    bool show_server_config = false;
    bool show_settings = false;
    int selected_menu = 0;
};

void render_main_menu(MainMenuState& state, const float pillar_values[NumPillars]);
