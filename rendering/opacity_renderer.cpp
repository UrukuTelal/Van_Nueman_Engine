#include "opacity_renderer.h"
#include <cmath>

EdgeInfo calculate_opacity(const float pillars[NUM_PILLARS], float px, float py, float pz, float cx, float cy, float cz, int voxel_material) {
    float dx = px - cx;
    float dy = py - cy;
    float dz = pz - cz;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    float edge_factor = fminf(dist / 10.0f, 1.0f);
    
    // Base opacity from Presence pillar (P8) - WHT decoded
    float base_opacity = pillars[PILLAR_PRESENCE];
    
    // Edge is more opaque (gradient: edge=0.8+0.2, internal=base_opacity)
    float edge_opacity = edge_factor * 0.8f + 0.2f;
    
    EdgeInfo result;
    result.edge_factor = edge_factor;
    result.opacity = (1.0f - edge_factor) * base_opacity + edge_factor * edge_opacity;
    return result;
}

void render_voxel_with_pillar_opacity(float px, float py, float pz, float size, const float pillars[NUM_PILLARS]) {
    float cx = px + size / 2;
    float cy = py + size / 2;
    float cz = pz + size / 2;
    EdgeInfo info = calculate_opacity(pillars, px, py, pz, cx, cy, cz, 0);
    
    // Render voxel with info.opacity (WHT pillar communication active)
}
