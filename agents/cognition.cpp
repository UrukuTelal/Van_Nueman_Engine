#include "cognition.h"
#include <algorithm>
#include <cmath>
#include <iostream>

AgentCognition::AgentCognition(int agent_id)
    : agent_id_(agent_id), tick_count_(0), rng_(std::random_device{}()) {
    current_state_.pillars.fill(0.5f);
    current_state_.distortion_level = 0.0f;
    current_state_.tick_count = 0;

    current_state_.pillars[PILLAR_AWARENESS] = 0.7f;
    current_state_.pillars[PILLAR_WILLPOWER] = 0.6f;
    current_state_.pillars[PILLAR_FORCE] = 0.5f;
    current_state_.pillars[PILLAR_HARM] = 0.1f;
    current_state_.pillars[PILLAR_DISTORTION] = 0.05f;

    dream_state_.shadow_patterns.fill(0.0f);
    dream_state_.lucid_level = 0.0f;
    dream_state_.rem_cycles = 0;
    dream_state_.is_dreaming = false;
    dream_state_.current_level = DreamLevel::LIGHT;
    dream_state_.dream_start_tick = 0;
    dream_state_.subconscious_drift_rate = 0.001f;

    for (int i = 0; i < 4; i++) {
        dream_state_.shadow_patterns[i] = 0.3f + (static_cast<float>(rng_()) / rng_.max()) * 0.2f;
    }
}

AgentCognition::~AgentCognition() = default;

void AgentCognition::perceive_voxels(const float* voxel_data, uint32_t voxel_count) {
    if (voxel_count == 0) return;
    
    // Update Awareness based on voxel data availability
    float data_density = std::min(1.0f, voxel_count / 100.0f);
    current_state_.pillars[0] = std::clamp(
        current_state_.pillars[0] * 0.8f + data_density * 0.2f, 0.0f, 1.0f);
}

void AgentCognition::perceive_agents(const AgentInfo* nearby_agents, uint32_t agent_count) {
    if (agent_count == 0) {
        // No nearby agents - reduce Relation
        current_state_.pillars[7] = std::max(0.1f, current_state_.pillars[7] - 0.05f);
        return;
    }
    
    // Update Relation and Cohesion based on nearby agents
    float relation_boost = std::min(0.3f, agent_count * 0.05f);
    current_state_.pillars[7] = std::clamp(
        current_state_.pillars[7] + relation_boost, 0.0f, 1.0f);
    current_state_.pillars[6] = std::clamp(
        current_state_.pillars[6] + relation_boost * 0.5f, 0.0f, 1.0f);
}

void AgentCognition::perceive_environment(float temperature, float hazard_level, float resource_density) {
    // High hazard increases Harm
    current_state_.pillars[12] = std::clamp(
        current_state_.pillars[12] + hazard_level * 0.1f, 0.0f, 1.0f);
    
    // Resources affect Attraction
    current_state_.pillars[11] = std::clamp(
        resource_density, 0.0f, 1.0f);
}

void AgentCognition::update(float delta_time) {
    tick_count_++;
    current_state_.tick_count = tick_count_;

    apply_pillar_constraints();

    current_state_.distortion_level = detect_distortion();

    dream_cycle();

    apply_subconscious_drift(delta_time);

    store_memory(current_state_);
}

void AgentCognition::apply_pillar_constraints() { 
    // Harm constraint: if Harm > 0.7, reduce other pillars
    if (current_state_.pillars[PILLAR_HARM] > 0.7f) {
        for (int i = 0; i < NUM_PILLARS; i++) {
            if (i != PILLAR_HARM) {
                current_state_.pillars[i] *= 0.95f;
            }
        }
    }
    
    // Distortion reduces Awareness effectiveness
    float distortion_factor = 1.0f - current_state_.pillars[PILLAR_DISTORTION];
    current_state_.pillars[PILLAR_AWARENESS] *= distortion_factor;
}

float AgentCognition::detect_distortion() const {
    // Simple distortion detection
    float distortion = 0.0f;
    
    // Check for inconsistent pillar values
    if (current_state_.pillars[PILLAR_FORCE] > 0.8f && current_state_.pillars[PILLAR_RESISTANCE] < 0.2f) {
        distortion += 0.3f;  // High Force but low Resistance
    }
    
    if (current_state_.pillars[PILLAR_HARM] > 0.5f && current_state_.pillars[PILLAR_INTEGRITY] > 0.8f) {
        distortion += 0.2f;  // High Harm but high Integrity (inconsistent)
    }
    
    return std::clamp(distortion, 0.0f, 1.0f);
}

bool AgentCognition::is_distorted() const {
    return current_state_.distortion_level > 0.5f;
}

AgentAction AgentCognition::decide() {
    // Simple rule-based decision making
    
    // High Harm - flee or defend
    if (current_state_.pillars[12] > 0.6f) {
        return current_state_.pillars[4] > 0.5f ? AgentAction::DEFEND : AgentAction::FLEE;
    }
    
    // High distortion - idle (paralysis)
    if (is_distorted()) {
        return AgentAction::IDLE;
    }
    
    // High Willpower + Force - build or gather
    if (current_state_.pillars[1] > 0.7f && current_state_.pillars[2] > 0.6f) {
        return AgentAction::BUILD;
    }
    
    // High Attraction + low resources nearby
    if (current_state_.pillars[11] > 0.6f) {
        return AgentAction::GATHER;
    }
    
    // High Awareness + low Relation - explore
    if (current_state_.pillars[0] > 0.6f && current_state_.pillars[7] < 0.4f) {
        return AgentAction::EXPLORE;
    }
    
    // Default: move randomly
    return AgentAction::MOVE;
}

void AgentCognition::query_llm(const std::string& task, LLMBridge& bridge) {
    LLMRequest request;
    request.model = "qwen2.5:0.5b";  // Use small model for quick reasoning
    request.prompt = task;
    request.current_state = current_state_.pillars;
    request.temperature = 0.7f;
    request.max_tokens = 256;
    
    LLMResponse response = bridge.query(request);
    if (response.success) {
        current_state_.last_reasoning = response.content;
        update_pillar_from_llm(response.updated_state);
    }
}

void AgentCognition::update_pillar_from_llm(const PillarVector& new_values) {
    // Blend current state with LLM suggestion
    for (int i = 0; i < NUM_PILLARS; i++) {
        current_state_.pillars[i] = current_state_.pillars[i] * 0.7f + new_values[i] * 0.3f;
    }
}

void AgentCognition::store_memory(const CognitiveState& state) {
    memory_.push_back(state);
    if (memory_.size() > 100) {
        memory_.erase(memory_.begin());
    }
}

CognitiveState AgentCognition::recall_memory(uint32_t ticks_ago) const {
    if (ticks_ago >= memory_.size()) {
        return memory_.front();
    }
    return memory_[memory_.size() - 1 - ticks_ago];
}

void AgentCognition::prune_memory(uint32_t max_entries) {
    if (memory_.size() > max_entries) {
        memory_.erase(memory_.begin(), memory_.begin() + (memory_.size() - max_entries));
    }
}

float AgentCognition::calculate_harm_delta() const {
    if (memory_.empty()) return 0.0f;

    const auto& prev = memory_.back();
    float delta = current_state_.pillars[12] - prev.pillars[12];
    return std::max(0.0f, delta - current_state_.pillars[4] - current_state_.pillars[5]);
}

void AgentCognition::enter_dream(DreamLevel level) {
    if (dream_state_.is_dreaming) return;

    dream_state_.is_dreaming = true;
    dream_state_.current_level = level;
    dream_state_.dream_start_tick = tick_count_;

    switch (level) {
        case DreamLevel::LIGHT:
            dream_state_.lucid_level = 0.3f;
            break;
        case DreamLevel::DEEP:
            dream_state_.lucid_level = 0.6f;
            break;
        case DreamLevel::LUCID:
            dream_state_.lucid_level = 1.0f;
            break;
    }

    DreamEpisode episode;
    episode.scenario = "Entering dream state";
    episode.emotional_valence = 0.5f;
    episode.resolution = ResolutionState::UNRESOLVED;
    episode.duration_ticks = 0;
    episode.simulated_pillars = current_state_.pillars;
    dream_state_.dream_content.push_back(episode);
}

void AgentCognition::process_shadow() {
    if (!dream_state_.is_dreaming) return;

    float influence = dream_state_.lucid_level * 0.1f;

    for (int i = 0; i < NUM_PILLARS; i++) {
        float shadow_delta = (current_state_.pillars[i] - dream_state_.shadow_patterns[i]) * influence;
        dream_state_.shadow_patterns[i] += shadow_delta * 0.05f;
        dream_state_.shadow_patterns[i] = std::clamp(dream_state_.shadow_patterns[i], 0.0f, 1.0f);
    }

    if (dream_state_.rem_cycles < 10) {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        if (dist(rng_) < 0.1f) {
            dream_state_.rem_cycles++;
        }
    }
}

void AgentCognition::wake_up() {
    if (!dream_state_.is_dreaming) return;

    for (int i = 0; i < NUM_PILLARS; i++) {
        float integration = dream_state_.shadow_patterns[i] * (1.0f - dream_state_.lucid_level * 0.5f);
        current_state_.pillars[i] = current_state_.pillars[i] * 0.7f + integration * 0.3f;
    }

    if (!dream_state_.dream_content.empty()) {
        auto& last_episode = dream_state_.dream_content.back();
        last_episode.resolution = ResolutionState::COMPLETE;
        last_episode.duration_ticks = tick_count_ - dream_state_.dream_start_tick;
    }

    dream_state_.is_dreaming = false;
    dream_state_.lucid_level = 0.0f;
}

void AgentCognition::dream_cycle() {
    if (!dream_state_.is_dreaming) {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        if (dist(rng_) < 0.02f) {
            enter_dream(DreamLevel::LIGHT);
        }
        return;
    }

    if (dream_state_.dream_content.empty()) return;

    auto& current_episode = dream_state_.dream_content.back();
    generate_dream_scenario(current_episode);
    process_shadow();
    process_dream_resolution(current_episode);

    if (current_episode.duration_ticks > 50 && current_episode.resolution != ResolutionState::UNRESOLVED) {
        if (dream_state_.lucid_level < 1.0f && dream_state_.rem_cycles > 3) {
            dream_state_.current_level = DreamLevel::DEEP;
            dream_state_.lucid_level = 0.6f;
        }
    }
}

void AgentCognition::apply_subconscious_drift(float delta_time) {
    if (tick_count_ < 100) return;

    float drift = dream_state_.subconscious_drift_rate * delta_time;

    for (int i = 0; i < NUM_PILLARS; i++) {
        if (i < 4) {
            float historical_avg = 0.5f;
            if (memory_.size() >= 10) {
                size_t start_idx = memory_.size() - 10;
                float sum = 0.0f;
                for (size_t j = start_idx; j < memory_.size(); j++) {
                    sum += memory_[j].pillars[i];
                }
                historical_avg = sum / 10.0f;
            }

            float drift_direction = (historical_avg - current_state_.pillars[i]) * drift;
            dream_state_.shadow_patterns[i] += drift_direction * 0.1f;
            dream_state_.shadow_patterns[i] = std::clamp(dream_state_.shadow_patterns[i], 0.0f, 1.0f);
        }
    }
}

void AgentCognition::generate_dream_scenario(DreamEpisode& episode) {
    std::uniform_int_distribution<int> scenario_dist(0, 5);
    int scenario_type = scenario_dist(rng_);

    switch (scenario_type) {
        case 0:
            episode.scenario = "Exploring unknown territory";
            episode.emotional_valence = 0.6f;
            break;
        case 1:
            episode.scenario = "Encountering other agents";
            episode.emotional_valence = 0.5f;
            break;
        case 2:
            episode.scenario = "Building or constructing";
            episode.emotional_valence = 0.7f;
            break;
        case 3:
            episode.scenario = "Facing hazardous situation";
            episode.emotional_valence = 0.3f;
            break;
        case 4:
            episode.scenario = "Resource gathering";
            episode.emotional_valence = 0.6f;
            break;
        case 5:
            episode.scenario = "Social interaction";
            episode.emotional_valence = 0.5f;
            break;
    }

    for (int i = 0; i < NUM_PILLARS; i++) {
        std::uniform_real_distribution<float> perturbation(-0.1f, 0.1f);
        episode.simulated_pillars[i] = current_state_.pillars[i] + perturbation(rng_);
        episode.simulated_pillars[i] = std::clamp(episode.simulated_pillars[i], 0.0f, 1.0f);
    }
}

void AgentCognition::process_dream_resolution(DreamEpisode& episode) {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    float resolution_chance = 0.3f + dream_state_.lucid_level * 0.4f;

    if (dist(rng_) < resolution_chance) {
        if (episode.emotional_valence > 0.6f) {
            episode.resolution = ResolutionState::COMPLETE;
        } else if (episode.emotional_valence > 0.4f) {
            episode.resolution = ResolutionState::PARTIAL;
        } else {
            episode.resolution = ResolutionState::ABORTED;
        }
    }

    episode.duration_ticks++;
}
