#ifndef VAN_NUEMAN_AGENTS_PERCEPTION_H
#define VAN_NUEMAN_AGENTS_PERCEPTION_H

#include <cstdint>
#include <array>
#include <vector>
#include <memory>
#include "../scene/chunk.h"

struct PerceptionResult {
    float voxel_density;        // 0.0 - 1.0
    int material_count[256];    // Count per material type
    float avg_r, avg_g, avg_b; // Average color
    bool has_voxels;
};

class AgentPerception {
public:
    AgentPerception(float view_distance = 50.0f);
    
    // Voxel sampling in radius
    PerceptionResult sample_voxels(float x, float y, float z,
                                   const std::vector<std::unique_ptr<Chunk>>& chunks);
    
    // Line-of-sight check (raycast)
    bool line_of_sight(float start_x, float start_y, float start_z,
                       float end_x, float end_y, float end_z,
                       const std::vector<std::unique_ptr<Chunk>>& chunks);
    
    // Agent proximity detection
    template<typename AgentIter>
    void detect_nearby_agents(float x, float y, float z, float radius,
                                AgentIter begin, AgentIter end,
                                std::vector<int>& out_agent_ids);
    
    // Environmental scanning
    void scan_environment(float x, float y, float z,
                          float& temperature, float& hazard, float& resources);
    
    void set_view_distance(float distance) { view_distance_ = distance; }
    float get_view_distance() const { return view_distance_; }
    
private:
    float view_distance_;
    
    // Helper: get chunk containing world position
    static Chunk* get_chunk_at(float x, float y, float z,
                                 const std::vector<std::unique_ptr<Chunk>>& chunks);
    
    // Helper: world to chunk coords
    static void world_to_chunk(float x, float y, float z,
                               int& chunk_x, int& chunk_y, int& chunk_z,
                               int& voxel_x, int& voxel_y, int& voxel_z);
};

#endif // VAN_NUEMAN_AGENTS_PERCEPTION_H
