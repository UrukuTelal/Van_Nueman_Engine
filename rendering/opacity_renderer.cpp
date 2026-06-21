#include "opacity_renderer.h"
#include <cmath>

EdgeInfo calculate_opacity(const float pillars[NumPillars], float px, float py, float pz, float cx, float cy, float cz, int voxel_material) {
    float dx = px - cx;
    float dy = py - cy;
    float dz = pz - cz;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    float edge_factor = fminf(dist / 10.0f, 1.0f);
    
    // Base opacity from Presence pillar (P8) - WHT decoded
    float base_opacity = pillars[Presence];
    
    // Edge is more opaque (gradient: edge=0.8+0.2, internal=base_opacity)
    float edge_opacity = edge_factor * 0.8f + 0.2f;
    
    EdgeInfo result;
    result.edge_factor = edge_factor;
    result.opacity = (1.0f - edge_factor) * base_opacity + edge_factor * edge_opacity;
    return result;
}

void render_voxel_with_pillar_opacity(float px, float py, float pz, float size, const float pillars[NumPillars]) {
    float cx = px + size / 2;
    float cy = py + size / 2;
    float cz = pz + size / 2;
    EdgeInfo info = calculate_opacity(pillars, px, py, pz, cx, cy, cz, 0);
    
    // Render voxel with info.opacity (WHT pillar communication active)
    (void)info;
}

void render_active_cell_mesh(const VoxelCell& cell,
                              const float agent_pillars[NumPillars],
                              float world_x, float world_y, float world_z,
                              bool use_dome) {
    VoxelMeshData mesh;
    generate_voxel_cell_mesh(cell, mesh, use_dome);

    // Compute world-space center for edge-opacity calculation
    float cx = world_x;
    float cy = world_y;
    float cz = world_z;

    // Apply pillar-derived opacity to each vertex
    for (uint32_t i = 0; i < mesh.vertex_count; i++) {
        // World position
        float wx = mesh.vertices[i].x + world_x;
        float wy = mesh.vertices[i].y + world_y;
        float wz = mesh.vertices[i].z + world_z;

        EdgeInfo info = calculate_opacity(agent_pillars, wx, wy, wz, cx, cy, cz, 0);

        // Blend existing vertex alpha with pillar-based opacity
        mesh.vertices[i].a = mesh.vertices[i].a * 0.5f + info.opacity * 0.5f;
    }

    // The mesh is now ready for Vulkan vertex buffer upload.
    // In a full implementation, mesh.vertices and mesh.indices
    // would be copied to a GPU buffer and issued as a draw call.
    (void)agent_pillars;
}
