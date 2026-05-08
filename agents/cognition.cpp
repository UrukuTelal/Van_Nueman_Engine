#include "cognition.h"
#include <algorithm>
#include <cmath>
#include <iostream>

AgentCognition::AgentCognition(int agent_id) 
    : agent_id_(agent_id) {
    // Initialize with default pillar values (center)
    current_state_.pillars.fill(0.5f);
    current_state_.distortion_level = 0.0f;
    current_state_.tick_count = 0;
    
    // Set some initial variations per agent
    current_state_.pillars[0] = 0.7f;  // Awareness - high
    current_state_.pillars[1] = 0.6f;  // Willpower
    current_state_.pillars[2] = 0.5f;  // Force
    current_state_.pillars[12] = 0.1f; // Harm - low
    current_state_.pillars[13] = 0.05f; // Distortion - very low
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
    current_state_.tick_count++;
    
    // Apply pillar constraints
    apply_pillar_constraints();
    
    // Detect distortion
    current_state_.distortion_level = detect_distortion();
    
    // Store state in memory
    store_memory(current_state_);
}

void AgentCognition::apply_pillar_constraints() {
    // Harm constraint: if Harm > 0.7, reduce other pillars
    if (current_state_.pillars[12] > 0.7f) {
        for (int i = 0; i < 14; i++) {
            if (i != 12) {
                current_state_.pillars[i] *= 0.95f;
            }
        }
    }
    
    // Distortion reduces Awareness effectiveness
    float distortion_factor = 1.0f - current_state_.pillars[13];
    current_state_.pillars[0] *= distortion_factor;
}

float AgentCognition::detect_distortion() const {
    // Simple distortion detection
    float distortion = 0.0f;
    
    // Check for inconsistent pillar values
    if (current_state_.pillars[2] > 0.8f && current_state_.pillars[4] < 0.2f) {
        distortion += 0.3f;  // High Force but low Resistance
    }
    
    if (current_state_.pillars[12] > 0.5f && current_state_.pillars[5] > 0.8f) {
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
    for (int i = 0; i < 14; i++) {
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
