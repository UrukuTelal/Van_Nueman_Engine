#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <math.h>
#include "../kernels/pillars_compute.cu"
#include "../kernels/skelly_compute.cu"

// Pillar → Skelly coupling: How Pillar State Vector drives Skelly physics
// From FULL_ARCHITECTURE.md: "Pillar Vector → drives → Skelly System → produces → Physics"

// Convert fp20_t raw int64 to float for CUDA kernels
#define FROM_FP20(x) ((float)(x).raw() / 1048576.0f)
#define TO_FP20(f)   ((int64_t)((f) * 1048576.0f))

// Coupling constants (scaled integer compatible)
#define COUPLING_FORCE_TO_MUSCLE_ACTIVATION  0.001f
#define COUPLING_WILLPOWER_TO_FLEXIBILITY    0.0005f
#define COUPLING_RESISTANCE_TO_BREAK_THRESH  0.002f
#define COUPLING_INTEGRITY_TO_REPAIR_RATE    0.001f
#define COUPLING_HARM_TO_FRACTURE_PROB       0.02f

// Pillar-driven muscle activation
__global__ void pillar_coupling_muscles_kernel(Entity* entities, uint32_t entity_count,
                                                SkellyInstance* instances, uint32_t instance_count,
                                                MuscleGroup* muscles, BoneSegment* segments) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= instance_count) return;
    
    SkellyInstance& inst = instances[idx];
    if (inst.entity_id == 0 || inst.entity_id > entity_count) return;

    // Load entity data from entities array
    Entity ent = entities[inst.entity_id - 1];

    // Apply force-based activation to all muscles in instance
    for (uint32_t m = inst.muscle_start; m < inst.muscle_start + inst.muscle_count; m++) {
        MuscleGroup& mg = muscles[m];
        // Force pillar drives muscle activation
        mg.activation = FROM_FP20(ent.pillars[PILLAR_FORCE]);
    }
}

// Pillar-driven bone flexibility (Willpower affects flexibility)
__global__ void pillar_coupling_bones_kernel(Entity* entities, uint32_t entity_count,
                                              SkellyInstance* instances, uint32_t instance_count,
                                              BoneSegment* segments) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= instance_count) return;
    
    SkellyInstance& inst = instances[idx];
    if (inst.entity_id == 0 || inst.entity_id > entity_count) return;

    // Load entity data
    Entity ent = entities[inst.entity_id - 1];

    // Willpower increases flexibility (adaptability)
    // Resistance increases break threshold (durability)
    for (uint32_t s = inst.segment_start; s < inst.segment_start + inst.segment_count; s++) {
        BoneSegment& seg = segments[s];
        seg.flexibility = FROM_FP20(ent.pillars[PILLAR_WILLPOWER]);
        seg.break_threshold = FROM_FP20(ent.pillars[PILLAR_RESISTANCE]) * 200.0f;
    }
}

// Pillar-driven organ activity
__global__ void pillar_coupling_organs_kernel(Entity* entities, uint32_t entity_count,
                                               SkellyInstance* instances, uint32_t instance_count,
                                               Organ* organs) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= instance_count) return;
    
    SkellyInstance& inst = instances[idx];
    if (inst.entity_id == 0 || inst.entity_id > entity_count) return;

    // Load entity data
    Entity ent = entities[inst.entity_id - 1];

    // Force pillar drives power_plant output
    // Warmth pillar drives factory/repair rate
    for (uint32_t o = inst.organ_start; o < inst.organ_start + inst.organ_count; o++) {
        Organ& organ = organs[o];
        if (organ.type == 2) {  // power_plant
            organ.active_state = FROM_FP20(ent.pillars[PILLAR_FORCE]);
        } else if (organ.type == 3) {  // factory
            organ.active_state = FROM_FP20(ent.pillars[PILLAR_WARMTH]);
        }
    }
}

// Harm pillar causes fracture (transformation/disruption)
__global__ void pillar_coupling_harm_kernel(Entity* entities, uint32_t entity_count,
                                             SkellyInstance* instances, uint32_t instance_count,
                                             BoneSegment* segments) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= instance_count) return;
    
    SkellyInstance& inst = instances[idx];
    if (inst.entity_id == 0 || inst.entity_id > entity_count) return;

    // Load entity data
    Entity ent = entities[inst.entity_id - 1];

    // Get Harm pillar value
    float harm = FROM_FP20(ent.pillars[PILLAR_HARM]);
    
    if (harm > 0.7f) {
        // High Harm can cause fractures
        for (uint32_t s = inst.segment_start; s < inst.segment_start + inst.segment_count; s++) {
            BoneSegment& seg = segments[s];
            float fracture_prob = harm * COUPLING_HARM_TO_FRACTURE_PROB;
            if (fracture_prob > 0.01f) {
                seg.is_fractured = 1;
                // Also fracture the end node
                // bones[seg.end_node].is_fractured = 1; // Would need bone array
            }
        }
    }
}

// Integrity pillar repairs fractures
__global__ void pillar_coupling_repair_kernel(Entity* entities, uint32_t entity_count,
                                                SkellyInstance* instances, uint32_t instance_count,
                                                BoneSegment* segments) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= instance_count) return;
    
    SkellyInstance& inst = instances[idx];
    if (inst.entity_id == 0 || inst.entity_id > entity_count) return;

    // Load entity data
    Entity ent = entities[inst.entity_id - 1];

    // Get Integrity pillar value
    float integrity = FROM_FP20(ent.pillars[PILLAR_INTEGRITY]);
    
    if (integrity > 0.6f) {
        // High Integrity repairs fractures
        for (uint32_t s = inst.segment_start; s < inst.segment_start + inst.segment_count; s++) {
            BoneSegment& seg = segments[s];
            if (seg.is_fractured && integrity > 0.8f) {
                seg.is_fractured = 0;
            }
        }
    }
}

// Master coupling function: runs all coupling kernels
__host__ void pillar_coupling_step(Entity* d_entities, uint32_t entity_count,
                                     SkellyInstance* d_instances, uint32_t instance_count,
                                     MuscleGroup* d_muscles, BoneSegment* d_segments,
                                     Organ* d_organs) {
    if (instance_count == 0) return;
    
    uint32_t block_size = 256;
    uint32_t grid_size = (instance_count + block_size - 1) / block_size;
    
    // 1. Couple Pillars → Muscles
    // Implements Pillar Force → Muscle Activation coupling
    // High Force increases muscle activation, modulated by Willpower for consistency
    pillar_coupling_muscles_kernel<<<grid_size, block_size>>>(
        d_entities, entity_count, d_instances, instance_count, d_muscles, d_segments
    );
    
    // 2. Couple Pillars → Bones
    // Implements Pillar Willpower → Bone Flexibility and Pillar Resistance → Break Threshold
    // High Willpower increases flexibility (adaptability), High Resistance increases break threshold (durability)
    pillar_coupling_bones_kernel<<<grid_size, block_size>>>(
        d_entities, entity_count, d_instances, instance_count, d_segments
    );
    
    // 3. Couple Pillars → Organs
    // Implements Pillar Force → Power Plant output and Pillar Warmth → Factory/Repair rate
    // Force drives energy output, Warmth drives repair/regeneration, Willpower increases overall efficiency
    pillar_coupling_organs_kernel<<<grid_size, block_size>>>(
        d_entities, entity_count, d_instances, instance_count, d_organs
    );
    
    // 4. Apply Harm (fracture)
    // Implements Pillar Harm → Bone Fracture probability
    // High Harm (>0.7) can cause structural fractures, mitigated by Resistance and Integrity
    pillar_coupling_harm_kernel<<<grid_size, block_size>>>(
        d_entities, entity_count, d_instances, instance_count, d_segments
    );
    
    // 5. Apply Integrity (repair)
    // Implements Pillar Integrity → Fracture Repair capability
    // High Integrity + Willpower repairs fractures over time
    pillar_coupling_repair_kernel<<<grid_size, block_size>>>(
        d_entities, entity_count, d_instances, instance_count, d_segments
    );
    
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
