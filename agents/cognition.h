#ifndef VAN_NUEMAN_AGENTS_COGNITION_H
#define VAN_NUEMAN_AGENTS_COGNITION_H

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <random>
#include "agent_info.h"
#include "../include/Entity.h"
#include "../api/llm_bridge.h"

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

enum class DreamLevel {
    LIGHT = 0,
    DEEP = 1,
    LUCID = 2
};

enum class ResolutionState {
    UNRESOLVED,
    PARTIAL,
    COMPLETE,
    ABORTED
};

struct DreamEpisode {
    std::string scenario;
    float emotional_valence;
    ResolutionState resolution;
    uint32_t duration_ticks;
    PillarVector simulated_pillars;
};

struct DreamState {
    PillarVector shadow_patterns;
    float lucid_level;
    uint32_t rem_cycles;
    std::vector<DreamEpisode> dream_content;
    bool is_dreaming;
    DreamLevel current_level;
    uint32_t dream_start_tick;
    float subconscious_drift_rate;
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

    void perceive_voxels(const float* voxel_data, uint32_t voxel_count);
    void perceive_agents(const AgentInfo* nearby_agents, uint32_t agent_count);
    void perceive_environment(float temperature, float hazard_level, float resource_density);

    void update(float delta_time);

    AgentAction decide();

    void query_llm(const std::string& task, LLMBridge& bridge);

    void store_memory(const CognitiveState& state);
    CognitiveState recall_memory(uint32_t ticks_ago) const;
    void prune_memory(uint32_t max_entries = 100);

    float detect_distortion() const;
    bool is_distorted() const;

    void enter_dream(DreamLevel level);
    void process_shadow();
    void wake_up();
    void dream_cycle();

    const PillarVector& get_pillars() const { return current_state_.pillars; }
    PillarVector& get_pillars() { return current_state_.pillars; }
    int get_agent_id() const { return agent_id_; }

    const DreamState& get_dream_state() const { return dream_state_; }
    DreamState& get_dream_state() { return dream_state_; }

    const PillarVector& get_shadow_state() const { return current_state_.pillars; }

private:
    int agent_id_;
    CognitiveState current_state_;
    std::vector<CognitiveState> memory_;
    DreamState dream_state_;
    uint32_t tick_count_;
    std::mt19937 rng_;

    void apply_pillar_constraints();
    float calculate_harm_delta() const;
    void update_pillar_from_llm(const PillarVector& new_values);
    void apply_subconscious_drift(float delta_time);
    void generate_dream_scenario(DreamEpisode& episode);
    void process_dream_resolution(DreamEpisode& episode);
};

#endif // VAN_NUEMAN_AGENTS_COGNITION_H
