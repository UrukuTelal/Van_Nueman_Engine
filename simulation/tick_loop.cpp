#include "tick_loop.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <unordered_map>

SimulationTickLoop::SimulationTickLoop()
    : micro_tick_count_(0), meso_tick_count_(0), macro_tick_count_(0) {
    world_state_.temperature = 0.5f;
    world_state_.hazard_level = 0.1f;
    world_state_.resource_density = 0.7f;
    world_state_.tick_count = 0;
}

SimulationTickLoop::SpatialHashKey SimulationTickLoop::hash_cell(int cx, int cy, int cz) {
    return ((static_cast<uint64_t>(cx) & 0xFFFFF) << 40) |
           ((static_cast<uint64_t>(cy) & 0xFFFFF) << 20) |
           (static_cast<uint64_t>(cz) & 0xFFFFF);
}

void SimulationTickLoop::get_cell_coords(float x, float y, float z, int& cx, int& cy, int& cz) {
    cx = static_cast<int>(std::floor(x / SPATIAL_HASH_CELL_SIZE));
    cy = static_cast<int>(std::floor(y / SPATIAL_HASH_CELL_SIZE));
    cz = static_cast<int>(std::floor(z / SPATIAL_HASH_CELL_SIZE));
}

void SimulationTickLoop::build_spatial_hash() {
    spatial_hash_.clear();
    for (size_t i = 0; i < agents_.size(); i++) {
        if (!agents_[i].active) continue;
        int cx, cy, cz;
        get_cell_coords(agents_[i].x, agents_[i].y, agents_[i].z, cx, cy, cz);
        SpatialHashKey key = hash_cell(cx, cy, cz);
        spatial_hash_[key].push_back(static_cast<int>(i));
    }
}

void SimulationTickLoop::query_nearby_agents(const Agent& agent, int* nearby_indices, int max_nearby, int& count) const {
    count = 0;
    int center_cx, center_cy, center_cz;
    get_cell_coords(agent.x, agent.y, agent.z, center_cx, center_cy, center_cz);
    
    const int radius_in_cells = static_cast<int>(std::ceil(INTERACTION_RADIUS / SPATIAL_HASH_CELL_SIZE));
    
    for (int dx = -radius_in_cells; dx <= radius_in_cells; dx++) {
        for (int dy = -radius_in_cells; dy <= radius_in_cells; dy++) {
            for (int dz = -radius_in_cells; dz <= radius_in_cells; dz++) {
                int neighbor_id = hash_cell(center_cx + dx, center_cy + dy, center_cz + dz);
                auto it = spatial_hash_.find(neighbor_id);
                if (it != spatial_hash_.end()) {
                    const std::vector<int>& neighbors = it->second;
                    for (int neighbor : neighbors) {
                        if (neighbor == agent.id) continue;
                        if (count >= max_nearby) return;
                        nearby_indices[count] = neighbor;
                        count++;
                    }
                }
            }
        }
    }
}

void SimulationTickLoop::gather_resources(Agent& agent) {
    // TODO: gather resources - scan voxels at agent position
    // For now, increase resource density slightly (simulated gather)
    world_state_.resource_density = std::clamp(
        world_state_.resource_density + 0.001f, 0.0f, 1.0f);
    
    // In real implementation:
    // - Query voxels at (agent.x, agent.y, agent.z)
    // - Check for resource voxels (type == RESOURCE)
    // - Transfer to agent inventory
    // - Mark voxels as collected
}

void SimulationTickLoop::place_voxels(Agent& agent) {
    // TODO: place voxels - build structures at agent position
    // For now, log the build action
    if (llm_bridge_) {
        std::string build_task = "Agent " + std::to_string(agent.id) +
            " is building at (" + std::to_string(agent.x) + ", " +
            std::to_string(agent.y) + ", " + std::to_string(agent.z) + ")";
        // In real implementation, this would modify the voxel chunk
    }
    
    // In real implementation:
    // - Get target chunk from world position
    // - Set voxel type at local coordinates
    // - Mark chunk as dirty for SVO rebuild
    // - Consume resources from agent inventory
}

void SimulationTickLoop::meso_tick(float delta_time) {
    // Form groups based on proximity and relation values
    form_groups();
    
    // Update group dynamics
    for (auto& group : groups_) {
        // Calculate average pillar values
        if (group.agent_ids.empty()) continue;
        
        float sum[NUM_PILLARS] = {0};
        for (int agent_id : group.agent_ids) {
            if (agent_id >= 0 && agent_id < static_cast<int>(agents_.size())) {
                const auto& pillars = agents_[agent_id].cognition->get_pillars();
                for (int i = 0; i < NUM_PILLARS; i++) {
                    sum[i] += pillars[i];
                }
            }
        }
        
        for (int i = 0; i < NUM_PILLARS; i++) {
            group.avg_pillar_values[i] = sum[i] / group.agent_ids.size();
        }
        
        // Cohesion affects group stability
        group.cohesion_level = group.avg_pillar_values[6];  // Cohesion pillar
    }
}

void SimulationTickLoop::form_groups() {
    groups_.clear();
    spatial_hash_.clear();
    
    // Simple grouping by proximity (same chunk)
    for (size_t i = 0; i < agents_.size(); i++) {
        if (!agents_[i].active) continue;
        int cx, cy, cz;
        get_cell_coords(agents_[i].x, agents_[i].y, agents_[i].z, cx, cy, cz);
        SpatialHashKey key = hash_cell(cx, cy, cz);
        spatial_hash_[key].push_back(static_cast<int>(i));
    }
}

void SimulationTickLoop::macro_tick(float delta_time) {
    // Update world state
    update_world(delta_time);
    
    // Update pillar vectors for all agents
    update_pillar_vectors();
}

void SimulationTickLoop::update_world(float delta_time) {
    // Simple environmental changes
    world_state_.temperature += 0.001f * sin(world_state_.tick_count * 0.1f);
    world_state_.hazard_level *= 0.99f;  // Hazard slowly decreases
    world_state_.resource_density -= 0.0001f;  // Resources slowly deplete
}

void SimulationTickLoop::update_pillar_vectors() {
    // Apply environmental effects to all agents
    for (auto& agent : agents_) {
        if (!agent.active) continue;
        
        auto& pillars = agent.cognition->get_pillars();
        
        // High hazard increases Harm
        pillars[PILLAR_HARM] = std::clamp(pillars[PILLAR_HARM] + world_state_.hazard_level * 0.01f, 0.0f, 1.0f);
        
        // Resources affect Attraction
        pillars[PILLAR_ATTRACTION] = std::clamp(world_state_.resource_density, 0.0f, 1.0f);
    }
}

int SimulationTickLoop::add_agent(float x, float y, float z) {
    Agent agent;
    agent.id = static_cast<int>(agents_.size());
    agent.x = x;
    agent.y = y;
    agent.z = z;
    agent.vx = 0.0f;
    agent.vy = 0.0f;
    agent.vz = 0.0f;
    agent.cognition = std::make_unique<AgentCognition>(agent.id);
    agent.active = true;
    agent.last_hash_x = x;
    agent.last_hash_y = y;
    agent.last_hash_z = z;
    
    agents_.push_back(std::move(agent));
    spatial_hash_dirty_ = true;  // New agent added, rebuild hash
    return agents_.back().id;
}

void SimulationTickLoop::remove_agent(int agent_id) {
    if (agent_id >= 0 && agent_id < static_cast<int>(agents_.size())) {
        if (agents_[agent_id].active) {
            agents_[agent_id].active = false;
            spatial_hash_dirty_ = true;  // Agent removed, rebuild hash
        }
    }
}

void SimulationTickLoop::update_chunks() {
    for (auto& chunk : chunks_) {
        if (chunk && chunk->is_dirty()) {
            chunk->update_svo();
        }
    }
}