#ifndef OPACITY_RENDERER_H
#define OPACITY_RENDERER_H

#include "../include/Entity.h"
#include "voxel_mesh_generator.h"
#include <cstdint>

struct EdgeInfo {
    float edge_factor;
    float opacity;
};

EdgeInfo calculate_opacity(const float pillars[NumPillars], float px, float py, float pz, float cx, float cy, float cz, int voxel_material);
void render_voxel_with_pillar_opacity(float px, float py, float pz, float size, const float pillars[NumPillars]);

// Render a truncated-octahedron active cell mesh using pillar-based opacity.
// Combines the mesh generator with pillar opacity calculation.
void render_active_cell_mesh(const VoxelCell& cell,
                             const float agent_pillars[NumPillars],
                             float world_x, float world_y, float world_z,
                             bool use_dome = false);

#endif // OPACITY_RENDERER_H
