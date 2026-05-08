// Van Nueman Pillar Coupling - vncc C++ Version
// Compile: vncc -emit-spirv pillar_coupling.cpp -o pillar_coupling.spv
//          vncc -emit-llvm pillar_coupling.cpp -o pillar_coupling.bc

#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <random>

static std::mt19937 rng(std::random_device{}());

// Include proper headers
#include "../include/Entity.h"
#include "../include/SkellyInstance.h"
#include "../include/SkellyTypes.h"

// Pillar → Skelly coupling: How Pillar State Vector drives Skelly physics
// From FULL_ARCHITECTURE.md: "Pillar Vector → drives → Skelly System → produces → Physics"

// Coupling constants (scaled integer compatible)
#define COUPLING_FORCE_TO_MUSCLE_ACTIVATION  0.001f
#define COUPLING_WILLPOWER_TO_FLEXIBILITY    0.0005f
#define COUPLING_RESISTANCE_TO_BREAK_THRESH  0.002f
#define COUPLING_INTEGRITY_TO_REPAIR_RATE    0.001f
#define COUPLING_HARM_TO_FRACTURE_PROB       0.005f

// Flux (Index 14) and Depth (Index 15) pillar indices
#define PILLAR_FLUX 14
#define PILLAR_DEPTH 15

// Flux coupling: high flux (>0.7) increases Perceive step frequency
#define COUPLING_FLUX_TO_PERCEIVE_FREQ 2.0f
// Depth threshold for shallow modifications
#define DEPTH_SHALLOW_THRESHOLD 0.3f

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
        
        // Get entity pillars
        Entity& ent = entities[inst.entity_id - 1];
        float force = ent.pillars[PILLAR_FORCE];
        float willpower = ent.pillars[PILLAR_WILLPOWER];
        
        // Force pillar drives muscle activation
        for (uint32_t m = inst.muscle_start; m < inst.muscle_start + inst.muscle_count; m++) {
            MuscleGroup& mg = muscles[m];
            mg.activation = force * COUPLING_FORCE_TO_MUSCLE_ACTIVATION;
            // Willpower increases activation consistency
            mg.activation *= (0.5f + willpower * 0.5f);
        }
    }
}

// Pillar-driven bone flexibility (Willpower affects flexibility) [[fractal]]
void pillar_coupling_bones(Entity* entities, uint32_t entity_count,
                            SkellyInstance* instances, uint32_t instance_count,
                            BoneSegment* segments) FRACTAL {
    for (uint32_t idx = 0; idx < instance_count; idx++) {
        SkellyInstance& inst = instances[idx];
        if (inst.entity_id == 0 || !inst.alive) continue;
        
        Entity& ent = entities[inst.entity_id - 1];
        float willpower = ent.pillars[PILLAR_WILLPOWER];
        float resistance = ent.pillars[PILLAR_RESISTANCE];
        float integrity = ent.pillars[PILLAR_INTEGRITY];
        
        // Willpower increases flexibility (adaptability)
        // Resistance increases break threshold (durability)
        // Integrity provides baseline stability
        for (uint32_t s = inst.segment_start; s < inst.segment_start + inst.segment_count; s++) {
            BoneSegment& seg = segments[s];
            seg.flexibility = 0.05f + willpower * COUPLING_WILLPOWER_TO_FLEXIBILITY;
            seg.break_threshold = 50.0f + resistance * COUPLING_RESISTANCE_TO_BREAK_THRESH;
            // Integrity helps maintain structural stability
            if (integrity > 0.7f) seg.flexibility *= 0.8f; // More rigid when high integrity
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
        float force = ent.pillars[PILLAR_FORCE];
        float warmth = ent.pillars[PILLAR_WARMTH];
        float willpower = ent.pillars[PILLAR_WILLPOWER];
        
        // Force pillar drives power_plant output
        // Warmth pillar drives factory/repair rate
        // Willpower increases overall organ efficiency
        float efficiency = 0.5f + willpower * 0.5f;
        
        for (uint32_t o = inst.organ_start; o < inst.organ_start + inst.organ_count; o++) {
            Organ& organ = organs[o];
            if (organ.type == 2) {  // power_plant
                organ.active_state = force * efficiency;
            } else if (organ.type == 3) {  // factory
                organ.active_state = warmth * efficiency * 0.8f;
            } else if (organ.type == 0) {  // pump
                organ.active_state = force * 0.6f;
            } else if (organ.type == 1) {  // valve
                organ.active_state = force * 0.4f;
            }
        }
    }
}

// Harm pillar causes fracture (transformation/disruption) [[fractal]]
void pillar_coupling_harm(Entity* entities, uint32_t entity_count,
                           SkellyInstance* instances, uint32_t instance_count,
                           BoneSegment* segments) FRACTAL {
    for (uint32_t idx = 0; idx < instance_count; idx++) {
        SkellyInstance& inst = instances[idx];
        if (inst.entity_id == 0 || !inst.alive) continue;
        
        Entity& ent = entities[inst.entity_id - 1];
        float harm = ent.pillars[PILLAR_HARM];
        float resistance = ent.pillars[PILLAR_RESISTANCE];
        float integrity = ent.pillars[PILLAR_INTEGRITY];
        
        // Effective harm after resistance and integrity
        float effective_harm = harm - resistance * 0.5f - integrity * 0.3f;
        if (effective_harm < 0) effective_harm = 0;
        
        if (effective_harm > 0.7f) {
            // High Harm can cause fractures
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            for (uint32_t s = inst.segment_start; s < inst.segment_start + inst.segment_count; s++) {
                BoneSegment& seg = segments[s];
                float fracture_prob = effective_harm * COUPLING_HARM_TO_FRACTURE_PROB;
                float rand_val = dist(rng);
                if (fracture_prob > rand_val) {
                    seg.is_fractured = 1;
                }
            }
        }
    }
}

// Integrity pillar repairs fractures [[fractal]]
void pillar_coupling_repair(Entity* entities, uint32_t entity_count,
                              SkellyInstance* instances, uint32_t instance_count,
                              BoneSegment* segments) FRACTAL {
    for (uint32_t idx = 0; idx < instance_count; idx++) {
        SkellyInstance& inst = instances[idx];
        if (inst.entity_id == 0 || !inst.alive) continue;
        
        Entity& ent = entities[inst.entity_id - 1];
        float integrity = ent.pillars[PILLAR_INTEGRITY];
        float willpower = ent.pillars[PILLAR_WILLPOWER];
        
        // High Integrity + Willpower repairs fractures
        float repair_capability = integrity * 0.7f + willpower * 0.3f;
        
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
void pillar_coupling_step(Entity* d_entities, uint32_t entity_count,
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
            pillar_coupling_step(entities, entity_count, instances, instance_count,
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
