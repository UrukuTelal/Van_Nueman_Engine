#ifndef OPACITY_RENDERER_H
#define OPACITY_RENDERER_H

#include "../include/Entity.h"

struct EdgeInfo {
    float edge_factor;
    float opacity;
};

EdgeInfo calculate_opacity(const float pillars[NUM_PILLARS], float px, float py, float pz, float cx, float cy, float cz, int voxel_material);
void render_voxel_with_pillar_opacity(float px, float py, float pz, float size, const float pillars[NUM_PILLARS]);

#endif // OPACITY_RENDERER_H
