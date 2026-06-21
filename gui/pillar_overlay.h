#ifndef VAN_NUEMAN_GUI_PILLAR_OVERLAY_H
#define VAN_NUEMAN_GUI_PILLAR_OVERLAY_H

#include <cstdint>
#include <array>
#include <string>
#include "imgui.h"
#include "../include/Entity.h"
#include "../include/PhotonicWrapper.h"

// Color display names for wrapper UI
static const char* WRAPPER_COLOR_NAMES[NUM_COLORS] = {
    "Red", "Yellow", "Green", "Cyan", "Blue", "Magenta"
};

static const ImVec4 WRAPPER_COLOR_VALUES[NUM_COLORS] = {
    ImVec4(0.94f, 0.27f, 0.27f, 1.0f),  // Red
    ImVec4(0.92f, 0.70f, 0.03f, 1.0f),  // Yellow
    ImVec4(0.13f, 0.77f, 0.37f, 1.0f),  // Green
    ImVec4(0.02f, 0.71f, 0.83f, 1.0f),  // Cyan
    ImVec4(0.23f, 0.51f, 0.96f, 1.0f),  // Blue
    ImVec4(0.93f, 0.27f, 0.60f, 1.0f),  // Magenta
};

class PillarOverlay {
public:
    PillarOverlay();
    ~PillarOverlay() = default;
    
    // Render the overlay
    void render(const PillarVector& pillars, float fps, int entity_count);
    
    // Render the photonic wrapper section
    void render_wrapper_section(const PhotonicWrapper& wrapper);
    
    // Set the active wrapper to display
    void set_active_wrapper(const PhotonicWrapper& w) { active_wrapper_ = w; has_wrapper_ = true; }
    void clear_wrapper() { has_wrapper_ = false; }
    
    // Toggle visibility
    void toggle() { visible_ = !visible_; }
    bool is_visible() const { return visible_; }
    
    // Set position/size
    void set_position(float x, float y) { pos_x_ = x; pos_y_ = y; }
    void set_size(float w, float h) { width_ = w; height_ = h; }
    
private:
    bool visible_;
    float pos_x_, pos_y_;
    float width_, height_;
    float alpha_;
    
    // Wrapper state
    bool has_wrapper_;
    PhotonicWrapper active_wrapper_;
    
    // Helper functions
    static const char* get_pillar_name(int idx);
    static ImVec4 get_pillar_color(float value);
    void render_pillar_bar(int idx, float value, const char* name);
};

#endif // VAN_NUEMAN_GUI_PILLAR_OVERLAY_H
