// Entity.h - Core entity type definitions for Van Nueman
// Pillar-constrained entity system with 16-dimension pillar state vector
// Entity ID = hash of 16D Pillar State Vector (same PSV = same entity)

#pragma once

#include <cstdint>
#include <cstring>
#include <array>
#include <cmath>
#include "../vn/PillarTypes.h"
#include "../scale/ScaleExponent.h"
#include "BlochMath.h"

// Unified PillarIndex enum from pillars.yaml (generated)
// Contains: Awareness, Willpower, Force, Influence, Resistance, Integrity,
// Cohesion, Relation, Presence, Warmth, Memory, Attraction, Harm, Distortion, Flux, Depth, NumPillars
#include "PillarEnum.h"

// Pillar vector type for LLM/agent systems (std430 compatible)
using PillarVector = std::array<float, NumPillars>;

// 16-dimensional pillar state vector (matches Taichi pillars_api.py)
struct PillarStateVector {
    float pillars[NumPillars];
    
    float& operator[](size_t i) { return pillars[i]; }
    const float& operator[](size_t i) const { return pillars[i]; }
    
    void fill(float value) {
        for (int i = 0; i < NumPillars; i++) pillars[i] = value;
    }
    
    // Compare two PSVs for functional equality (with tolerance)
    bool equals(const PillarStateVector& other, float tolerance = 0.001f) const {
        for (int i = 0; i < NumPillars; i++) {
            if (std::fabs(pillars[i] - other.pillars[i]) > tolerance) return false;
        }
        return true;
    }
};

// Compute entity ID from 16D Pillar State Vector
// Same PSV = same ID = functionally the same entity
// Uses FNV-1a 64-bit to eliminate collision risk at 500K entities
// (birthday paradox: 32-bit gives 50% collision at ~77K, 64-bit at ~5B)
using entity_id_t = uint64_t;

inline entity_id_t compute_entity_id(const PillarStateVector& psv) {
    // FNV-1a 64-bit hash of the 16 float values
    uint64_t hash = 14695981039346656037ULL;
    for (int i = 0; i < NumPillars; i++) {
        uint64_t bits;
        memcpy(&bits, &psv.pillars[i], sizeof(float));
        bits &= 0xFFFFFFFFULL;  // only 32 meaningful bits per float
        hash ^= bits;
        hash *= 1099511628211ULL;
    }
    return hash ? hash : 1ULL;
}

// Entity state flags
struct EntityFlags {
    uint32_t alive : 1;
    uint32_t is_fractured : 1;
    uint32_t is_severed : 1;
    uint32_t constrained : 1;
    uint32_t reserved : 28;
};

// Extended UID: UID = PSV(PID) + SID(SkellyID) + HID(History)
// All fields use entity_id_t (uint64_t) to eliminate collision risk.
struct EntityUID {
    entity_id_t psv_hash;  // from PSV(PID) - identity core
    entity_id_t sid;       // Skelly structure ID - anatomy
    entity_id_t hid;       // History hash - changes over time
    
    entity_id_t combined() const {
        return psv_hash ^ (sid * 0x9e3779b97f4a7c15ULL) ^ (hid * 0xbf58476d1ce4e5b9ULL);
    }
    
    bool operator==(const EntityUID& other) const {
        return psv_hash == other.psv_hash && 
               sid == other.sid && 
               hid == other.hid;
    }
};

// Core entity structure
struct Entity {
    entity_id_t id;
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
    
    // Constraint level (Bloch-space)
    vn::fp20_t constraint_level;
    
    // Grid cell cache for spatial partitioning
    int32_t grid_cell_x, grid_cell_y;
    
    // Entity type (for Skelly, physics, etc.)
    uint32_t type_id;
    uint32_t skelly_instance_id;  // 0 if not a skelly entity
    
    // Native scale (exponent for multi-scale interactions)
    ScaleExponent native_scale;

    // Resolve a pillar reference to its value in the authoritative PSV.
    // Treats the reference as a symbolic coordinate into the PSV array.
    // Returns 0.0f for invalid references (bounds-safe).
    float resolve(int pillar_idx) const {
        if (pillar_idx >= 0 && pillar_idx < NumPillars) {
            return pillars[pillar_idx];
        }
        return 0.0f;
    }

    // Mutable resolve for setting pillar values via symbolic reference
    float& resolve_ref(int pillar_idx) {
        static float fallback = 0.0f;
        if (pillar_idx >= 0 && pillar_idx < NumPillars) {
            return pillars[pillar_idx];
        }
        return fallback;
    }

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

inline PillarStateVector to_pillar_state(const float arr[NumPillars]) {
    PillarStateVector state;
    for (int i = 0; i < NumPillars; i++) state.pillars[i] = arr[i];
    return state;
}

inline float calculate_harm_delta(float incoming_damage, float resistance, float integrity) {
    // h_new = h_old + max(0, incoming_damage - Resistance - Integrity)
    float harm_increase = incoming_damage - resistance - integrity;
    return (harm_increase > 0.0f) ? harm_increase : 0.0f;
}

inline float calculate_effective_awareness(float awareness, float distortion) {
    return awareness * (1.0f - distortion);
}

// ── Bloch Sphere Operations ─────────────────────────────────
// Canonical implementation moved to BlochMath.h.
// These deprecated aliases remain for backward compatibility.
// New code should include BlochMath.h and use bloch_*() directly.

// ── Entity layout static_asserts (Issue #2) ──
// Verify that CPU entity struct is compatible with GPU SoA expectations.
// PillarStateVector must contain exactly NumPillars floats.
static_assert(sizeof(PillarStateVector) == sizeof(float) * NumPillars,
    "PillarStateVector layout mismatch: expected exactly NumPillars floats");
static_assert(offsetof(Entity, pillars) % alignof(float) == 0,
    "Entity::pillars must be float-aligned for GPU buffer mapping");

// Maximum entities constant
static constexpr uint32_t MAX_ENTITIES = 500000;
// SkellyInstance.h contains: static_assert(MAX_INSTANCES >= MAX_ENTITIES)
static constexpr uint32_t GRID_RES = 128;
static constexpr uint32_t MAX_PER_CELL = 32;
