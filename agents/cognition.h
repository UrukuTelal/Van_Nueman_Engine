#ifndef VAN_NUEMAN_AGENTS_COGNITION_H
#define VAN_NUEMAN_AGENTS_COGNITION_H

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <memory>
#include "../api/llm_bridge.h"
#include "../include/Entity.h"
#include "agent_info.h"

using PillarVector = std::array<float, NUM_PILLARS>;

enum class AgentAction {
    IDLE,
    MOVE,
    GATHER,
    BUILD,
    COMMUNICATE,
    DEFEND,
    FLEE,
    EXPLORE
};

struct CognitiveState {
    PillarVector pillars;
    float distortion_level;
    uint32_t tick_count;
    std::string last_reasoning;
};

class AgentCognition {
public:
    AgentCognition(int agent_id);
    ~AgentCognition();
    
    // Perception phase: update from environment
    void perceive_voxels(const float* voxel_data, uint32_t voxel_count);
    void perceive_agents(const AgentInfo* nearby_agents, uint32_t agent_count);
    void perceive_environment(float temperature, float hazard_level, float resource_density);
    
    // Update phase: internal state recalculation
    void update(float delta_time);
    
    // Decision phase: choose action
    AgentAction decide();
    
    // LLM integration for complex reasoning
    void query_llm(const std::string& task, LLMBridge& bridge);
    
    // Memory system
    void store_memory(const CognitiveState& state);
    CognitiveState recall_memory(uint32_t ticks_ago) const;
    void prune_memory(uint32_t max_entries = 100);
    
    // Distortion detection (Pillar 13)
    float detect_distortion() const;
    bool is_distorted() const;
    
    // Getters
    const PillarVector& get_pillars() const { return current_state_.pillars; }
    PillarVector& get_pillars() { return current_state_.pillars; }
    int get_agent_id() const { return agent_id_; }
    
private:
    int agent_id_;
    CognitiveState current_state_;
    std::vector<CognitiveState> memory_;
    
    // Helper functions
    void apply_pillar_constraints();
    float calculate_harm_delta() const;
    void update_pillar_from_llm(const PillarVector& new_values);
};

#endif // VAN_NUEMAN_AGENTS_COGNITION_H
