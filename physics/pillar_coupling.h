// pillar_coupling.h - Header for pillar coupling system

#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct Entity;  // from Entity.h
struct SkellyInstance;
struct MuscleGroup;

// Initialize pillar coupling system
void pillar_coupling_init(uint32_t max_entities, uint32_t max_muscle_groups);

// Update pillar-driven physics (call each tick)
void pillar_coupling_update(float dt);

// Couple pillar state to skelly muscles
void pillar_coupling_muscles(struct Entity* entities, uint32_t entity_count,
                              struct SkellyInstance* instances, uint32_t instance_count,
                              struct MuscleGroup* muscles, uint32_t muscle_count);

// Apply deformation based on pillar states
void pillar_coupling_deform(struct Entity* entities, uint32_t entity_count,
                            float dt);

// Get pillar state for an entity
void get_entity_pillars(uint32_t entity_id, float out_pillars[16]);

// Set pillar value for an entity
void set_entity_pillar(uint32_t entity_id, uint32_t pillar_idx, float value);

#ifdef __cplusplus
}
#endif
