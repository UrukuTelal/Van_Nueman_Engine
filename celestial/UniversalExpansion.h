#pragma once

#include "LargeScaleStructure.h"
#include <cmath>

struct CosmologicalConfig {
    float hubble_constant;
    float omega_matter;
    float omega_dark_energy;
    float scale_factor;
    float expansion_rate;
    float tick_interval;
    float structure_threshold;
    bool expanding;

    CosmologicalConfig()
        : hubble_constant(0.07f), omega_matter(0.3f),
          omega_dark_energy(0.7f), scale_factor(1.0f),
          expansion_rate(0.001f), tick_interval(1.0f),
          structure_threshold(0.4f), expanding(true) {}
};

inline float hubble_velocity(float distance, const CosmologicalConfig& config) {
    return config.hubble_constant * config.expansion_rate * distance;
}

inline float scale_factor_at_time(float t, const CosmologicalConfig& config) {
    if (config.expanding) {
        return config.scale_factor * (1.0f + config.expansion_rate * t);
    } else {
        return config.scale_factor * (1.0f - config.expansion_rate * t);
    }
}

inline void cosmological_expand(GalaxyCluster& cluster, const CosmologicalConfig& config, float dt) {
    float expansion = 1.0f + hubble_velocity(1.0f, config) * dt;
    cluster.position.i = static_cast<int32_t>(cluster.position.i * expansion);
    cluster.position.j = static_cast<int32_t>(cluster.position.j * expansion);
    cluster.position.k = static_cast<int32_t>(cluster.position.k * expansion);
    cluster.radius *= expansion;
    cluster.velocity_x *= (1.0f - 0.01f * dt);
    cluster.velocity_y *= (1.0f - 0.01f * dt);
    cluster.velocity_z *= (1.0f - 0.01f * dt);
}

inline void cosmological_tick(CosmicField& field, CosmologicalConfig& config, float dt) {
    config.scale_factor = scale_factor_at_time(dt, config);

    for (auto& cluster : field.clusters) {
        cluster.position.i += static_cast<int32_t>(cluster.velocity_x * dt);
        cluster.position.j += static_cast<int32_t>(cluster.velocity_y * dt);
        cluster.position.k += static_cast<int32_t>(cluster.velocity_z * dt);

        if (config.expanding) {
            cosmological_expand(cluster, config, dt);
        }

        cluster.pillar_warmth *= (1.0f - 0.005f * dt);
    }

    for (size_t i = 0; i < field.clusters.size(); i++) {
        for (size_t j = i + 1; j < field.clusters.size(); j++) {
            auto& a = field.clusters[i];
            auto& b = field.clusters[j];
            int32_t di = b.position.i - a.position.i;
            int32_t dj = b.position.j - a.position.j;
            int32_t dk = b.position.k - a.position.k;
            float dist_sq = static_cast<float>(di*di + dj*dj + dk*dk);
            if (dist_sq < 1.0f) continue;

            float G = (a.pillar_attraction + b.pillar_attraction) * 0.5f * 0.01f;
            float force = G * a.mass * b.mass / dist_sq;
            float inv_dist = 1.0f / sqrtf(dist_sq);
            float ax = force * di * inv_dist / a.mass;
            float ay = force * dj * inv_dist / a.mass;
            float az = force * dk * inv_dist / a.mass;
            a.velocity_x += ax * dt;
            a.velocity_y += ay * dt;
            a.velocity_z += az * dt;
            b.velocity_x -= ax * dt;
            b.velocity_y -= ay * dt;
            b.velocity_z -= az * dt;
        }
    }
}
