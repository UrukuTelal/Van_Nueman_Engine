#pragma once

#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"
#include "../include/Entity.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>

struct LayerThresholds {
    float core_density;
    float mantle_density;
    float crust_density;
    float atmosphere_height;

    LayerThresholds() : core_density(2.5f), mantle_density(1.5f),
                        crust_density(0.8f), atmosphere_height(3.0f) {}
};

struct PlanetaryBody {
    BCCCoord center;
    float radius;
    float core_radius;
    float mantle_radius;
    float total_mass;
    LayerThresholds layers;

    std::vector<VoxelCell> core_cells;
    std::vector<VoxelCell> mantle_cells;
    std::vector<VoxelCell> crust_cells;
    std::vector<VoxelCell> atmosphere_cells;

    PlanetaryBody() : center({0,0,0,0}), radius(0), core_radius(0),
                      mantle_radius(0), total_mass(0) {}
};

inline float voxel_density(const VoxelCell& cell) {
    float cohesion = 0.0f, integrity = 0.0f, flux = 0.0f;
    for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
        cohesion += cell.pyramids[f].material_composition[Cohesion].to_float();
        integrity += cell.pyramids[f].material_composition[Integrity].to_float();
        flux += cell.pyramids[f].material_composition[Flux].to_float();
    }
    cohesion /= VoxelCell::FACE_COUNT;
    integrity /= VoxelCell::FACE_COUNT;
    flux /= VoxelCell::FACE_COUNT;
    return 0.5f + cohesion * 1.5f + integrity * 1.0f - flux * 0.3f;
}

inline void differentiate(std::vector<VoxelCell>& cells, PlanetaryBody& body) {
    if (cells.empty()) return;

    body.center = {0, 0, 0, 0};
    for (const auto& c : cells) {
        body.center.i += c.coord.i;
        body.center.j += c.coord.j;
        body.center.k += c.coord.k;
    }
    size_t n = cells.size();
    body.center.i /= static_cast<int32_t>(n);
    body.center.j /= static_cast<int32_t>(n);
    body.center.k /= static_cast<int32_t>(n);
    body.center.l = (body.center.i + body.center.j + body.center.k) & 1;

    body.core_cells.clear();
    body.mantle_cells.clear();
    body.crust_cells.clear();
    body.atmosphere_cells.clear();

    std::vector<std::pair<float, size_t>> density_idx;
    for (size_t i = 0; i < n; i++) {
        if (!cells[i].active) continue;
        density_idx.push_back({voxel_density(cells[i]), i});
    }
    std::sort(density_idx.begin(), density_idx.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    size_t core_count = n / 4;
    size_t mantle_count = n / 2;
    body.radius = powf(static_cast<float>(n), 1.0f / 3.0f) * 2.0f;
    body.core_radius = body.radius * 0.4f;
    body.mantle_radius = body.radius * 0.8f;

    for (size_t i = 0; i < density_idx.size(); i++) {
        size_t idx = density_idx[i].second;
        BCCCoord coord = cells[idx].coord;
        float dx = static_cast<float>(coord.i - body.center.i);
        float dy = static_cast<float>(coord.j - body.center.j);
        float dz = static_cast<float>(coord.k - body.center.k);
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);

        if (i < core_count) {
            BCCCoord new_pos;
            float theta = 2.0f * 3.14159f * static_cast<float>(i) / core_count;
            float phi = acosf(1.0f - 2.0f * static_cast<float>(i) / core_count);
            new_pos.i = body.center.i + static_cast<int32_t>(body.core_radius * 0.5f * sinf(phi) * cosf(theta));
            new_pos.j = body.center.j + static_cast<int32_t>(body.core_radius * 0.5f * sinf(phi) * sinf(theta));
            new_pos.k = body.center.k + static_cast<int32_t>(body.core_radius * 0.5f * cosf(phi));
            new_pos.l = (new_pos.i + new_pos.j + new_pos.k) & 1;
            cells[idx].coord = new_pos;
            body.core_cells.push_back(cells[idx]);
        } else if (i < core_count + mantle_count) {
            BCCCoord new_pos;
            float theta = 2.0f * 3.14159f * static_cast<float>(i - core_count) / mantle_count;
            float phi = acosf(1.0f - 2.0f * static_cast<float>(i - core_count) / mantle_count);
            float r = body.core_radius + (body.mantle_radius - body.core_radius) * 0.5f;
            new_pos.i = body.center.i + static_cast<int32_t>(r * sinf(phi) * cosf(theta));
            new_pos.j = body.center.j + static_cast<int32_t>(r * sinf(phi) * sinf(theta));
            new_pos.k = body.center.k + static_cast<int32_t>(r * cosf(phi));
            new_pos.l = (new_pos.i + new_pos.j + new_pos.k) & 1;
            cells[idx].coord = new_pos;
            body.mantle_cells.push_back(cells[idx]);
        } else {
            BCCCoord new_pos;
            float theta = 2.0f * 3.14159f * static_cast<float>(i - core_count - mantle_count) / (n - core_count - mantle_count);
            float phi = acosf(1.0f - 2.0f * static_cast<float>(i - core_count - mantle_count) / (n - core_count - mantle_count));
            new_pos.i = body.center.i + static_cast<int32_t>(body.mantle_radius * 0.9f * sinf(phi) * cosf(theta));
            new_pos.j = body.center.j + static_cast<int32_t>(body.mantle_radius * 0.9f * sinf(phi) * sinf(theta));
            new_pos.k = body.center.k + static_cast<int32_t>(body.mantle_radius * 0.9f * cosf(phi));
            new_pos.l = (new_pos.i + new_pos.j + new_pos.k) & 1;
            cells[idx].coord = new_pos;
            body.crust_cells.push_back(cells[idx]);
        }
    }

    body.total_mass = 0.0f;
    for (const auto& c : cells) {
        float cohesion = 0.0f;
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++)
            cohesion += c.pyramids[f].material_composition[Cohesion].to_float();
        body.total_mass += cohesion / VoxelCell::FACE_COUNT;
    }
}

inline void compute_planetary_structure(PlanetaryBody& body) {
    float core_density = 0.0f, mantle_density = 0.0f, crust_density = 0.0f;
    for (const auto& c : body.core_cells) core_density += voxel_density(c);
    for (const auto& c : body.mantle_cells) mantle_density += voxel_density(c);
    for (const auto& c : body.crust_cells) crust_density += voxel_density(c);
    if (!body.core_cells.empty()) core_density /= body.core_cells.size();
    if (!body.mantle_cells.empty()) mantle_density /= body.mantle_cells.size();
    if (!body.crust_cells.empty()) crust_density /= body.crust_cells.size();
    body.layers.core_density = core_density;
    body.layers.mantle_density = mantle_density;
    body.layers.crust_density = crust_density;
}

inline const char* layer_name(float density, const LayerThresholds& thresh) {
    if (density >= thresh.core_density) return "Core";
    if (density >= thresh.mantle_density) return "Mantle";
    if (density >= thresh.crust_density) return "Crust";
    return "Atmosphere";
}
