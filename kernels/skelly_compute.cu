#include "skelly_shared.cuh"

// Update bone global positions (recursive, done in kernel)
__global__ void skelly_update_bones_kernel(BoneNode* bones, uint32_t count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    // Walk up hierarchy to find root, then compute positions root-to-leaf
    // This ensures each thread computes all ancestors before its own bone,
    // avoiding cross-thread races on parent->global_pos reads.
    uint32_t chain[64];
    int depth = 0;
    uint32_t cur = idx;
    while (cur != (uint32_t)-1 && cur < count && depth < 64) {
        chain[depth++] = cur;
        cur = bones[cur].parent_idx;
    }

    // Compute from root (last in chain) down to current (first in chain)
    for (int i = depth - 1; i >= 0; --i) {
        uint32_t b = chain[i];
        if (bones[b].parent_idx == (uint32_t)-1 || bones[b].is_fractured) {
            bones[b].global_pos = bones[b].local_pos;
        } else {
            bones[b].global_pos = bones[bones[b].parent_idx].global_pos + bones[b].local_pos;
        }
    }
}

// Muscle volume update (expansion/contraction based on activation)
__global__ void skelly_update_muscles_kernel(MuscleGroup* muscles, MuscleStrand* strands,
                                              BoneNode* bones, uint32_t count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    MuscleGroup& mg = muscles[idx];
    BoneNode& origin = bones[mg.origin_node];
    BoneNode& insertion = bones[mg.insertion_node];

    float curr_len = length(insertion.global_pos - origin.global_pos);
    float rest_len = curr_len;  // Would store rest length separately

    float ratio = (rest_len > 1e-8f && curr_len > 1e-8f) ? fminf(rest_len / curr_len, 1e8f) : 1.0f;
    float expansion = sqrtf(ratio);

    // Update all strands
    for (uint32_t s = 0; s < mg.strand_count; s++) {
        MuscleStrand& strand = strands[mg.strand_start + s];
        strand.current_r = strand.base_r * expansion * (1.0f + mg.activation);
    }

    // Calculate new volume
    mg.current_volume = 0.0f;
    for (uint32_t s = 0; s < mg.strand_count; s++) {
        MuscleStrand& strand = strands[mg.strand_start + s];
        mg.current_volume += SK_PI * strand.current_r * strand.current_r * curr_len;
    }
}

// Organ operation (pump, valve, power_plant, factory)
__global__ void skelly_operate_organs_kernel(Organ* organs, uint32_t count,
                                              Transport* transports, PillarStateVector* entity_pillars) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    (void)entity_pillars;
    Organ& organ = organs[idx];

    if (organ.type == 2) {  // power_plant
        organ.energy_output = organ.volume * organ.active_state * 5.0f;
    } else if (organ.type == 3) {  // factory
        organ.energy_output = organ.volume * organ.active_state * 1.2f;
    } else if (organ.type == 0 && organ.segment_idx < MAX_TRANSPORTS) {  // pump
        Transport& t = transports[organ.segment_idx];  // simplified: segment_idx maps to transport
        t.flow_rate += (organ.volume * organ.active_state) / (t.resistance + 1e-6f);
    } else if (organ.type == 1) {  // valve
        if (organ.segment_idx < MAX_TRANSPORTS) {
            Transport& t = transports[organ.segment_idx];
            t.resistance = 1.0f + (organ.active_state * 10.0f);
        }
    }

    organ.active_state *= 0.99f;  // decay
}

// Interstitial fluid turgor calculation
__global__ void skelly_turgor_kernel(InterstitialFluid* fluids, uint32_t fluid_count,
                                      Transport* transports, MuscleGroup* muscles,
                                      uint32_t muscle_count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= fluid_count) return;

    InterstitialFluid& fluid = fluids[idx];
    Transport& t = transports[fluid.transport_idx];

    // Calculate occupancy from muscles attached to this segment
    float occ = 0.0f;
    // Simplified: would iterate muscles attached to segment
    // For now, just use muscle_count as proxy
    occ = (float)muscle_count * 0.1f;

    fluid.turgor_pressure = 1.0f + (occ * 1.5f);
    t.pressure = fluid.turgor_pressure;

    // Update segment flexibility based on pressure
    // (would need segment reference)
    t.flow_rate *= (1.0f / (fluid.turgor_pressure + 1e-6f));
}

// Apply Skelly deformation to SVO (muscle expansion -> voxel push)
__global__ void skelly_deform_svo_kernel(SkellyInstance* instances, uint32_t instance_count,
                                          BoneSegment* segments, MuscleGroup* muscles,
                                          uint32_t* dirty_chunks) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= instance_count) return;

    SkellyInstance& inst = instances[idx];

    // For each muscle group, push voxels outward
    for (uint32_t m = inst.muscle_start; m < inst.muscle_start + inst.muscle_count; m++) {
        MuscleGroup& mg = muscles[m];
        if (mg.activation > 0.01f) {
            // Mark nearby chunks as dirty for SVO rebuild
            // Would calculate affected chunk coordinates here
            // For now, just set a flag
            if (dirty_chunks) dirty_chunks[0] = 1;
        }
    }
}
