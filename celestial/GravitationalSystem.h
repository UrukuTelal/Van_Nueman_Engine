#pragma once

#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"
#include "../include/Entity.h"
#include <vector>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <queue>

struct GravitySource {
    BCCCoord center;
    float total_mass;
    float attraction_strength;
    float influence_radius;

    GravitySource() : center({0,0,0,0}), total_mass(1.0f),
                      attraction_strength(1.0f), influence_radius(100.0f) {}
};

struct GravitationalForce {
    float fx, fy, fz;
    float magnitude;

    GravitationalForce() : fx(0), fy(0), fz(0), magnitude(0) {}
};

inline float voxel_mass(const VoxelCell& cell) {
    float mass = 0.0f;
    for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
        mass += cell.pyramids[f].material_composition[Cohesion].to_float() +
                cell.pyramids[f].material_composition[Integrity].to_float();
        mass += cell.pyramids[f].volume.to_float();
    }
    return mass / static_cast<float>(VoxelCell::FACE_COUNT);
}

inline float body_mass(const std::vector<VoxelCell>& cells) {
    float total = 0.0f;
    for (const auto& c : cells) total += voxel_mass(c);
    return total;
}

inline BCCCoord center_of_mass(const std::vector<VoxelCell>& cells) {
    BCCCoord com = {0, 0, 0, 0};
    float total_mass = 0.0f;
    for (const auto& c : cells) {
        float m = voxel_mass(c);
        com.i += static_cast<int32_t>(c.coord.i * m);
        com.j += static_cast<int32_t>(c.coord.j * m);
        com.k += static_cast<int32_t>(c.coord.k * m);
        total_mass += m;
    }
    if (total_mass > 0) {
        com.i = static_cast<int32_t>(com.i / total_mass);
        com.j = static_cast<int32_t>(com.j / total_mass);
        com.k = static_cast<int32_t>(com.k / total_mass);
        com.l = (com.i + com.j + com.k) & 1;
    }
    return com;
}

inline GravitationalForce gravitational_force(const VoxelCell& a, const VoxelCell& b) {
    GravitationalForce force;
    int32_t di = b.coord.i - a.coord.i;
    int32_t dj = b.coord.j - a.coord.j;
    int32_t dk = b.coord.k - a.coord.k;
    float dist_sq = static_cast<float>(di * di + dj * dj + dk * dk);
    if (dist_sq < 0.1f) return force;

    float dist = sqrtf(dist_sq);
    float ma = voxel_mass(a);
    float mb = voxel_mass(b);

    float attraction_a = 0.0f, attraction_b = 0.0f;
    for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
        attraction_a += a.pyramids[f].material_composition[Attraction].to_float();
        attraction_b += b.pyramids[f].material_composition[Attraction].to_float();
    }
    attraction_a /= VoxelCell::FACE_COUNT;
    attraction_b /= VoxelCell::FACE_COUNT;
    float G = (attraction_a + attraction_b) * 0.5f * 0.1f;

    force.magnitude = G * ma * mb / dist_sq;
    float inv_dist = 1.0f / dist;
    force.fx = force.magnitude * (static_cast<float>(di) * inv_dist);
    force.fy = force.magnitude * (static_cast<float>(dj) * inv_dist);
    force.fz = force.magnitude * (static_cast<float>(dk) * inv_dist);
    return force;
}

inline std::vector<std::vector<uint32_t>> aggregate_clusters(
    const std::vector<VoxelCell>& cells, float attraction_threshold = 0.4f) {
    size_t n = cells.size();
    std::vector<bool> visited(n, false);
    std::vector<std::vector<uint32_t>> clusters;

    for (size_t i = 0; i < n; i++) {
        if (visited[i] || !cells[i].active) continue;

        std::vector<uint32_t> cluster;
        std::queue<size_t> q;
        q.push(i);
        visited[i] = true;

        while (!q.empty()) {
            size_t idx = q.front(); q.pop();
            cluster.push_back(static_cast<uint32_t>(idx));

            for (size_t j = 0; j < n; j++) {
                if (visited[j] || !cells[j].active) continue;
                int32_t di = cells[idx].coord.i - cells[j].coord.i;
                int32_t dj = cells[idx].coord.j - cells[j].coord.j;
                int32_t dk = cells[idx].coord.k - cells[j].coord.k;
                float dist = sqrtf(static_cast<float>(di*di + dj*dj + dk*dk));
                if (dist > 3.0f) continue;

                float att = 0.0f;
                for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                    att += cells[j].pyramids[f].material_composition[Attraction].to_float();
                }
                att /= VoxelCell::FACE_COUNT;

                if (att > attraction_threshold) {
                    visited[j] = true;
                    q.push(j);
                }
            }
        }
        if (cluster.size() >= 2) clusters.push_back(cluster);
    }
    return clusters;
}

inline float orbital_velocity(float central_mass, float orbit_radius, float G = 0.1f) {
    if (orbit_radius < 0.1f) return 0.0f;
    return sqrtf(G * central_mass / orbit_radius);
}

inline float escape_velocity(float central_mass, float orbit_radius, float G = 0.1f) {
    if (orbit_radius < 0.1f) return 0.0f;
    return sqrtf(2.0f * G * central_mass / orbit_radius);
}

inline bool is_bound_orbit(float velocity, float central_mass,
                            float orbit_radius, float G = 0.1f) {
    float v_esc = escape_velocity(central_mass, orbit_radius, G);
    return velocity < v_esc;
}
