// SkellyInstance.h - Instance structure for pillar→skelly coupling

#pragma once

#include <cstdint>

// Skelly instance (matches pillars_api.cpp definition)
struct SkellyInstance {
    uint32_t id;
    uint32_t entity_id;          // 0 = free slot
    uint32_t node_start;         // index into node array
    uint32_t node_count;
    uint32_t segment_start;      // index into segment array
    uint32_t segment_count;
    uint32_t muscle_start;       // index into muscle array
    uint32_t muscle_count;
    uint32_t organ_start;        // index into organ array
    uint32_t organ_count;
    float energy;
    float health;
    uint32_t alive;
};

// Maximum instances
static constexpr uint32_t MAX_INSTANCES = 100000;
