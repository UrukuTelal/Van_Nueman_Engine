#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cmath>

#define MAX_BONES 1000000
#define MAX_SEGMENTS 1000000
#define MAX_MUSCLES 2000000
#define MAX_ORGANS 500000
#define MAX_TRANSPORTS 500000

// Float3 operations (CUDA built-in float3 used, but defining helpers)
__device__ float3 make_float3(float x, float y, float z) {
    float3 r; r.x = x; r.y = y; r.z = z; return r;
}

// Bone node (joint)
struct BoneNode {
    float3 local_pos;
    float3 global_pos;
    uint32_t parent_idx;
    uint32_t is_fractured;
    float constraint_pitch_min, constraint_pitch_max;
    float constraint_yaw_min, constraint_yaw_max;
    float constraint_roll_min, constraint_roll_max;
};

// Bone segment (between two nodes)
struct BoneSegment {
    uint32_t start_node, end_node;
    float flexibility;       // 0 = rigid, 1 = very flexible
    float break_threshold;
    uint32_t is_fractured;
    float total_capacity;    // volume capacity for muscles/organs
    uint32_t organ_count;
    uint32_t organ_start;    // index into organ list
};

// Muscle strand (part of a muscle group)
struct MuscleStrand {
    float base_r;
    float current_r;
    float origin_rot, insertion_rot;
};

// Volumetric muscle group
struct MuscleGroup {
    uint32_t origin_node, insertion_node;
    uint32_t strand_start;   // index into strand list
    uint32_t strand_count;
    float activation;        // 0-1
    float current_volume;
};

// Organ (energy production, processing, etc.)
struct Organ {
    uint32_t type;           // 0=pump, 1=valve, 2=power_plant, 3=factory
    float volume;
    float active_state;      // 0-1 pulse intensity
    float energy_output;
    uint32_t segment_idx;    // which segment this organ is attached to
};

// Transport system (Conduit/Gate/Driver from UniversalTerms)
struct Transport {
    uint32_t start_node, end_node;
    float flow_rate;
    float resistance;
    float pressure;
    uint32_t is_severed;
    float elasticity;
};

// Interstitial fluid (specialized transport with turgor pressure)
struct InterstitialFluid {
    uint32_t transport_idx;
    uint32_t segment_idx;
    float turgor_pressure;
};

// Entity Skelly instance
struct SkellyInstance {
    uint32_t entity_id;
    uint32_t bone_start, bone_count;
    uint32_t segment_start, segment_count;
    uint32_t muscle_start, muscle_count;
    uint32_t organ_start, organ_count;
    uint32_t transport_start, transport_count;
};

// Update bone global positions (recursive, done in kernel)
__global__ void skelly_update_bones_kernel(BoneNode* bones, uint32_t count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    BoneNode& node = bones[idx];
    if (node.parent_idx == (uint32_t)-1 || node.is_fractured) {
        node.global_pos = node.local_pos;
    } else {
        BoneNode& parent = bones[node.parent_idx];
        node.global_pos = parent.global_pos + node.local_pos;
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

    float expansion = (rest_len > 0 && curr_len > 0) ? sqrtf(rest_len / curr_len) : 1.0f;

    // Update all strands
    for (uint32_t s = 0; s < mg.strand_count; s++) {
        MuscleStrand& strand = strands[mg.strand_start + s];
        strand.current_r = strand.base_r * expansion * (1.0f + mg.activation);
    }

    // Calculate new volume
    mg.current_volume = 0.0f;
    for (uint32_t s = 0; s < mg.strand_count; s++) {
        MuscleStrand& strand = strands[mg.strand_start + s];
        mg.current_volume += M_PI * strand.current_r * strand.current_r * curr_len;
    }
}

// Organ operation (pump, valve, power_plant, factory)
__global__ void skelly_operate_organs_kernel(Organ* organs, uint32_t count,
                                              Transport* transports, PillarStateVector* entity_pillars) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

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
