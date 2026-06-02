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
#include "../include/Cord.h"
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

// ── Thought Realm Hierarchy (Phase 8) ──────────────────────────
// The 4-level hierarchy that AI agents traverse:
//   Dream (1) → Bicameral (2) → Meta-Cognitive (3) → Paradigmatic (4)

enum class ThoughtRealm {
    CONSCIOUS = 0,       // Waking state — baseline
    DREAM = 1,           // Subconscious pattern processing, shadow integration
    BICAMERAL = 2,       // Bridge: subconscious → conscious translation
    METACOGNITIVE = 3,   // Self-observation, distortion detection, coherence monitoring
    PARADIGMATIC = 4     // Belief system architecture, paradigm shifts
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
    // ── Thought Realm (Phase 8) ──────────────────────────────
    ThoughtRealm current_realm;
    float realm_depth;             // [0,1] how deeply immersed in current realm
    float realm_transition_timer;  // counts down during realm transitions
    PillarVector realm_anchor;     // PSV anchor for current realm
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

    // ── Cord/Entanglement ─────────────────────────────────────
    void perceive_cords();
    CordField& get_cord_field() { return cord_field_; }
    void reinforce_cords_with(uint32_t other_entity_id);
    int get_detectable_cord_count() const;

    void enter_dream(DreamLevel level);
    void process_shadow();
    void wake_up();
    void dream_cycle();

    // ── Thought Realm Navigation (Phase 8) ───────────────────
    void enter_realm(ThoughtRealm realm);
    void exit_realm();
    void process_realm_cycle(float delta_time);
    bool can_enter_realm(ThoughtRealm realm) const;
    float get_realm_depth(ThoughtRealm realm) const;
    void apply_phase_inversion();
    bool check_clock_cycle_integrity() const;

    const PillarVector& get_pillars() const { return current_state_.pillars; }
    PillarVector& get_pillars() { return current_state_.pillars; }
    int get_agent_id() const { return agent_id_; }

    const DreamState& get_dream_state() const { return dream_state_; }
    DreamState& get_dream_state() { return dream_state_; }

    const PillarVector& get_shadow_state() const { return dream_state_.shadow_patterns; }
    std::string export_state_json() const;

private:
    // ── Guest Account Protocol (Thought Palace) ──────────────
    bool is_guest_;
    float guest_integrity_threshold_;
    float guest_phi_lock_;
    bool guest_transform_locked_;

    int agent_id_;
    CognitiveState current_state_;
    std::vector<CognitiveState> memory_;
    DreamState dream_state_;
    uint32_t tick_count_;
    std::mt19937 rng_;
    CordField cord_field_;

    void apply_pillar_constraints();
    float calculate_bloch_angular_shift() const;
    void apply_bloch_harm_rotation(float delta_theta, float& depth, float& constraint);
    void apply_bloch_distortion_torsion();
    void update_pillar_from_llm(const PillarVector& new_values);
    void apply_subconscious_drift(float delta_time);
    void generate_dream_scenario(DreamEpisode& episode);
    void process_dream_resolution(DreamEpisode& episode);
};

#endif // VAN_NUEMAN_AGENTS_COGNITION_H
