#pragma once

#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"
#include "../include/Entity.h"
#include <vector>
#include <cmath>
#include <cstdint>

enum class CosmicStructure : uint8_t {
    Void = 0,
    Filament = 1,
    Sheet = 2,
    Cluster = 3,
    Supercluster = 4
};

inline const char* cosmic_structure_name(CosmicStructure s) {
    switch (s) {
        case CosmicStructure::Void: return "Void";
        case CosmicStructure::Filament: return "Filament";
        case CosmicStructure::Sheet: return "Sheet";
        case CosmicStructure::Cluster: return "Cluster";
        case CosmicStructure::Supercluster: return "Supercluster";
        default: return "Unknown";
    }
}

struct GalaxyCluster {
    BCCCoord position;
    float mass;
    float radius;
    float velocity_x, velocity_y, velocity_z;
    float pillar_attraction;
    float pillar_warmth;
    uint32_t num_galaxies;

    GalaxyCluster()
        : position({0,0,0,0}), mass(0), radius(0),
          velocity_x(0), velocity_y(0), velocity_z(0),
          pillar_attraction(0), pillar_warmth(0), num_galaxies(0) {}
};

struct CosmicField {
    std::vector<GalaxyCluster> clusters;
    std::vector<CosmicStructure> structure_map;
    float field_resolution;
    int grid_dim_x, grid_dim_y, grid_dim_z;

    CosmicField() : field_resolution(10.0f),
                    grid_dim_x(0), grid_dim_y(0), grid_dim_z(0) {}
};

inline CosmicStructure classify_region_cosmic(float local_density, float neighbor_density_avg) {
    if (local_density < neighbor_density_avg * 0.3f) return CosmicStructure::Void;
    if (local_density > neighbor_density_avg * 1.5f) return CosmicStructure::Cluster;
    if (local_density > neighbor_density_avg * 1.2f) return CosmicStructure::Sheet;
    return CosmicStructure::Filament;
}

inline void detect_large_scale_structure(CosmicField& field) {
    size_t n = field.clusters.size();
    if (n == 0) return;

    field.structure_map.resize(n);
    std::vector<float> densities(n);
    float avg_density = 0.0f;

    for (size_t i = 0; i < n; i++) {
        float r = field.clusters[i].radius;
        float density = field.clusters[i].mass / (4.0f / 3.0f * 3.14159f * r * r * r + 1.0f);
        densities[i] = density;
        avg_density += density;
    }
    avg_density /= n;

    for (size_t i = 0; i < n; i++) {
        float neighbor_density = 0.0f;
        int neighbor_count = 0;
        for (size_t j = 0; j < n; j++) {
            if (i == j) continue;
            int32_t di = field.clusters[j].position.i - field.clusters[i].position.i;
            int32_t dj = field.clusters[j].position.j - field.clusters[i].position.j;
            int32_t dk = field.clusters[j].position.k - field.clusters[i].position.k;
            float dist = sqrtf(static_cast<float>(di*di + dj*dj + dk*dk));
            if (dist < field.field_resolution * 5.0f) {
                neighbor_density += densities[j];
                neighbor_count++;
            }
        }
        float avg_neighbor = neighbor_count > 0 ? neighbor_density / neighbor_count : avg_density;
        field.structure_map[i] = classify_region_cosmic(densities[i], avg_neighbor);
    }
}
