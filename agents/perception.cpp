// perception.cpp - Agent perception implementation with creature integration

#include "perception.h"
#include "../scene/chunk.h"
#include <cmath>
#include <algorithm>

AgentPerception::AgentPerception(float view_distance)
    : view_distance_(view_distance) {
}

PerceptionResult AgentPerception::sample_voxels(float x, float y, float z,
                                                  const std::vector<std::unique_ptr<Chunk>>& chunks) {
    PerceptionResult result = {};
    result.has_voxels = false;
    
    Chunk* chunk = get_chunk_at(x, y, z, chunks);
    if (!chunk) return result;
    
    int chunk_x, chunk_y, chunk_z;
    int voxel_x, voxel_y, voxel_z;
    world_to_chunk(x, y, z, chunk_x, chunk_y, chunk_z, voxel_x, voxel_y, voxel_z);
    
    int count = 0;
    float total_r = 0.0f, total_g = 0.0f, total_b = 0.0f;
    float total_density = 0.0f;
    
    int radius = 3;
    for (int dz = -radius; dz <= radius; dz++) {
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                int vx = voxel_x + dx;
                int vy = voxel_y + dy;
                int vz = voxel_z + dz;
                
                if (vx < 0 || vx >= 16 || vy < 0 || vy >= 16 || vz < 0 || vz >= 16) continue;
                
                if (chunk->get_voxel(vx, vy, vz).material != 0) {
                    result.has_voxels = true;
                    total_density += 1.0f;
                }
                count++;
            }
        }
    }
    
    result.voxel_density = (count > 0) ? (total_density / count) : 0.0f;
    
    return result;
}

bool AgentPerception::line_of_sight(float start_x, float start_y, float start_z,
                                      float end_x, float end_y, float end_z,
                                      const std::vector<std::unique_ptr<Chunk>>& chunks) {
    float dx = end_x - start_x;
    float dy = end_y - start_y;
    float dz = end_z - start_z;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    
    if (dist < 0.001f) return true;
    
    int steps = static_cast<int>(dist * 2.0f);
    steps = std::max(steps, 1);
    
    float step_x = dx / steps;
    float step_y = dy / steps;
    float step_z = dz / steps;
    
    for (int i = 1; i < steps; i++) {
        float x = start_x + step_x * i;
        float y = start_y + step_y * i;
        float z = start_z + step_z * i;
        
        Chunk* chunk = get_chunk_at(x, y, z, chunks);
        if (!chunk) continue;
        
        int chunk_x, chunk_y, chunk_z;
        int voxel_x, voxel_y, voxel_z;
        world_to_chunk(x, y, z, chunk_x, chunk_y, chunk_z, voxel_x, voxel_y, voxel_z);
        
        if (chunk->get_voxel(voxel_x, voxel_y, voxel_z).material != 0) {
            return false;
        }
    }
    
    return true;
}

template<typename AgentIter>
void AgentPerception::detect_nearby_agents(float x, float y, float z, float radius,
                                             AgentIter begin, AgentIter end,
                                             std::vector<int>& out_agent_ids) {
    out_agent_ids.clear();
    
    int id = 0;
    for (auto it = begin; it != end; ++it, id++) {
        float ax = 0.0f, ay = 0.0f, az = 0.0f;
        
        float dist = distance_3d(x, y, z, ax, ay, az);
        if (dist <= radius) {
            out_agent_ids.push_back(id);
        }
    }
}

void AgentPerception::scan_environment(float x, float y, float z,
                                          float& temperature, float& hazard, float& resources) {
    temperature = 20.0f;
    hazard = 0.0f;
    resources = 0.5f;
}

CreaturePerceptionResult AgentPerception::perceive_creature(const Creature* creature,
                                                            float agent_x, float agent_y, float agent_z) {
    CreaturePerceptionResult result = {};
    
    if (!creature) return result;
    
    result.creature_id = creature->get_entity_id();
    
    auto state = creature->get_state();
    
    // TODO: get creature position from Creature class when available
    // For now, creature position defaults to origin
    float creature_x = 0.0f, creature_y = 0.0f, creature_z = 0.0f;
    result.distance = distance_3d(agent_x, agent_y, agent_z, creature_x, creature_y, creature_z);
    result.size = state.scale;
    result.health = state.overall_health;
    
    result.threat_level = 0.0f;
    
    result.is_dreaming = state.is_dreaming;
    result.is_in_shadow = state.in_shadow;
    
    result.r = state.pillars[PILLAR_AWARENESS];
    result.g = state.pillars[PILLAR_HARM];
    result.b = state.pillars[PILLAR_DEPTH];
    result.a = state.pillars[PILLAR_INTEGRITY];
    
    for (int i = 0; i < 8; i++) {
        result.creature_type_score[i] = state.pillars[i];
    }
    
    return result;
}

void AgentPerception::detect_nearby_creatures(float x, float y, float z, float radius,
                                              GrowthSystem* growth_system,
                                              std::vector<CreaturePerceptionResult>& out_results) {
    out_results.clear();
    
    if (!growth_system) return;
    
    for (uint32_t entity_id : growth_system->get_alive_creatures()) {
        auto* creature = growth_system->get_creature(entity_id);
        if (!creature) continue;
        
        auto perception = perceive_creature(creature, x, y, z);
        
        if (perception.distance <= radius) {
            out_results.push_back(perception);
        }
    }
    
    std::sort(out_results.begin(), out_results.end(),
        [](const CreaturePerceptionResult& a, const CreaturePerceptionResult& b) {
            return a.distance < b.distance;
        });
}

void AgentPerception::perceive_creatures_in_radius(float x, float y, float z, float radius,
                                                    GrowthSystem* growth_system,
                                                    std::vector<CreaturePerceptionResult>& out_results) {
    detect_nearby_creatures(x, y, z, radius, growth_system, out_results);
}

float AgentPerception::calculate_threat_level(const CreaturePerceptionResult& perception,
                                               const PillarStateVector& agent_pillars) {
    float threat = 0.0f;
    
    float awareness = (agent_pillars[PILLAR_AWARENESS] + agent_pillars[PILLAR_WILLPOWER]) * 0.5f;
    
    if (perception.is_dreaming) {
        threat += THREAT_WEIGHT_DREAMING;
    }
    if (perception.is_in_shadow) {
        threat += THREAT_WEIGHT_SHADOW;
    }
    if (perception.health < 0.3f) {
        threat += THREAT_WEIGHT_LOW_HEALTH;
    }
    if (perception.size > 1.5f) {
        threat += THREAT_WEIGHT_LARGE_SIZE;
    }
    
    float distance_factor = 1.0f / (1.0f + perception.distance * 0.1f);
    threat *= distance_factor;
    
    return std::fmax(0.0f, threat);
}

Chunk* AgentPerception::get_chunk_at(float x, float y, float z,
                                       const std::vector<std::unique_ptr<Chunk>>& chunks) {
    int chunk_x, chunk_y, chunk_z;
    int voxel_x, voxel_y, voxel_z;
    world_to_chunk(x, y, z, chunk_x, chunk_y, chunk_z, voxel_x, voxel_y, voxel_z);
    
    for (const auto& chunk : chunks) {
        if (chunk && chunk->get_chunk_x() == chunk_x && chunk->get_chunk_y() == chunk_y && chunk->get_chunk_z() == chunk_z) {
            return chunk.get();
        }
    }
    
    return nullptr;
}

void AgentPerception::world_to_chunk(float x, float y, float z,
                                      int& chunk_x, int& chunk_y, int& chunk_z,
                                      int& voxel_x, int& voxel_y, int& voxel_z) {
    const int CHUNK_SIZE = 16;
    
    chunk_x = static_cast<int>(std::floor(x / CHUNK_SIZE));
    chunk_y = static_cast<int>(std::floor(y / CHUNK_SIZE));
    chunk_z = static_cast<int>(std::floor(z / CHUNK_SIZE));
    
    voxel_x = static_cast<int>(x) - chunk_x * CHUNK_SIZE;
    voxel_y = static_cast<int>(y) - chunk_y * CHUNK_SIZE;
    voxel_z = static_cast<int>(z) - chunk_z * CHUNK_SIZE;
    
    if (voxel_x < 0) { chunk_x--; voxel_x += CHUNK_SIZE; }
    if (voxel_y < 0) { chunk_y--; voxel_y += CHUNK_SIZE; }
    if (voxel_z < 0) { chunk_z--; voxel_z += CHUNK_SIZE; }
    if (voxel_x >= CHUNK_SIZE) { chunk_x++; voxel_x -= CHUNK_SIZE; }
    if (voxel_y >= CHUNK_SIZE) { chunk_y++; voxel_y -= CHUNK_SIZE; }
    if (voxel_z >= CHUNK_SIZE) { chunk_z++; voxel_z -= CHUNK_SIZE; }
}

float AgentPerception::distance_3d(float x1, float y1, float z1,
                                    float x2, float y2, float z2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}