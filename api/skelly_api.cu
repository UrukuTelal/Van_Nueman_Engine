#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <stdio.h>
#include <math.h>
#include "../kernels/skelly_shared.cuh"  // Shared Skelly definitions

// Extern declarations for kernels defined in skelly_compute.cu (resolved at -rdc=true link time)
extern __global__ void skelly_update_bones_kernel(BoneNode* bones, uint32_t count);
extern __global__ void skelly_update_muscles_kernel(MuscleGroup* muscles, MuscleStrand* strands,
                                                     BoneNode* bones, uint32_t count);
extern __global__ void skelly_operate_organs_kernel(Organ* organs, uint32_t count,
                                                     Transport* transports, PillarStateVector* entity_pillars);
extern __global__ void skelly_turgor_kernel(InterstitialFluid* fluids, uint32_t fluid_count,
                                             Transport* transports, MuscleGroup* muscles,
                                             uint32_t muscle_count);
extern __global__ void skelly_deform_svo_kernel(SkellyInstance* instances, uint32_t instance_count,
                                                 BoneSegment* segments, MuscleGroup* muscles,
                                                 uint32_t* dirty_chunks);

// Fractal Skelly API: works for entity, server, federation scales
// Ported from APIs/skelly_api.py (Python/Taichi reference)

#define MAX_API_INSTANCES 100000
#define STRANDS_PER_MUSCLE 8
#define ORGANS_PER_SEGMENT 4

// Skelly API context
struct SkellyAPIContext {
    SkellyInstance* d_instances;
    BoneNode* d_bones;
    BoneSegment* d_segments;
    MuscleGroup* d_muscles;
    MuscleStrand* d_strands;
    Organ* d_organs;
    Transport* d_transports;
    InterstitialFluid* d_fluids;
    
    uint32_t instance_count;
    uint32_t bone_count;
    uint32_t segment_count;
    uint32_t muscle_count;
    uint32_t strand_count;
    uint32_t organ_count;
    uint32_t transport_count;
    uint32_t fluid_count;
    
    // Pointers to next free slot for each array
    uint32_t next_instance;
    uint32_t next_bone;
    uint32_t next_segment;
    uint32_t next_muscle;
    uint32_t next_strand;
    uint32_t next_organ;
    uint32_t next_transport;
    uint32_t next_fluid;
};

// Initialize Skelly API
__host__ SkellyAPIContext* skelly_api_init(uint32_t max_instances, uint32_t max_bones,
                                            uint32_t max_segments, uint32_t max_muscles) {
    SkellyAPIContext* ctx = (SkellyAPIContext*)malloc(sizeof(SkellyAPIContext));
    
    auto check = [](cudaError_t e, const char* label) {
        if (e != cudaSuccess) {
            fprintf(stderr, "[skelly_api] cudaMalloc failed: %s (%s)\n", label, cudaGetErrorString(e));
        }
        return e;
    };

    check(cudaMalloc(&ctx->d_instances, max_instances * sizeof(SkellyInstance)), "d_instances");
    check(cudaMalloc(&ctx->d_bones, max_bones * sizeof(BoneNode)), "d_bones");
    check(cudaMalloc(&ctx->d_segments, max_segments * sizeof(BoneSegment)), "d_segments");
    check(cudaMalloc(&ctx->d_muscles, max_muscles * sizeof(MuscleGroup)), "d_muscles");
    check(cudaMalloc(&ctx->d_strands, max_muscles * STRANDS_PER_MUSCLE * sizeof(MuscleStrand)), "d_strands");
    check(cudaMalloc(&ctx->d_organs, max_segments * ORGANS_PER_SEGMENT * sizeof(Organ)), "d_organs");
    check(cudaMalloc(&ctx->d_transports, max_segments * sizeof(Transport)), "d_transports");
    check(cudaMalloc(&ctx->d_fluids, max_segments * sizeof(InterstitialFluid)), "d_fluids");
    
    ctx->instance_count = 0;
    ctx->bone_count = 0;
    ctx->segment_count = 0;
    ctx->muscle_count = 0;
    ctx->strand_count = 0;
    ctx->organ_count = 0;
    ctx->transport_count = 0;
    ctx->fluid_count = 0;
    
    ctx->next_instance = 0;
    ctx->next_bone = 0;
    ctx->next_segment = 0;
    ctx->next_muscle = 0;
    ctx->next_strand = 0;
    ctx->next_organ = 0;
    ctx->next_transport = 0;
    ctx->next_fluid = 0;
    
    return ctx;
}

// Create a creature (from Python reference create_creature)
__host__ uint32_t skelly_api_create_creature(SkellyAPIContext* ctx, uint32_t entity_id) {
    if (ctx->next_instance >= MAX_API_INSTANCES) return (uint32_t)-1;
    
    uint32_t instance_idx = ctx->next_instance++;
    SkellyInstance inst;
    inst.entity_id = entity_id;
    inst.bone_start = ctx->next_bone;
    inst.bone_count = 0;
    inst.segment_start = ctx->next_segment;
    inst.segment_count = 0;
    inst.muscle_start = ctx->next_muscle;
    inst.muscle_count = 0;
    inst.organ_start = ctx->next_organ;
    inst.organ_count = 0;
    inst.transport_start = ctx->next_transport;
    inst.transport_count = 0;
    
    // Create root bone
    uint32_t root_idx = ctx->next_bone++;
    BoneNode root;
    root.local_pos = make_float3(0, 0, 0);
    root.global_pos = make_float3(0, 0, 0);
    root.parent_idx = (uint32_t)-1;
    root.is_fractured = 0;
    root.constraint_pitch_min = -1.5f; root.constraint_pitch_max = 1.5f;
    root.constraint_yaw_min = -1.5f; root.constraint_yaw_max = 1.5f;
    root.constraint_roll_min = -0.5f; root.constraint_roll_max = 0.5f;
    cudaMemcpy(&ctx->d_bones[root_idx], &root, sizeof(BoneNode), cudaMemcpyHostToDevice);
    inst.bone_start = root_idx;
    inst.bone_count = 1;
    
    // Create core bone (from Python reference)
    uint32_t core_idx = ctx->next_bone++;
    BoneNode core;
    core.local_pos = make_float3(0, 0, 20);
    core.global_pos = make_float3(0, 0, 20);
    core.parent_idx = root_idx;
    core.is_fractured = 0;
    core.constraint_pitch_min = -1.5f; core.constraint_pitch_max = 1.5f;
    core.constraint_yaw_min = -1.5f; core.constraint_yaw_max = 1.5f;
    core.constraint_roll_min = -0.5f; core.constraint_roll_max = 0.5f;
    cudaMemcpy(&ctx->d_bones[core_idx], &core, sizeof(BoneNode), cudaMemcpyHostToDevice);
    inst.bone_count++;
    
    // Create core segment
    uint32_t seg_idx = ctx->next_segment++;
    BoneSegment seg;
    seg.start_node = root_idx;
    seg.end_node = core_idx;
    seg.flexibility = 0.1f;
    seg.break_threshold = 100.0f;
    seg.is_fractured = 0;
    seg.total_capacity = M_PI * 2.0f * 2.0f * 20.0f;  // pi*r²*len
    seg.organ_count = 0;
    seg.organ_start = 0;
    cudaMemcpy(&ctx->d_segments[seg_idx], &seg, sizeof(BoneSegment), cudaMemcpyHostToDevice);
    inst.segment_count = 1;
    
    // Create core muscle group
    uint32_t muscle_idx = ctx->next_muscle++;
    MuscleGroup mg;
    mg.origin_node = root_idx;
    mg.insertion_node = core_idx;
    mg.strand_start = ctx->next_strand;
    mg.strand_count = 8;
    mg.activation = 0.0f;
    mg.current_volume = 0.0f;
    cudaMemcpy(&ctx->d_muscles[muscle_idx], &mg, sizeof(MuscleGroup), cudaMemcpyHostToDevice);
    inst.muscle_count = 1;
    ctx->next_strand += 8;
    
    // Create core organ (power_plant, from Python reference)
    uint32_t organ_idx = ctx->next_organ++;
    Organ organ;
    organ.type = 2;  // power_plant
    organ.volume = 50.0f;
    organ.active_state = 0.0f;
    organ.energy_output = 0.0f;
    organ.segment_idx = seg_idx;
    cudaMemcpy(&ctx->d_organs[organ_idx], &organ, sizeof(Organ), cudaMemcpyHostToDevice);
    inst.organ_count = 1;
    seg.organ_start = organ_idx;
    seg.organ_count = 1;
    cudaMemcpy(&ctx->d_segments[seg_idx], &seg, sizeof(BoneSegment), cudaMemcpyHostToDevice);
    
    // Create interstitial fluid for segment
    uint32_t fluid_idx = ctx->next_fluid++;
    InterstitialFluid fluid;
    fluid.transport_idx = ctx->next_transport++;
    fluid.segment_idx = seg_idx;
    fluid.turgor_pressure = 1.0f;
    cudaMemcpy(&ctx->d_fluids[fluid_idx], &fluid, sizeof(InterstitialFluid), cudaMemcpyHostToDevice);
    inst.transport_count = 1;
    
    // Save instance
    cudaMemcpy(&ctx->d_instances[instance_idx], &inst, sizeof(SkellyInstance), cudaMemcpyHostToDevice);
    ctx->instance_count++;
    
    return instance_idx;
}

// Set organ activity (from Python reference set_organ_activity)
__host__ void skelly_api_set_organ_activity(SkellyAPIContext* ctx, uint32_t instance_idx,
                                              uint32_t organ_idx, float intensity) {
    SkellyInstance inst;
    cudaMemcpy(&inst, &ctx->d_instances[instance_idx], sizeof(SkellyInstance), cudaMemcpyDeviceToHost);
    
    if (organ_idx >= inst.organ_count) return;
    uint32_t global_organ_idx = inst.organ_start + organ_idx;
    
    Organ organ;
    cudaMemcpy(&organ, &ctx->d_organs[global_organ_idx], sizeof(Organ), cudaMemcpyDeviceToHost);
    organ.active_state = max(0.0f, min(1.0f, intensity));
    cudaMemcpy(&ctx->d_organs[global_organ_idx], &organ, sizeof(Organ), cudaMemcpyHostToDevice);
}

// Step simulation (from Python reference step())
__host__ void skelly_api_step(SkellyAPIContext* ctx, float dt) {
    if (ctx->instance_count == 0) return;
    
    // 1. Update bone global positions
    skelly_update_bones_kernel<<<(ctx->bone_count + 255) / 256, 256>>>(
        ctx->d_bones, ctx->bone_count
    );
    
    // 2. Update muscles
    skelly_update_muscles_kernel<<<(ctx->muscle_count + 255) / 256, 256>>>(
        ctx->d_muscles, ctx->d_strands, ctx->d_bones, ctx->muscle_count
    );
    
    // 3. Operate organs
    skelly_operate_organs_kernel<<<(ctx->organ_count + 255) / 256, 256>>>(
        ctx->d_organs, ctx->organ_count, ctx->d_transports, nullptr  // pillar pointers TBD
    );
    
    // 4. Update turgor pressure
    skelly_turgor_kernel<<<(ctx->fluid_count + 255) / 256, 256>>>(
        ctx->d_fluids, ctx->fluid_count, ctx->d_transports, ctx->d_muscles, ctx->muscle_count
    );
    
    // 5. Deform SVO (mark dirty chunks)
    uint32_t* d_dirty;
    cudaMalloc(&d_dirty, sizeof(uint32_t));
    cudaMemset(d_dirty, 0, sizeof(uint32_t));
    skelly_deform_svo_kernel<<<(ctx->instance_count + 255) / 256, 256>>>(
        ctx->d_instances, ctx->instance_count, ctx->d_segments, ctx->d_muscles, d_dirty
    );
    cudaFree(d_dirty);
}

// Get system state (from Python reference get_system_state)
__host__ void skelly_api_get_state(SkellyAPIContext* ctx, uint32_t instance_idx,
                                    float* energy_out, float* pressure_avg_out, bool* is_alive_out) {
    SkellyInstance inst;
    cudaMemcpy(&inst, &ctx->d_instances[instance_idx], sizeof(SkellyInstance), cudaMemcpyDeviceToHost);
    
    float total_energy = 0.0f;
    for (uint32_t i = 0; i < inst.organ_count; i++) {
        Organ organ;
        cudaMemcpy(&organ, &ctx->d_organs[inst.organ_start + i], sizeof(Organ), cudaMemcpyDeviceToHost);
        total_energy += organ.energy_output;
    }
    *energy_out = total_energy;
    
    float total_pressure = 0.0f;
    for (uint32_t i = 0; i < inst.transport_count; i++) {
        Transport t;
        cudaMemcpy(&t, &ctx->d_transports[inst.transport_start + i], sizeof(Transport), cudaMemcpyDeviceToHost);
        total_pressure += t.pressure;
    }
    *pressure_avg_out = (inst.transport_count > 0) ? total_pressure / inst.transport_count : 0.0f;
    
    // Check if any segment is fractured
    bool alive = true;
    for (uint32_t i = 0; i < inst.segment_count; i++) {
        BoneSegment seg;
        cudaMemcpy(&seg, &ctx->d_segments[inst.segment_start + i], sizeof(BoneSegment), cudaMemcpyDeviceToHost);
        if (seg.is_fractured) {
            alive = false;
            break;
        }
    }
    *is_alive_out = alive;
}

// Fractal: server-level Skelly (servers = nodes, bandwidth = muscles, latency = turgor)
__host__ void skelly_api_create_server_skelly(SkellyAPIContext* ctx, uint32_t server_id) {
    // Simplified: create a Skelly instance representing a server's compute/bandwidth/latency
    uint32_t instance_idx = skelly_api_create_creature(ctx, server_id);
    // Modify to represent server components instead of biological parts
    // (compute = bones, processes = organs, bandwidth = muscles, latency = turgor)
}

// Cleanup
__host__ void skelly_api_cleanup(SkellyAPIContext* ctx) {
    if (!ctx) return;
    cudaFree(ctx->d_instances);
    cudaFree(ctx->d_bones);
    cudaFree(ctx->d_segments);
    cudaFree(ctx->d_muscles);
    cudaFree(ctx->d_strands);
    cudaFree(ctx->d_organs);
    cudaFree(ctx->d_transports);
    cudaFree(ctx->d_fluids);
    free(ctx);
}
