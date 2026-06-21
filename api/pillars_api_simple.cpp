// Simplified Pillar API for regular C++ compilation (non-CUDA fallback)
// Original: pillars_api.cpp (for vncc compiler)

#include <cstdint>
#include <cmath>
#include <PillarEnum.h>
#include "../vn/ScaledInt.h"

#ifndef VN_USE_CUDA  // CUDA provides real GPU versions of these functions

// Pillar indices
#define Awareness     0
#define Willpower     1
#define Force         2
#define Influence     3
#define Resistance    4
#define Integrity     5
#define Cohesion      6
#define Relation      7
#define Presence      8
#define Warmth        9
#define Memory        10
#define Attraction    11
#define Harm          12
#define Distortion    13
#define Flux          14
#define Depth         15

// 3D vector
struct float3 {
    float x, y, z;
    float3() : x(0), y(0), z(0) {}
    float3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    float3 operator+(const float3& o) const { return float3(x+o.x, y+o.y, z+o.z); }
    float3 operator*(float s) const { return float3(x*s, y*s, z*s); }
};

// Pillar State Vector
struct PillarStateVector {
    vn::fp20_t p[NumPillars];
};

// Entity
struct Entity {
    uint32_t id;
    uint32_t type;
    float3 position;
    float3 velocity;
    PillarStateVector pillars;
    PillarStateVector baseline;
    uint32_t alive;
    uint32_t server_id;
    float energy;
    float health;
};

// Server State
struct ServerState {
    uint32_t id;
    PillarStateVector pillars;
    uint32_t player_count;
    uint32_t entity_count;
    float tick_accumulator;
};

// Pillar API context
struct PillarsAPIContext {
    Entity* d_entities;
    ServerState* d_servers;
    uint32_t entity_count;
    uint32_t server_count;
};

// Initialize Pillar API
PillarsAPIContext* pillars_api_init(uint32_t max_entities) {
    PillarsAPIContext* ctx = new PillarsAPIContext();
    ctx->d_entities = new Entity[max_entities]();
    ctx->d_servers = nullptr;
    ctx->entity_count = 0;
    ctx->server_count = 0;
    return ctx;
}

// Reset simulation
void pillars_api_reset(PillarsAPIContext* ctx, uint32_t count) {
    if (!ctx) return;
    ctx->entity_count = count;
    for (uint32_t i = 0; i < count; i++) {
        ctx->d_entities[i].alive = 1;
        ctx->d_entities[i].id = i;
        // Initialize pillars to default values
        for (int p = 0; p < NumPillars; p++) {
            ctx->d_entities[i].pillars.p[p] = vn::fp20_t(0.5f); // Default center value
        }
    }
}

// ── Bloch Sphere Helpers ─────────────────────────────────────

inline float bloch_value_to_theta(float value) {
    float v = (value < 0.0f) ? 0.0f : (value > 1.0f) ? 1.0f : value;
    return std::acos(2.0f * v - 1.0f);
}

inline float bloch_theta_to_value(float theta) {
    float t = (theta < 0.0f) ? 0.0f : (theta > 3.14159265f) ? 3.14159265f : theta;
    return (std::cos(t) + 1.0f) * 0.5f;
}

// Apply a Bloch rotation: convert to theta, rotate, convert back
// Matches Entity.h apply_bloch_rotation exactly
inline float bloch_rotate(float current_value, float delta_theta) {
    float theta = bloch_value_to_theta(current_value);
    float t = theta + delta_theta;
    return bloch_theta_to_value(t);
}

// Update pillars — using Bloch rotation, not scalar arithmetic
void pillars_update(Entity* entities, uint32_t count, float dt) {
    const float PI = 3.14159265f;
    for (uint32_t idx = 0; idx < count; idx++) {
        if (!entities[idx].alive) continue;
        
        PillarStateVector& pillars = entities[idx].pillars;
        
        // Distortion twists Awareness phase, does not dampen its value
        float distortion = pillars.p[Distortion].to_float();
        if (distortion > 0.001f) {
            float theta = bloch_value_to_theta(pillars.p[Awareness]);
            float phi_twist = distortion * 0.001f * dt;  // gentle phase drift
            float z = std::cos(theta) * std::cos(phi_twist);
            pillars.p[Awareness] = vn::fp20_t((z + 1.0f) * 0.5f);
        }
        
        // Gentle Bloch drift: all pillars drift toward baseline via
        // small theta rotations rather than scalar interpolation
        for (int p = 0; p < NumPillars; p++) {
            float current = pillars.p[p].to_float();
            float baseline = (p == Harm) ? 0.1f : 0.5f;
            float theta = bloch_value_to_theta(current);
            float target_theta = bloch_value_to_theta(baseline);
            float diff = target_theta - theta;
            float drift = diff * 0.001f * dt;
            pillars.p[p] = vn::fp20_t(bloch_rotate(current, drift));
        }
    }
}

// Move entities
void pillars_move(Entity* entities, uint32_t count, float dt) {
    for (uint32_t idx = 0; idx < count; idx++) {
        if (!entities[idx].alive) continue;
        
        entities[idx].position.x += entities[idx].velocity.x * dt;
        entities[idx].position.y += entities[idx].velocity.y * dt;
        entities[idx].position.z += entities[idx].velocity.z * dt;
    }
}

// Main update entry point
void pillars_api_update(PillarsAPIContext* ctx, float dt) {
    if (!ctx || ctx->entity_count == 0) return;
    
    // Update pillars
    pillars_update(ctx->d_entities, ctx->entity_count, dt);
    
    // Move entities
    pillars_move(ctx->d_entities, ctx->entity_count, dt);
}

// Cleanup
void pillars_api_cleanup(PillarsAPIContext* ctx) {
    if (!ctx) return;
    delete[] ctx->d_entities;
    delete ctx;
}

#endif // VN_USE_CUDA
