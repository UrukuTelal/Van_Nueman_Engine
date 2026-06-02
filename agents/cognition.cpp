#include "cognition.h"
#include "../scale/SemanticProjection.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

AgentCognition::AgentCognition(int agent_id)
    : agent_id_(agent_id), tick_count_(0), rng_(std::random_device{}()) {
    current_state_.pillars.fill(vn::fp20_t(0.5f));
    current_state_.distortion_level = 0.0f;
    current_state_.tick_count = 0;

    current_state_.pillars[PILLAR_AWARENESS] = vn::fp20_t(0.7f);
    current_state_.pillars[PILLAR_WILLPOWER] = vn::fp20_t(0.6f);
    current_state_.pillars[PILLAR_FORCE] = vn::fp20_t(0.5f);
    current_state_.pillars[PILLAR_HARM] = vn::fp20_t(0.1f);
    current_state_.pillars[PILLAR_DISTORTION] = vn::fp20_t(0.05f);

    dream_state_.shadow_patterns.fill(vn::fp20_t(0.0f));
    dream_state_.lucid_level = 0.0f;
    dream_state_.rem_cycles = 0;
    dream_state_.is_dreaming = false;
    dream_state_.current_level = DreamLevel::LIGHT;
    dream_state_.dream_start_tick = 0;
    dream_state_.subconscious_drift_rate = 0.001f;

    for (int i = 0; i < 4; i++) {
        dream_state_.shadow_patterns[i] = vn::fp20_t(0.3f + (static_cast<float>(rng_()) / rng_.max()) * 0.2f);
    }
    
    cord_field_.init();
}

AgentCognition::~AgentCognition() = default;

void AgentCognition::perceive_voxels(const float* voxel_data, uint32_t voxel_count) {
    if (voxel_count == 0) return;
    
    float data_density = std::min(1.0f, voxel_count / 100.0f);
    float current_val = current_state_.pillars[0].to_float();
    float theta = std::acos(2.0f * current_val - 1.0f);
    float delta_theta = (data_density - 0.5f) * 0.1f;
    float new_theta = theta + delta_theta;
    if (new_theta < 0.0f) new_theta = 0.0f;
    if (new_theta > 3.14159265f) new_theta = 3.14159265f;
    current_state_.pillars[0] = vn::fp20_t((std::cos(new_theta) + 1.0f) * 0.5f);
}

void AgentCognition::perceive_agents(const AgentInfo* nearby_agents, uint32_t agent_count) {
    if (agent_count == 0) {
        // No nearby agents - reduce social memory (Relation projection)
        float rel = current_state_.pillars[PILLAR_RELATION].to_float() - 0.05f;
        current_state_.pillars[PILLAR_RELATION] = vn::fp20_t(std::max(0.1f, rel));
        return;
    }
    
    // Update social memory and cohesion based on nearby agents
    float relation_boost = std::min(0.3f, agent_count * 0.05f);

    float rel_val = current_state_.pillars[PILLAR_RELATION].to_float();
    float rel_theta = std::acos(2.0f * rel_val - 1.0f);
    float rel_delta = relation_boost * 0.1f;
    float new_rel_theta = rel_theta + rel_delta;
    if (new_rel_theta < 0.0f) new_rel_theta = 0.0f;
    if (new_rel_theta > 3.14159265f) new_rel_theta = 3.14159265f;
    current_state_.pillars[PILLAR_RELATION] = vn::fp20_t((std::cos(new_rel_theta) + 1.0f) * 0.5f);

    float coh_val = current_state_.pillars[PILLAR_COHESION].to_float();
    float coh_theta = std::acos(2.0f * coh_val - 1.0f);
    float coh_delta = relation_boost * 0.05f;
    float new_coh_theta = coh_theta + coh_delta;
    if (new_coh_theta < 0.0f) new_coh_theta = 0.0f;
    if (new_coh_theta > 3.14159265f) new_coh_theta = 3.14159265f;
    current_state_.pillars[PILLAR_COHESION] = vn::fp20_t((std::cos(new_coh_theta) + 1.0f) * 0.5f);
}

void AgentCognition::perceive_environment(float temperature, float hazard_level, float resource_density) {
    (void)temperature;
    float harm_val = current_state_.pillars[PILLAR_HARM].to_float();
    float will_val = current_state_.pillars[PILLAR_WILLPOWER].to_float();
    float dep_val = current_state_.pillars[PILLAR_DEPTH].to_float();
    float theta = std::acos(2.0f * harm_val - 1.0f);
    float resistance = (will_val * dep_val > 1e-8f) ? (will_val * dep_val) : 1e-8f;
    float delta_theta = (hazard_level * 0.1f * harm_val) / resistance;
    if (delta_theta > 3.14159265f) delta_theta = 3.14159265f;
    float new_theta = theta + delta_theta;
    if (new_theta < 0.0f) new_theta = 0.0f;
    if (new_theta > 3.14159265f) new_theta = 3.14159265f;
    current_state_.pillars[PILLAR_HARM] = vn::fp20_t((std::cos(new_theta) + 1.0f) * 0.5f);
    
    // Resources affect curiosity (Attraction projection)
    current_state_.pillars[PILLAR_ATTRACTION] = vn::fp20_t(std::clamp(resource_density, 0.0f, 1.0f));
}

void AgentCognition::update(float delta_time) {
    tick_count_++;
    current_state_.tick_count = tick_count_;

    apply_pillar_constraints();

    current_state_.distortion_level = detect_distortion();

    perceive_cords();

    cord_field_.tick_all(delta_time);

    dream_cycle();

    apply_subconscious_drift(delta_time);

    store_memory(current_state_);
}

void AgentCognition::apply_pillar_constraints() { 
    const float PI = 3.14159265f;
    
    // Harm constraint: if Harm > 0.7, apply gentle Bloch rotation to other
    // pillars (rotating them toward constraint pole). This is NOT a scalar
    // reduction — it is a geometric rotation.
    if (current_state_.pillars[PILLAR_HARM].to_float() > 0.7f) {
        float harm_theta = std::acos(2.0f * current_state_.pillars[PILLAR_HARM].to_float() - 1.0f);
        float gentle_rotation = -0.05f;  // small rotation toward south pole
        for (int i = 0; i < NUM_PILLARS; i++) {
            if (i != PILLAR_HARM) {
                float theta = std::acos(2.0f * current_state_.pillars[i].to_float() - 1.0f);
                float new_theta = theta + gentle_rotation;
                if (new_theta < 0.0f) new_theta = 0.0f;
                if (new_theta > PI) new_theta = PI;
                current_state_.pillars[i] = vn::fp20_t((std::cos(new_theta) + 1.0f) * 0.5f);
            }
        }
    }
    
    // Distortion does NOT reduce Awareness. It twists the phi (phase)
    // angle of the Awareness Bloch sphere, making perception unreliable
    // without diminishing its raw magnitude.
    float distortion = current_state_.pillars[PILLAR_DISTORTION].to_float();
    if (distortion > 0.01f) {
        float theta = std::acos(2.0f * current_state_.pillars[PILLAR_AWARENESS].to_float() - 1.0f);
        float phi_twist = distortion * PI;
        float z_projection = std::cos(theta + phi_twist);
        current_state_.pillars[PILLAR_AWARENESS] = vn::fp20_t((z_projection + 1.0f) * 0.5f);
    }
}

float AgentCognition::detect_distortion() const {
    float distortion = 0.0f;
    
    // Project to cognitive layer and check for incoherence signals
    CognitiveProjection cp = CognitiveProjection::project(to_pillar_state(current_state_.pillars.data()));
    
    // High agency without identity coherence = unstable drive
    if (cp.agency > 0.7f && cp.identity_coherence < 0.3f) {
        distortion += 0.3f;
    }
    
    // High cognitive load with high identity coherence = suppressed dissonance
    if (cp.cognitive_load > 0.5f && cp.identity_coherence > 0.8f) {
        distortion += 0.2f;
    }
    
    return std::clamp(distortion, 0.0f, 1.0f);
}

bool AgentCognition::is_distorted() const {
    return current_state_.distortion_level > 0.5f;
}

AgentAction AgentCognition::decide() {
    CognitiveProjection cp = CognitiveProjection::project(to_pillar_state(current_state_.pillars.data()));
    
    // High cognitive load (Harm projection) - flee or defend
    if (cp.cognitive_load > 0.6f) {
        return cp.identity_coherence > 0.5f ? AgentAction::DEFEND : AgentAction::FLEE;
    }
    
    // High distortion - idle (paralysis)
    if (is_distorted()) {
        return AgentAction::IDLE;
    }
    
    // High agency (Willpower + Force projection) - build
    if (cp.agency > 0.65f) {
        return AgentAction::BUILD;
    }
    
    // High curiosity (Attraction projection) - gather
    if (cp.curiosity > 0.6f) {
        return AgentAction::GATHER;
    }
    
    // Detectable cords → communicate to reinforce
    if (get_detectable_cord_count() > 0 && cp.social_memory > 0.4f) {
        return AgentAction::COMMUNICATE;
    }
    
    // Low focus_inward (high outward awareness) + low social memory - explore
    if (cp.focus_inward < 0.4f && cp.social_memory < 0.4f) {
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
    // Blend current state with LLM suggestion — θ-space rotation delta (NOT Cartesian)
    for (int i = 0; i < NUM_PILLARS; i++) {
        float old_theta = pillar_value_to_theta(current_state_.pillars[i]).to_float();
        float suggestion_theta = pillar_value_to_theta(new_values[i]).to_float();
        float delta_theta = (suggestion_theta - old_theta) * 0.3f;
        float result_theta = old_theta + delta_theta;
        if (result_theta < 0.0f) result_theta = 0.0f;
        if (result_theta > 3.14159265f) result_theta = 3.14159265f;
        current_state_.pillars[i] = theta_to_pillar_value(vn::fp20_t(result_theta));
    }
}

void AgentCognition::store_memory(const CognitiveState& state) {
    memory_.push_back(state);
    if (memory_.size() > 100) {
        memory_.erase(memory_.begin());
    }
}

CognitiveState AgentCognition::recall_memory(uint32_t ticks_ago) const {
    if (memory_.empty()) {
        return CognitiveState{};
    }
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

void AgentCognition::perceive_cords() {
    Cord nearby[MAX_CORDS_PER_ENTITY];
    int count = cord_field_.get_for_entity(agent_id_, nearby, MAX_CORDS_PER_ENTITY);

    float awareness = current_state_.pillars[PILLAR_AWARENESS].to_float();
    int detectable = 0;

    for (int i = 0; i < count; i++) {
        if (nearby[i].is_detectable_by(awareness)) {
            detectable++;
            // Each detectable cord gently rotates Relation upward
            float rel_theta = pillar_value_to_theta(current_state_.pillars[PILLAR_RELATION]).to_float();
            float delta = nearby[i].strength * 0.01f;
            current_state_.pillars[PILLAR_RELATION] = apply_bloch_rotation(
                current_state_.pillars[PILLAR_RELATION], vn::fp20_t(delta));

            // Attraction also gets a small rotation
            float attr_theta = pillar_value_to_theta(current_state_.pillars[PILLAR_ATTRACTION]).to_float();
            float attr_delta = nearby[i].strength * 0.005f;
            current_state_.pillars[PILLAR_ATTRACTION] = apply_bloch_rotation(
                current_state_.pillars[PILLAR_ATTRACTION], vn::fp20_t(attr_delta));
        }
    }
}

void AgentCognition::reinforce_cords_with(uint32_t other_entity_id) {
    Cord* cord = cord_field_.find(agent_id_, other_entity_id);
    if (cord) {
        cord->reinforce();
    } else {
        cord_field_.create(agent_id_, other_entity_id);
    }
}

int AgentCognition::get_detectable_cord_count() const {
    Cord nearby[MAX_CORDS_PER_ENTITY];
    int count = cord_field_.get_for_entity(agent_id_, nearby, MAX_CORDS_PER_ENTITY);
    float awareness = current_state_.pillars[PILLAR_AWARENESS].to_float();
    int detectable = 0;
    for (int i = 0; i < count; i++) {
        if (nearby[i].is_detectable_by(awareness)) detectable++;
    }
    return detectable;
}

float AgentCognition::calculate_bloch_angular_shift() const {
    // Compute the angular shift of the Harm pillar using Bloch rotation:
    // Δθ = (incoming_force * operator_magnitude) / (willpower * depth)
    // where incoming_force is the change in Harm, and operator_magnitude
    // is the current Harm value itself.
    if (memory_.empty()) return 0.0f;
    
    const auto& prev = memory_.back();
    float incoming_force = std::abs(current_state_.pillars[PILLAR_HARM].to_float() - prev.pillars[PILLAR_HARM].to_float());
    float operator_mag = current_state_.pillars[PILLAR_HARM].to_float();
    float willpower = current_state_.pillars[PILLAR_WILLPOWER].to_float();
    float depth = current_state_.pillars[PILLAR_DEPTH].to_float();
    
    float resistance = willpower * depth;
    if (resistance < 1e-8f) resistance = 1e-8f;
    float raw_shift = (incoming_force * operator_mag) / resistance;
    if (raw_shift > 3.14159265f) raw_shift = 3.14159265f;
    if (raw_shift < -3.14159265f) raw_shift = -3.14159265f;
    return raw_shift;
}

void AgentCognition::apply_bloch_harm_rotation(float delta_theta, float& depth, float& constraint) {
    // Apply Harm rotation to Integrity (default target) with Depth buffer
    float raw_shift = delta_theta;
    float shift_abs = (raw_shift < 0.0f) ? -raw_shift : raw_shift;
    float applied = raw_shift;
    float willpower = current_state_.pillars[PILLAR_WILLPOWER].to_float();
    
    if (shift_abs > willpower) {
        applied = (raw_shift < 0.0f) ? -willpower : willpower;
        float excess = shift_abs - willpower;
        float depth_cost = excess * 0.5f;
        
        if (depth - depth_cost <= 0.0f) {
            float remaining = depth_cost - depth;
            depth = 0.0f;
            constraint += remaining;
            if (constraint > 1.0f) constraint = 1.0f;
        } else {
            depth -= depth_cost;
        }
    }
    
    // Apply Bloch rotation to Integrity
    float theta = std::acos(2.0f * current_state_.pillars[PILLAR_INTEGRITY].to_float() - 1.0f);
    float new_theta = theta + applied;
    if (new_theta < 0.0f) new_theta = 0.0f;
    if (new_theta > 3.14159265f) new_theta = 3.14159265f;
    current_state_.pillars[PILLAR_INTEGRITY] = vn::fp20_t((std::cos(new_theta) + 1.0f) * 0.5f);
}

void AgentCognition::apply_bloch_distortion_torsion() {
    // Apply Distortion torsion to Awareness: twist phi, don't dampen value
    float distortion = current_state_.pillars[PILLAR_DISTORTION].to_float();
    if (distortion <= 0.01f) return;
    
    float theta = std::acos(2.0f * current_state_.pillars[PILLAR_AWARENESS].to_float() - 1.0f);
    float phi_twist = distortion * 3.14159265f;
    float z_projection = std::cos(theta + phi_twist);
    current_state_.pillars[PILLAR_AWARENESS] = vn::fp20_t((z_projection + 1.0f) * 0.5f);
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
        float shadow_delta = (current_state_.pillars[i].to_float() - dream_state_.shadow_patterns[i].to_float()) * influence;
        dream_state_.shadow_patterns[i] = vn::fp20_t(dream_state_.shadow_patterns[i].to_float() + shadow_delta * 0.05f);
        dream_state_.shadow_patterns[i] = vn::fp20_t(std::clamp(dream_state_.shadow_patterns[i].to_float(), 0.0f, 1.0f));
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
        float integration = dream_state_.shadow_patterns[i].to_float() * (1.0f - dream_state_.lucid_level * 0.5f);
        float current_theta = pillar_value_to_theta(current_state_.pillars[i]).to_float();
        float integration_theta = pillar_value_to_theta(vn::fp20_t(integration)).to_float();
        float delta_theta = (integration_theta - current_theta) * 0.3f;
        float result_theta = current_theta + delta_theta;
        if (result_theta < 0.0f) result_theta = 0.0f;
        if (result_theta > 3.14159265f) result_theta = 3.14159265f;
        current_state_.pillars[i] = theta_to_pillar_value(vn::fp20_t(result_theta));
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
                    sum += memory_[j].pillars[i].to_float();
                }
                historical_avg = sum / 10.0f;
            }

            float drift_direction = (historical_avg - current_state_.pillars[i].to_float()) * drift;
            dream_state_.shadow_patterns[i] = vn::fp20_t(dream_state_.shadow_patterns[i].to_float() + drift_direction * 0.1f);
            dream_state_.shadow_patterns[i] = vn::fp20_t(std::clamp(dream_state_.shadow_patterns[i].to_float(), 0.0f, 1.0f));
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
        float theta = pillar_value_to_theta(current_state_.pillars[i]).to_float();
        float perturbed_theta = theta + perturbation(rng_);
        if (perturbed_theta < 0.0f) perturbed_theta = 0.0f;
        if (perturbed_theta > 3.14159265f) perturbed_theta = 3.14159265f;
        episode.simulated_pillars[i] = theta_to_pillar_value(vn::fp20_t(perturbed_theta));
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

std::string AgentCognition::export_state_json() const {
    std::ostringstream json;
    json << "{";
    json << "\"agent_id\":" << agent_id_ << ",";
    json << "\"tick\":" << tick_count_ << ",";
    json << "\"distortion_level\":" << current_state_.distortion_level << ",";
    json << "\"pillars\":{"
         << "\"awareness\":" << current_state_.pillars[PILLAR_AWARENESS].to_float() << ","
         << "\"willpower\":" << current_state_.pillars[PILLAR_WILLPOWER].to_float() << ","
         << "\"force\":" << current_state_.pillars[PILLAR_FORCE].to_float() << ","
         << "\"influence\":" << current_state_.pillars[PILLAR_INFLUENCE].to_float() << ","
         << "\"resistance\":" << current_state_.pillars[PILLAR_RESISTANCE].to_float() << ","
         << "\"integrity\":" << current_state_.pillars[PILLAR_INTEGRITY].to_float() << ","
         << "\"cohesion\":" << current_state_.pillars[PILLAR_COHESION].to_float() << ","
         << "\"relation\":" << current_state_.pillars[PILLAR_RELATION].to_float() << ","
         << "\"presence\":" << current_state_.pillars[PILLAR_PRESENCE].to_float() << ","
         << "\"warmth\":" << current_state_.pillars[PILLAR_WARMTH].to_float() << ","
         << "\"memory\":" << current_state_.pillars[PILLAR_MEMORY].to_float() << ","
         << "\"attraction\":" << current_state_.pillars[PILLAR_ATTRACTION].to_float() << ","
         << "\"harm\":" << current_state_.pillars[PILLAR_HARM].to_float() << ","
         << "\"distortion\":" << current_state_.pillars[PILLAR_DISTORTION].to_float() << ","
         << "\"flux\":" << current_state_.pillars[PILLAR_FLUX].to_float() << ","
         << "\"depth\":" << current_state_.pillars[PILLAR_DEPTH].to_float()
         << "},";
    json << "\"dreaming\":" << (dream_state_.is_dreaming ? "true" : "false") << ",";
    json << "\"dream_level\":" << static_cast<int>(dream_state_.current_level) << ",";
    json << "\"lucid_level\":" << dream_state_.lucid_level << ",";
    json << "\"cord_count\":" << cord_field_.get_active_count();
    json << "}";
    return json.str();
}
