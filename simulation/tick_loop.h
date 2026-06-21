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
#include "attention_loop.h"
#include "../voxel/FracturePipeline.h"
#include "../physics/voxel_coupling.h"
#include "../include/simulation/AgentECS.h"
#include "transform_compute.h"
#include "hopf_compute.h"
#include "cuda_pillar_compute.h"
#ifndef VN_USE_NATIVE_REASONING
#include "llm_worker.h"
#endif
#include "native_reasoning_worker.h"
#include "../include/HopfPID.h"
#include "../include/BlochSpace.h"

constexpr int MICRO_TICKS_PER_SECOND = 60;
constexpr int MESO_TICKS_PER_SECOND = 10;
constexpr int MACRO_TICKS_PER_SECOND = 1;

constexpr float INTERACTION_RADIUS = 10.0f;
constexpr float SPATIAL_HASH_CELL_SIZE = 15.0f; // Slightly larger than interaction radius for neighbor checks

// Agent data is now stored in the AgentECS (Structure-of-Arrays) for cache efficiency.
// The SimulationTickLoop no longer maintains a vector of Agent structs.

struct Group {
    int id;
    std::vector<int> agent_ids;
    float cohesion_level;
    float avg_pillar_values[NumPillars];
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
    
    // Initialize GPU compute backend (optional - if null, CPU fallback is used)
    bool init_gpu(VkDevice device, VkPhysicalDevice physicalDevice,
                  VkQueue computeQueue, uint32_t computeFamily,
                  uint32_t maxEntities = 4096);

    // Main tick function (call at 60Hz)
    void tick(float delta_time);
    
    // Get current state
    const WorldState& get_world_state() const { return world_state_; }
    // Note: We no longer expose the agents as a vector of Agent structs.
    // Instead, we provide access to the ECS for systems that need it.
    const vn::simulation::AgentECS& get_agent_ecs() const { return agent_ecs_; }
    vn::simulation::AgentECS& get_agent_ecs() { return agent_ecs_; }
    
    // Add/remove agents
    int add_agent(float x, float y, float z);
    void remove_agent(int agent_id);
    
#ifndef VN_USE_NATIVE_REASONING
    // Initialize async LLM worker (uses ZeroMQ + Ollama)
    bool init_llm_worker(const char* zmq_endpoint = "inproc://llm_worker");

    // Request LLM reasoning for an agent (non-blocking)
    bool request_agent_reasoning(vn::simulation::AgentECS::Index idx,
                                  const char* task = "Evaluate your current state and suggest new pillar values.");

    // Collect pending LLM responses and apply them to agents
    void collect_llm_responses();
#endif

    // Initialize native reasoning worker (replaces Ollama with ThoughtEngine)
    bool init_native_reasoning(float creativity = 0.2f, float coherence = 0.8f,
                               const char* zmq_endpoint = "inproc://native_reasoning");

    // Request native reasoning for an agent (non-blocking)
    bool request_native_reasoning(vn::simulation::AgentECS::Index idx);

    // Collect pending native reasoning responses
    void collect_native_responses();

    // Hopf-PID engine integration
    void hopf_tick_all(float delta_time, float hazard, float resources, bool apply_env = true);
    void hopf_ensure_state_size();
    HopfPIDState& get_hopf_state(vn::simulation::AgentECS::Index idx);
    static void hopf_apply_environment(HopfPIDState& state, float hazard, float resource_density);
    void hopf_transform_pairs();

    // Update chunk system
    void update_chunks();
    
private:
    // Scale-specific updates
    void micro_tick(float delta_time);   // 60Hz - agent decisions
    void meso_tick(float delta_time);     // 10Hz - group dynamics
    void macro_tick(float delta_time);    // 1Hz - world evolution
    
    // Helper functions
    void update_agent(vn::simulation::AgentECS::Index idx, float delta_time);
    void update_agent_perception(vn::simulation::AgentECS::Index idx);  // Get voxels + nearby agents
    void gather_resources(vn::simulation::AgentECS::Index idx);          // Gather resources from voxels
    void place_voxels(vn::simulation::AgentECS::Index idx);             // Place/build voxels
    void fracture_tick(float delta_time);         // Agent→voxel fracture coupling
    void form_groups();
    void update_world(float delta_time);
    void update_pillar_vectors();
    
    // State
    vn::simulation::AgentECS agent_ecs_;
    std::vector<Group> groups_;
    std::vector<std::unique_ptr<Chunk>> chunks_;
    WorldState world_state_;
    
    // Tick counters
    uint32_t micro_tick_count_;
    uint32_t meso_tick_count_;
    uint32_t macro_tick_count_;
    
    // Current delta_time for GPU dispatch paths
    float dt_;
    
    // GPU CUDA pillar compute (optional - CPU fallback if null)
    std::unique_ptr<CUDAPillarCompute> cuda_compute_;

    // GPU TRANSFORM compute (optional - CPU fallback if null)
    std::unique_ptr<TransformCompute> transform_compute_;

    // GPU Hopf-PID compute (optional - CPU fallback if null)
    std::unique_ptr<HopfCompute> hopf_compute_;

    // Hopf-PID per-agent state (parallel to ECS)
    std::vector<HopfPIDState> hopf_states_;
    CordField hopf_cord_field_;

#ifndef VN_USE_NATIVE_REASONING
    // Async LLM worker (ZeroMQ background thread + Ollama)
    std::unique_ptr<LLMWorker> llm_worker_;
#endif

    // Native reasoning worker (ZeroMQ background thread + ThoughtEngine)
    std::unique_ptr<NativeReasoningWorker> native_worker_;
    
    // Attention attenuation system
    AttentionLoop attention_loop_;
    InfluenceBuffer influence_buf_;
    
    // Bloch-space Depth monitoring & crystallized sub-pillar system (per-agent)
    std::vector<BlochConductor> bloch_conductors_;

    // Voxel deformation coupling (entity pillars → voxel physics)
    DeformationMap deformation_map_;
    
    // Spatial hashing for O(n) nearby agent lookups
    using SpatialHashKey = uint64_t;
    std::unordered_map<SpatialHashKey, std::vector<int>> spatial_hash_;
    bool spatial_hash_dirty_ = true;  // Rebuild only when agents move enough
    
    static SpatialHashKey hash_cell(int cx, int cy, int cz);
    static void get_cell_coords(float x, float y, float z, int& cx, int& cy, int& cz);
    void build_spatial_hash();
    void query_nearby_agents(vn::simulation::AgentECS::Index self_idx, int* nearby_indices, int max_nearby, int& count) const;
};

#endif // VAN_NUEMAN_SIMULATION_TICK_LOOP_H
