// pillars_api.h - C API header for Pillar API
// Use this instead of including .cu files directly

#pragma once

#include <cstdint>
#include <cstddef>
#include "../include/Entity.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque context pointer
typedef struct PillarsAPIContextImpl* PillarsAPIContext;

// Pillar indices (defined in Entity.h)

// Entity structure (defined in Entity.h)
typedef struct Entity Entity;

// API functions
PillarsAPIContext pillars_api_init(uint32_t max_entities);
void pillars_api_cleanup(PillarsAPIContext ctx);

// Update simulation
void pillars_api_update(PillarsAPIContext ctx, float dt);

// Get entity data (for reading pillar states)
const Entity* pillars_api_get_entities(PillarsAPIContext ctx, uint32_t* count_out);
Entity* pillars_api_get_entities_mutable(PillarsAPIContext ctx);

// Calculate harm delta
float pillars_api_calculate_harm_delta(float incoming_damage, float resistance, float integrity);

// Get pillar state vector for an entity
void pillars_api_get_entity_pillars(PillarsAPIContext ctx, uint32_t entity_id, float out_pillars[NUM_PILLARS]);

// Set pillar value for an entity
void pillars_api_set_entity_pillar(PillarsAPIContext ctx, uint32_t entity_id, PillarIndex pillar, float value);

#ifdef __cplusplus
}
#endif
