// deformation.cpp - Deformation system implementation

#include "deformation.h"
#include "../include/Entity.h"
#include "../include/TransformCore.h"
#include <cmath>
#include <cstdlib>
#include <vector>

struct DeformationState {
    bool initialized;
    uint32_t max_entities;
    std::vector<float> deformation_strength;
    std::vector<float> noise_phase;
};

static DeformationState s_deform = {false, 0, {}, {}};

static float simple_noise(float phase) {
    // Simple pseudo-random noise via sin
    float n = sinf(phase * 12.9898f + 78.233f) * 43758.5453f;
    return n - floorf(n);
}

void deformation_init(uint32_t max_entities) {
    s_deform.max_entities = max_entities;
    s_deform.deformation_strength.resize(max_entities, 0.0f);
    s_deform.noise_phase.resize(max_entities, 0.0f);
    for (uint32_t i = 0; i < max_entities; i++) {
        s_deform.noise_phase[i] = (float)(rand() % 1000) / 1000.0f * 6.2832f;
    }
    s_deform.initialized = true;
}

void apply_deformation(Entity* entities, uint32_t count, float dt) {
    if (!entities || !s_deform.initialized) return;

    for (uint32_t i = 0; i < count && i < s_deform.max_entities; i++) {
        Entity& ent = entities[i];
        if (ent.pillars[Integrity] <= 0.0f) continue;

        float harm = ent.pillars[Harm];
        float integrity = ent.pillars[Integrity];
        float willpower = ent.pillars[Willpower];

        float target_def = harm * (1.0f - integrity * 0.5f);
        float resistance = willpower * 0.3f;
        target_def = fmaxf(0.0f, target_def - resistance);

        float& current = s_deform.deformation_strength[i];
        current += (target_def - current) * fminf(1.0f, dt * 3.0f);
        if (current < 0.0f) current = 0.0f;

        s_deform.noise_phase[i] += dt * (1.0f + current * 2.0f);
    }
}

void fractal_deform(Entity* entity, float intensity) {
    if (!entity || !s_deform.initialized) return;

    float noise_val = simple_noise(s_deform.noise_phase[entity->id % s_deform.max_entities]);
    float scale = 1.0f + intensity * noise_val * 0.15f;
    float displacement = intensity * (noise_val - 0.5f) * 0.2f;

    entity->pos_x += cosf(s_deform.noise_phase[entity->id % s_deform.max_entities]) * displacement;
    entity->pos_y += sinf(s_deform.noise_phase[entity->id % s_deform.max_entities] * 1.3f) * displacement;
}

float calculate_stress(Entity* entity) {
    if (!entity) return 0.0f;

    vn::fp20_t harm(entity->pillars[Harm]);
    vn::fp20_t willpower(entity->pillars[Willpower]);
    vn::fp20_t depth(entity->pillars[Depth]);

    vn::fp20_t delta_theta = compute_angular_shift(harm, vn::fp20_t(1.0f), vn::fp20_t(0.5f), willpower, depth).shift_rad;
    float result = delta_theta.to_float();
    if (result > 1.0f) result = 1.0f;
    if (result < 0.0f) result = 0.0f;
    return result;
}
