#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <math.h>
#include "../kernels/voxel_svo.cu"
#include "../kernels/skelly_compute.cu"
#include "pillar_coupling.cu"

// Skelly physics → SVO deformation
// From FULL_ARCHITECTURE.md: "Deformation (Skelly → SVO)"
// - Muscle expansion → voxel push
// - Segment fracture → voxel removal
// - Organ volume → structural deformation

#define DEFORMATION_PUSH_STRENGTH 0.5f
#define DEFORMATION_REMOVAL_RADIUS 2.0f
#define CHUNK_SIZE 64

// Apply muscle expansion to SVO (voxel push)
__global__ void deformation_muscle_push_kernel(MuscleGroup* muscles, uint32_t muscle_count,
                                               BoneNode* bones, SVO_Chunk* chunks,
                                               uint32_t* dirty_chunks, uint32_t chunk_count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= muscle_count) return;
    
    MuscleGroup& mg = muscles[idx];
    if (mg.activation < 0.01f) return;  // No significant activation
    
    BoneNode& origin = bones[mg.origin_node];
    BoneNode& insertion = bones[mg.insertion_node];
    
    // Calculate expansion direction and magnitude
    float3 dir = normalize(insertion.global_pos - origin.global_pos);
    float expansion = mg.current_volume * DEFORMATION_PUSH_STRENGTH;
    
    // Push voxels along the bone direction
    // Find affected chunks
    float3 min_bound = make_float3(
        min(origin.global_pos.x, insertion.global_pos.x),
        min(origin.global_pos.y, insertion.global_pos.y),
        min(origin.global_pos.z, insertion.global_pos.z)
    );
    float3 max_bound = make_float3(
        max(origin.global_pos.x, insertion.global_pos.x),
        max(origin.global_pos.y, insertion.global_pos.y),
        max(origin.global_pos.z, insertion.global_pos.z)
    );
    
    // Expand bounds by activation radius
    float radius = sqrtf(expansion) * 0.1f;
    min_bound = min_bound - make_float3(radius, radius, radius);
    max_bound = max_bound + make_float3(radius, radius, radius);
    
    // Mark affected chunks as dirty
    for (uint32_t c = 0; c < chunk_count; c++) {
        SVO_Chunk& chunk = chunks[c];
        float3 chunk_center = make_float3(chunk.x * CHUNK_SIZE + CHUNK_SIZE/2,
                                           chunk.y * CHUNK_SIZE + CHUNK_SIZE/2,
                                           chunk.z * CHUNK_SIZE + CHUNK_SIZE/2);
        float3 chunk_min = chunk_center - make_float3(CHUNK_SIZE/2, CHUNK_SIZE/2, CHUNK_SIZE/2);
        float3 chunk_max = chunk_center + make_float3(CHUNK_SIZE/2, CHUNK_SIZE/2, CHUNK_SIZE/2);
        
        // AABB intersection
        bool intersects = (min_bound.x <= chunk_max.x && max_bound.x >= chunk_min.x &&
                           min_bound.y <= chunk_max.y && max_bound.y >= chunk_min.y &&
                           min_bound.z <= chunk_max.z && max_bound.z >= chunk_min.z);
        if (intersects) {
            atomicExch(&chunk.dirty, 1);
            atomicExch(&dirty_chunks[0], 1);
        }
    }
}

// Apply segment fracture to SVO (voxel removal)
__global__ void deformation_fracture_removal_kernel(BoneSegment* segments, uint32_t segment_count,
                                                     BoneNode* bones, SVO_Chunk* chunks,
                                                     uint32_t* dirty_chunks, uint32_t chunk_count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= segment_count) return;
    
    BoneSegment& seg = segments[idx];
    if (!seg.is_fractured) return;
    
    BoneNode& start = bones[seg.start_node];
    BoneNode& end = bones[seg.end_node];
    
    // Remove voxels in a radius around the fractured segment
    float3 center = (start.global_pos + end.global_pos) * 0.5f;
    float radius = DEFORMATION_REMOVAL_RADIUS;
    
    // Mark affected chunks as dirty
    int min_cx = floorf((center.x - radius) / CHUNK_SIZE);
    int min_cy = floorf((center.y - radius) / CHUNK_SIZE);
    int min_cz = floorf((center.z - radius) / CHUNK_SIZE);
    int max_cx = floorf((center.x + radius) / CHUNK_SIZE);
    int max_cy = floorf((center.y + radius) / CHUNK_SIZE);
    int max_cz = floorf((center.z + radius) / CHUNK_SIZE);
    
    for (int cx = min_cx; cx <= max_cx; cx++) {
        for (int cy = min_cy; cy <= max_cy; cy++) {
            for (int cz = min_cz; cz <= max_cz; cz++) {
                for (uint32_t c = 0; c < chunk_count; c++) {
                    SVO_Chunk& chunk = chunks[c];
                    if ((int)chunk.x == cx && (int)chunk.y == cy && (int)chunk.z == cz) {
                        atomicExch(&chunk.dirty, 1);
                        atomicExch(&dirty_chunks[0], 1);
                        break;
                    }
                }
            }
        }
    }
}

// Apply organ volume to SVO (structural deformation)
__global__ void deformation_organ_volume_kernel(Organ* organs, uint32_t organ_count,
                                                BoneSegment* segments, BoneNode* bones,
                                                SVO_Chunk* chunks, uint32_t* dirty_chunks,
                                                uint32_t chunk_count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= organ_count) return;
    
    Organ& organ = organs[idx];
    if (organ.volume < 0.1f) return;
    
    // Get attached segment
    BoneSegment& seg = segments[organ.segment_idx];
    BoneNode& start = bones[seg.start_node];
    BoneNode& end = bones[seg.end_node];
    
    // Deform based on organ volume
    float3 center = (start.global_pos + end.global_pos) * 0.5f;
    float deform_radius = sqrtf(organ.volume) * 0.2f;
    
    // Mark affected chunks as dirty
    // Simplified: just mark all chunks for now
    for (uint32_t c = 0; c < chunk_count; c++) {
        atomicExch(&chunks[c].dirty, 1);
    }
    atomicExch(&dirty_chunks[0], 1);
}

// Master deformation step: Skelly → SVO
__host__ void deformation_step(MuscleGroup* d_muscles, uint32_t muscle_count,
                               BoneSegment* d_segments, uint32_t segment_count,
                               BoneNode* d_bones, uint32_t bone_count,
                               Organ* d_organs, uint32_t organ_count,
                               SVO_Chunk* d_chunks, uint32_t chunk_count,
                               uint32_t* d_dirty_flag) {
    
    uint32_t block_size = 256;
    
    // 1. Muscle expansion → voxel push
    if (muscle_count > 0) {
        uint32_t grid_size = (muscle_count + block_size - 1) / block_size;
        deformation_muscle_push_kernel<<<grid_size, block_size>>>(
            d_muscles, muscle_count, d_bones, d_chunks, d_dirty_flag, chunk_count
        );
    }
    
    // 2. Fracture → voxel removal
    if (segment_count > 0) {
        uint32_t grid_size = (segment_count + block_size - 1) / block_size;
        deformation_fracture_removal_kernel<<<grid_size, block_size>>>(
            d_segments, segment_count, d_bones, d_chunks, d_dirty_flag, chunk_count
        );
    }
    
    // 3. Organ volume → structural deformation
    if (organ_count > 0) {
        uint32_t grid_size = (organ_count + block_size - 1) / block_size;
        deformation_organ_volume_kernel<<<grid_size, block_size>>>(
            d_organs, organ_count, d_segments, d_bones, d_chunks, d_dirty_flag, chunk_count
        );
    }
}
