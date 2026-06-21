// Van Nueman Pillar Coupling - vncc C++ Version
// Compile: vncc -emit-spirv pillar_coupling.cpp -o pillar_coupling.spv
//          vncc -emit-llvm pillar_coupling.cpp -o pillar_coupling.bc

#include <cstdint>
#include <cmath>
#include <cstdlib>

// GPU-compatible deterministic PRNG (xorshift32) — no stdlib dependency.
// Uses entity index + segment index as seed, deterministic per-instance.
inline float deterministic_rand(uint32_t seed) {
    seed ^= seed >> 16;
    seed *= 0x85ebca6b;
    seed ^= seed >> 13;
    seed *= 0xc2b2ae35;
    seed ^= seed >> 16;
    return (float)(seed & 0x00FFFFFF) / 16777216.0f;
}

// Include proper headers
#include "../include/Entity.h"
#include "../include/SkellyInstance.h"
#include "../include/SkellyTypes.h"
#include "../scale/SemanticProjection.h"

// Pillar → Skelly coupling: Layer 1 biomechanical projection from substrate operators
// All semantic mappings go through SemanticProjection — see Section XIII.

// Coupling constants (scaled integer compatible)
#define COUPLING_FORCE_TO_MUSCLE_ACTIVATION  0.001f
#define COUPLING_WILLPOWER_TO_FLEXIBILITY    0.0005f
#define COUPLING_RESISTANCE_TO_BREAK_THRESH  0.002f
#define COUPLING_INTEGRITY_TO_REPAIR_RATE    0.001f
#define COUPLING_HARM_TO_FRACTURE_PROB       0.02f


// Pillar-driven muscle activation [[fractal]]
#if defined(__GNUC__) || defined(__clang__)
#define FRACTAL __attribute__((annotate("fractal")))
#else
#define FRACTAL
#endif

void pillar_coupling_muscles(Entity* entities, uint32_t entity_count,
                               SkellyInstance* instances, uint32_t instance_count,
                               MuscleGroup* muscles, BoneSegment* segments) FRACTAL {
    for (uint32_t idx = 0; idx < instance_count; idx++) {
        SkellyInstance& inst = instances[idx];
        if (inst.entity_id == 0 || !inst.alive) continue;
        
        Entity& ent = entities[inst.entity_id - 1];
        BiologicalProjection bp = BiologicalProjection::project(ent.pillars);
        
        // Force output (Force projection) drives muscle activation
        // Willpower stamina ensures activation consistency
        for (uint32_t m = inst.muscle_start; m < inst.muscle_start + inst.muscle_count; m++) {
            MuscleGroup& mg = muscles[m];
            mg.activation = bp.force_output * COUPLING_FORCE_TO_MUSCLE_ACTIVATION;
            mg.activation *= (0.5f + bp.willpower_stamina * 0.5f);
        }
    }
}

// Pillar-driven bone flexibility [[fractal]]
void pillar_coupling_bones(Entity* entities, uint32_t entity_count,
                            SkellyInstance* instances, uint32_t instance_count,
                            BoneSegment* segments) FRACTAL {
    for (uint32_t idx = 0; idx < instance_count; idx++) {
        SkellyInstance& inst = instances[idx];
        if (inst.entity_id == 0 || !inst.alive) continue;
        
        Entity& ent = entities[inst.entity_id - 1];
        BiologicalProjection bp = BiologicalProjection::project(ent.pillars);
        
        // Willpower stamina → flexibility (adaptability)
        // Membrane integrity → structural stability
        for (uint32_t s = inst.segment_start; s < inst.segment_start + inst.segment_count; s++) {
            BoneSegment& seg = segments[s];
            seg.flexibility = 0.05f + bp.willpower_stamina * COUPLING_WILLPOWER_TO_FLEXIBILITY;
            seg.break_threshold = 50.0f + (1.0f - bp.stress_level) * COUPLING_RESISTANCE_TO_BREAK_THRESH;
            if (bp.membrane_integrity > 0.7f) seg.flexibility *= 0.8f;
        }
    }
}

// Pillar-driven organ activity [[fractal]]
void pillar_coupling_organs(Entity* entities, uint32_t entity_count,
                             SkellyInstance* instances, uint32_t instance_count,
                             Organ* organs) FRACTAL {
    for (uint32_t idx = 0; idx < instance_count; idx++) {
        SkellyInstance& inst = instances[idx];
        if (inst.entity_id == 0 || !inst.alive) continue;
        
        Entity& ent = entities[inst.entity_id - 1];
        BiologicalProjection bp = BiologicalProjection::project(ent.pillars);
        float efficiency = 0.5f + bp.willpower_stamina * 0.5f;
        
        // force_output → power plant, metabolic_rate → factory/repair
        for (uint32_t o = inst.organ_start; o < inst.organ_start + inst.organ_count; o++) {
            Organ& organ = organs[o];
            if (organ.type == 2) {  // power_plant
                organ.active_state = bp.force_output * efficiency;
            } else if (organ.type == 3) {  // factory
                organ.active_state = bp.metabolic_rate * efficiency * 0.8f;
            } else if (organ.type == 0) {  // pump
                organ.active_state = bp.force_output * 0.6f;
            } else if (organ.type == 1) {  // valve
                organ.active_state = bp.force_output * 0.4f;
            }
        }
    }
}

// Harm pillar → fracture (BiologicalProjection::stress_level) [[fractal]]
// Integrity is already a Bloch-rotated value from prior TRANSFORM operations.
// Low membrane_integrity = rotated toward fracture pole.
void pillar_coupling_harm(Entity* entities, uint32_t entity_count,
                           SkellyInstance* instances, uint32_t instance_count,
                           BoneSegment* segments) FRACTAL {
    for (uint32_t idx = 0; idx < instance_count; idx++) {
        SkellyInstance& inst = instances[idx];
        if (inst.entity_id == 0 || !inst.alive) continue;
        
        Entity& ent = entities[inst.entity_id - 1];
        BiologicalProjection bp = BiologicalProjection::project(ent.pillars);
        
        // stress_level + low membrane_integrity → fracture probability
        float fracture_base = 1.0f - bp.membrane_integrity;
        float stress_mod = 0.5f + bp.stress_level * 0.5f;
        float fracture_probability = fracture_base * stress_mod * COUPLING_HARM_TO_FRACTURE_PROB;
        
        if (fracture_probability > 1e-6f) {
            for (uint32_t s = inst.segment_start; s < inst.segment_start + inst.segment_count; s++) {
                BoneSegment& seg = segments[s];
                float rand_val = deterministic_rand(inst.entity_id * 2654435761u + s);
                if (fracture_probability > rand_val) {
                    seg.is_fractured = 1;
                }
            }
        }
    }
}

// Membrane integrity + willpower stamina repairs fractures [[fractal]]
void pillar_coupling_repair(Entity* entities, uint32_t entity_count,
                              SkellyInstance* instances, uint32_t instance_count,
                              BoneSegment* segments) FRACTAL {
    for (uint32_t idx = 0; idx < instance_count; idx++) {
        SkellyInstance& inst = instances[idx];
        if (inst.entity_id == 0 || !inst.alive) continue;
        
        Entity& ent = entities[inst.entity_id - 1];
        BiologicalProjection bp = BiologicalProjection::project(ent.pillars);
        
        // membrane_integrity + willpower_stamina → repair capability
        float repair_capability = bp.membrane_integrity * 0.7f + bp.willpower_stamina * 0.3f;
        
        if (repair_capability > 0.6f) {
            for (uint32_t s = inst.segment_start; s < inst.segment_start + inst.segment_count; s++) {
                BoneSegment& seg = segments[s];
                if (seg.is_fractured && repair_capability > 0.8f) {
                    seg.is_fractured = 0;
                }
            }
        }
    }
}

// Master coupling function: runs all coupling steps
void pillar_coupling_step(float dt, Entity* d_entities, uint32_t entity_count,
                            SkellyInstance* d_instances, uint32_t instance_count,
                            MuscleGroup* d_muscles, BoneSegment* d_segments,
                            Organ* d_organs) {
    if (instance_count == 0) return;

    // 1. Couple Pillars → Muscles
    // Implements Pillar Force → Muscle Activation coupling
    // High Force increases muscle activation, modulated by Willpower for consistency
    pillar_coupling_muscles(d_entities, entity_count, d_instances, instance_count, d_muscles, d_segments);

    // 2. Couple Pillars → Bones
    // Implements Pillar Willpower → Bone Flexibility and Pillar Resistance → Break Threshold
    // High Willpower increases flexibility (adaptability), High Resistance increases break threshold (durability)
    pillar_coupling_bones(d_entities, entity_count, d_instances, instance_count, d_segments);

    // 3. Couple Pillars → Organs
    // Implements Pillar Force → Power Plant output and Pillar Warmth → Factory/Repair rate
    // Force drives energy output, Warmth drives repair/regeneration, Willpower increases overall efficiency
    pillar_coupling_organs(d_entities, entity_count, d_instances, instance_count, d_organs);

    // 4. Apply Harm (fracture)
    // Implements Pillar Harm → Bone Fracture probability
    // High Harm (>0.7) can cause structural fractures, mitigated by Resistance and Integrity
    pillar_coupling_harm(d_entities, entity_count, d_instances, instance_count, d_segments);

    // 5. Apply Integrity (repair)
    // Implements Pillar Integrity → Fracture Repair capability
    // High Integrity + Willpower repairs fractures over time
    pillar_coupling_repair(d_entities, entity_count, d_instances, instance_count, d_segments);

    // 6. Apply Flux coupling: System volatility monitor
    // Flux (Index 14) represents the rate of energy/information flow through the system
    // High flux (>0.7) increases perception frequency and system responsiveness
    // Flux effects are applied in the main simulation loop where perception frequency is adjusted
    // This implements the PCMSRM constraint: "Monitor system volatility. High flux requires more frequent Perceive steps."

    // 7. Apply Depth coupling: Recursive self-reference depth
    // Depth (Index 15) represents the level of meta-cognitive processing and self-reflection
    // Low depth (<0.3) indicates shallow processing which increases distortion vulnerability
    // High depth enables more thoughtful, resilient state changes that resist distortion
    // Depth effects are applied in pillar validation and modification functions
    // This implements the PCMSRM constraint: "Ensure changes have meaningful impact. Avoid shallow modifications that increase Distortion."
}

// Static state for pillar coupling system
static uint32_t g_max_entities = 0;
static uint32_t g_max_muscle_groups = 0;

void pillar_coupling_init(uint32_t max_entities, uint32_t max_muscle_groups) {
    g_max_entities = max_entities;
    g_max_muscle_groups = max_muscle_groups;
}

void pillar_coupling_update(float dt) {
    // Scale coupling constants by dt to make them frame-rate independent
    // Without dt scaling, the coupling would be stronger at higher frame rates
    (void)dt;
}

// Main entry point
extern "C" void pillar_coupling_main(
    Entity* entities,
    uint32_t entity_count,
    SkellyInstance* instances,
    uint32_t instance_count,
    MuscleGroup* muscles,
    BoneSegment* segments,
    Organ* organs,
    int mode  // 0 = full step, 1-5 = individual coupling steps
) {
    switch (mode) {
        case 0:
            pillar_coupling_step(0.016f, entities, entity_count, instances, instance_count,
                                muscles, segments, organs);
            break;
        case 1:
            pillar_coupling_muscles(entities, entity_count, instances, instance_count, muscles, segments);
            break;
        case 2:
            pillar_coupling_bones(entities, entity_count, instances, instance_count, segments);
            break;
        case 3:
            pillar_coupling_organs(entities, entity_count, instances, instance_count, organs);
            break;
        case 4:
            pillar_coupling_harm(entities, entity_count, instances, instance_count, segments);
            break;
        case 5:
            pillar_coupling_repair(entities, entity_count, instances, instance_count, segments);
            break;
    }
}
