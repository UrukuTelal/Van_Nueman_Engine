#include "perception.h"
#include <cmath>
#include <algorithm>
#include <cstring>

AgentPerception::AgentPerception(float view_distance)
    : view_distance_(view_distance) {
}

PerceptionResult AgentPerception::sample_voxels(float x, float y, float z,
                                                 const std::vector<std::unique_ptr<Chunk>>& chunks) {
    PerceptionResult result;
    std::memset(result.material_count, 0, sizeof(result.material_count));
    result.voxel_density = 0.0f;
    result.has_voxels = false;
    result.avg_r = result.avg_g = result.avg_b = 0.0f;
    
    int chunks_checked = 0;
    int total_voxels = 0;
    int solid_voxels = 0;
    
    // Check chunks within view distance
    int chunk_radius = static_cast<int>(std::ceil(view_distance_ / CHUNK_SIZE));
    int center_chunk_x = static_cast<int>(x / CHUNK_SIZE);
    int center_chunk_y = static_cast<int>(y / CHUNK_SIZE);
    int center_chunk_z = static_cast<int>(z / CHUNK_SIZE);
    
    for (int dx = -chunk_radius; dx <= chunk_radius; dx++) {
        for (int dy = -chunk_radius; dy <= chunk_radius; dy++) {
            for (int dz = -chunk_radius; dz <= chunk_radius; dz++) {
                int check_x = center_chunk_x + dx;
                int check_y = center_chunk_y + dy;
                int check_z = center_chunk_z + dz;
                
                // Find chunk
                Chunk* chunk = nullptr;
                for (const auto& c : chunks) {
                    if (c->get_chunk_x() == check_x &&
                        c->get_chunk_y() == check_y &&
                        c->get_chunk_z() == check_z) {
                        chunk = c.get();
                        break;
                    }
                }
                
                if (!chunk) continue;
                chunks_checked++;
                
                // Sample voxels in this chunk (simplified - just check a few)
                for (int vx = 0; vx < CHUNK_SIZE; vx += 4) {
                    for (int vy = 0; vy < CHUNK_SIZE; vy += 4) {
                        for (int vz = 0; vz < CHUNK_SIZE; vz += 4) {
                            const Voxel& v = chunk->get_voxel(vx, vy, vz);
                            if (v.material != 0) {
                                solid_voxels++;
                                result.material_count[v.material]++;
                                result.avg_r += v.r;
                                result.avg_g += v.g;
                                result.avg_b += v.b;
                            }
                            total_voxels++;
                        }
                    }
                }
            }
        }
    }
    
    if (total_voxels > 0) {
        result.voxel_density = static_cast<float>(solid_voxels) / total_voxels;
        if (solid_voxels > 0) {
            result.avg_r /= solid_voxels;
            result.avg_g /= solid_voxels;
            result.avg_b /= solid_voxels;
        }
        result.has_voxels = solid_voxels > 0;
    }
    
    return result;
}

bool AgentPerception::line_of_sight(float start_x, float start_y, float start_z,
                                    float end_x, float end_y, float end_z,
                                    const std::vector<std::unique_ptr<Chunk>>& chunks) {
    float dx = end_x - start_x;
    float dy = end_y - start_y;
    float dz = end_z - start_z;
    float length = std::sqrt(dx*dx + dy*dy + dz*dz);
    
    if (length < 0.001f) return true;
    
    // Normalize direction
    dx /= length;
    dy /= length;
    dz /= length;
    
    float t = 0.0f;
    float step = 1.0f;  // Step size in world units
    
    while (t < length) {
        float x = start_x + dx * t;
        float y = start_y + dy * t;
        float z = start_z + dz * t;
        
        Chunk* chunk = get_chunk_at(x, y, z, chunks);
        if (chunk) {
            int voxel_x, voxel_y, voxel_z, chunk_x, chunk_y, chunk_z;
            world_to_chunk(x, y, z, chunk_x, chunk_y, chunk_z, voxel_x, voxel_y, voxel_z);
            
            if (Chunk::is_valid_pos(voxel_x, voxel_y, voxel_z)) {
                const Voxel& v = chunk->get_voxel(voxel_x, voxel_y, voxel_z);
                if (v.material != 0) {
                    return false;  // Hit a solid voxel
                }
            }
        }
        
        t += step;
    }
    
    return true;  // No obstruction
}

void AgentPerception::world_to_chunk(float x, float y, float z,
                                     int& chunk_x, int& chunk_y, int& chunk_z,
                                     int& voxel_x, int& voxel_y, int& voxel_z) {
    chunk_x = static_cast<int>(std::floor(x / CHUNK_SIZE));
    chunk_y = static_cast<int>(std::floor(y / CHUNK_SIZE));
    chunk_z = static_cast<int>(std::floor(z / CHUNK_SIZE));
    
    float local_x = x - chunk_x * CHUNK_SIZE;
    float local_y = y - chunk_y * CHUNK_SIZE;
    float local_z = z - chunk_z * CHUNK_SIZE;
    
    voxel_x = static_cast<int>(local_x);
    voxel_y = static_cast<int>(local_y);
    voxel_z = static_cast<int>(local_z);
}

Chunk* AgentPerception::get_chunk_at(float x, float y, float z,
                                       const std::vector<std::unique_ptr<Chunk>>& chunks) {
    int chunk_x, chunk_y, chunk_z, dummy1, dummy2, dummy3;
    world_to_chunk(x, y, z, chunk_x, chunk_y, chunk_z, dummy1, dummy2, dummy3);
    
    for (const auto& chunk : chunks) {
        if (chunk->get_chunk_x() == chunk_x &&
            chunk->get_chunk_y() == chunk_y &&
            chunk->get_chunk_z() == chunk_z) {
            return chunk.get();
        }
    }
    return nullptr;
}

void AgentPerception::scan_environment(float x, float y, float z,
                                      float& temperature, float& hazard, float& resources) {
    // Simplified: return world state values
    // In real implementation, would sample environment at (x,y,z)
    temperature = 0.5f;
    hazard = 0.1f;
    resources = 0.7f;
}
