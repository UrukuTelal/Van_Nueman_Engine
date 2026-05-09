// Entity.h - Core entity type definitions for Van Nueman
// Pillar-constrained entity system with 16-dimension pillar state vector
// Entity ID = hash of 16D Pillar State Vector (same PSV = same entity)

#pragma once

#include <cstdint>
#include <cstring>
#include <array>

// Number of pillars in PCMSRM
#ifndef NUM_PILLARS
#define NUM_PILLARS 16
#endif

// Pillar indices (matches PillarAIColab indices)
enum PillarIndex : uint32_t {
    PILLAR_AWARENESS = 0,
    PILLAR_WILLPOWER = 1,
    PILLAR_FORCE = 2,
    PILLAR_INFLUENCE = 3,
    PILLAR_RESISTANCE = 4,
    PILLAR_INTEGRITY = 5,
    PILLAR_COHESION = 6,
    PILLAR_RELATION = 7,
    PILLAR_PRESENCE = 8,
    PILLAR_WARMTH = 9,
    PILLAR_MEMORY = 10,
    PILLAR_ATTRACTION = 11,
    PILLAR_HARM = 12,
    PILLAR_DISTORTION = 13,
    PILLAR_FLUX = 14,
    PILLAR_DEPTH = 15
};

// 16-dimensional pillar state vector (matches Taichi pillars_api.py)
struct PillarStateVector {
    float pillars[NUM_PILLARS];
    
    float& operator[](size_t i) { return pillars[i]; }
    const float& operator[](size_t i) const { return pillars[i]; }
    
    void fill(float value) {
        for (int i = 0; i < NUM_PILLARS; i++) pillars[i] = value;
    }
    
    // Compare two PSVs for functional equality (with tolerance)
    bool equals(const PillarStateVector& other, float tolerance = 0.001f) const {
        for (int i = 0; i < NUM_PILLARS; i++) {
            if (fabs(pillars[i] - other.pillars[i]) > tolerance) return false;
        }
        return true;
    }
};

// Compute entity ID from 16D Pillar State Vector
// Same PSV = same ID = functionally the same entity
inline uint32_t compute_entity_id(const PillarStateVector& psv) {
    // FNV-1a 32-bit hash of the 16 float values
    uint32_t hash = 2166136261u;
    for (int i = 0; i < NUM_PILLARS; i++) {
        // Treat float bits as uint32_t for hashing
        uint32_t bits;
        memcpy(&bits, &psv.pillars[i], sizeof(float));
        hash ^= bits;
        hash *= 16777619u;
    }
    // Ensure non-zero ID (0 reserved for invalid)
    return hash ? hash : 1u;
}

// Entity state flags
struct EntityFlags {
    uint32_t alive : 1;
    uint32_t is_fractured : 1;
    uint32_t is_severed : 1;
    uint32_t reserved : 29;
};

// Extended UID: UID = PSV(PID) + SID(SkellyID) + HID(History)
struct EntityUID {
    uint32_t psv_hash;    // from PSV(PID) - identity core
    uint32_t sid;         // Skelly structure ID - anatomy
    uint32_t hid;        // History hash - changes over time
    
    uint64_t combined() const { 
        return ((uint64_t)psv_hash << 32) | ((uint64_t)sid << 16) | hid;
    }
    
    bool operator==(const EntityUID& other) const {
        return psv_hash == other.psv_hash && 
               sid == other.sid && 
               hid == other.hid;
    }
};

// Core entity structure
struct Entity {
    uint32_t id;
    EntityFlags flags;
    EntityUID uid;
    
    // Position (2D for now, can extend to 3D)
    float pos_x, pos_y;
    float vel_x, vel_y;
    
    // Pillar state (current)
    PillarStateVector pillars;
    
    // Pillar baseline (initial/stable state) - defines entity identity
    PillarStateVector baseline_pillars;
    
    // Internal pillar state (computed)
    PillarStateVector internal_pillars;
    
    // Energy and health (constrained by Harm pillar)
    float energy;
    float health;
    
    // Grid cell cache for spatial partitioning
    int32_t grid_cell_x, grid_cell_y;
    
    // Entity type (for Skelly, physics, etc.)
    uint32_t type_id;
    uint32_t skelly_instance_id;  // 0 if not a skelly entity
    
    // Update ID from current baseline_pillars
    void update_id() {
        id = compute_entity_id(baseline_pillars);
    }
    
    // Check if this entity is functionally the same as another
    bool is_same_entity(const Entity& other, float tolerance = 0.001f) const {
        return baseline_pillars.equals(other.baseline_pillars, tolerance);
    }
};

// Entity type IDs
enum EntityType : uint32_t {
    ENTITY_TYPE_BASIC = 0,
    ENTITY_TYPE_SKELLY = 1,
    ENTITY_TYPE_MUSCLE_GROUP = 2,
    ENTITY_TYPE_ORGAN = 3,
    ENTITY_TYPE_TRANSPORT = 4
};

inline PillarStateVector create_default_pillar_state(float default_value = 0.5f) {
    PillarStateVector state;
    state.fill(default_value);
    return state;
}

inline PillarStateVector to_pillar_state(const float arr[NUM_PILLARS]) {
    PillarStateVector state;
    for (int i = 0; i < NUM_PILLARS; i++) state.pillars[i] = arr[i];
    return state;
}

inline float calculate_harm_delta(float incoming_damage, float resistance, float integrity) {
    // h_new = h_old + max(0, incoming_damage - Resistance - Integrity)
    float harm_increase = incoming_damage - resistance - integrity;
    return (harm_increase > 0.0f) ? harm_increase : 0.0f;
}

inline float calculate_effective_awareness(float awareness, float distortion) {
    // effective_awareness = Awareness * (1.0 - Distortion)
    return awareness * (1.0f - distortion);
}

// Maximum entities constant
static constexpr uint32_t MAX_ENTITIES = 500000;
static constexpr uint32_t GRID_RES = 128;
static constexpr uint32_t MAX_PER_CELL = 32;
