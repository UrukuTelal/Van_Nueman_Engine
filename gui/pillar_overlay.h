#ifndef VAN_NUEMAN_GUI_PILLAR_OVERLAY_H
#define VAN_NUEMAN_GUI_PILLAR_OVERLAY_H

#include <cstdint>
#include <array>
#include <string>
#include "imgui.h"
#include "../include/Entity.h"

class PillarOverlay {
public:
    PillarOverlay();
    ~PillarOverlay() = default;
    
    // Render the overlay
    void render(const std::array<float, NUM_PILLARS>& pillars, float fps, int entity_count);
    
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
    
    // Helper functions
    static const char* get_pillar_name(int idx);
    static ImVec4 get_pillar_color(float value);
    void render_pillar_bar(int idx, float value, const char* name);
};

#endif // VAN_NUEMAN_GUI_PILLAR_OVERLAY_H
