#ifndef VAN_NUEMAN_SIMULATION_TICK_LOOP_H
#define VAN_NUEMAN_SIMULATION_TICK_LOOP_H

#include <cstdint>
#include <vector>
#include <array>
#include <memory>
#include <unordered_map>
#include <cmath>
#include "../agents/cognition.h"
#include "../scene/chunk.h"

constexpr int MICRO_TICKS_PER_SECOND = 60;
constexpr int MESO_TICKS_PER_SECOND = 10;
constexpr int MACRO_TICKS_PER_SECOND = 1;

constexpr float INTERACTION_RADIUS = 10.0f;
constexpr float SPATIAL_HASH_CELL_SIZE = 15.0f; // Slightly larger than interaction radius for neighbor checks

struct Agent {
    int id;
    float x, y, z;
    float vx, vy, vz;
    std::unique_ptr<AgentCognition> cognition;
    bool active;
    float last_hash_x, last_hash_y, last_hash_z;  // Position when spatial hash was last built
};

struct Group {
    int id;
    std::vector<int> agent_ids;
    float cohesion_level;
    float avg_pillar_values[NUM_PILLARS];
};

struct WorldState {
    float temperature;
    float hazard_level;
    float resource_density;
    int tick_count;
};

class SimulationTickLoop {
public:
    SimulationTickLoop();
    ~SimulationTickLoop();
    
    // Initialize simulation
    bool initialize(int max_agents = 1000);
    
    // Main tick function (call at 60Hz)
    void tick(float delta_time);
    
    // Get current state
    const WorldState& get_world_state() const { return world_state_; }
    const std::vector<Agent>& get_agents() const { return agents_; }
    
    // Add/remove agents
    int add_agent(float x, float y, float z);
    void remove_agent(int agent_id);
    
    // Update chunk system
    void update_chunks();
    
private:
    // Scale-specific updates
    void micro_tick(float delta_time);   // 60Hz - agent decisions
    void meso_tick(float delta_time);     // 10Hz - group dynamics
    void macro_tick(float delta_time);    // 1Hz - world evolution
    
    // Helper functions
    void update_agent(Agent& agent, float delta_time);
    void update_agent_perception(Agent& agent);  // Get voxels + nearby agents
    void gather_resources(Agent& agent);          // Gather resources from voxels
    void place_voxels(Agent& agent);             // Place/build voxels
    void form_groups();
    void update_world(float delta_time);
    void update_pillar_vectors();
    
    // State
    std::vector<Agent> agents_;
    std::vector<Group> groups_;
    std::vector<std::unique_ptr<Chunk>> chunks_;
    WorldState world_state_;
    
    // Tick counters
    uint32_t micro_tick_count_;
    uint32_t meso_tick_count_;
    uint32_t macro_tick_count_;
    
    // LLM bridge for complex decisions
    std::unique_ptr<LLMBridge> llm_bridge_;
    
    // Spatial hashing for O(n) nearby agent lookups
    using SpatialHashKey = uint64_t;
    std::unordered_map<SpatialHashKey, std::vector<int>> spatial_hash_;
    bool spatial_hash_dirty_ = true;  // Rebuild only when agents move enough
    
    static SpatialHashKey hash_cell(int cx, int cy, int cz);
    static void get_cell_coords(float x, float y, float z, int& cx, int& cy, int& cz);
    void build_spatial_hash();
    void query_nearby_agents(const Agent& agent, int* nearby_indices, int max_nearby, int& count) const;
};

#endif // VAN_NUEMAN_SIMULATION_TICK_LOOP_H
