// deformation.h - Header for deformation system

#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

struct Entity;  // From Entity.h

// Initialize deformation system
void deformation_init(uint32_t max_entities);

// Apply deformation based on pillar states
void apply_deformation(struct Entity* entities, uint32_t count, float dt);

// Fractal-based deformation (GPU/CUDA version available in deformation.cu)
void fractal_deform(struct Entity* entity, float intensity);

// Calculate stress on entity
float calculate_stress(struct Entity* entity);

#ifdef __cplusplus
}
#endif
