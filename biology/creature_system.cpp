// creature_system.cpp - 16-pillar driven creature growth system implementation

#include "creature_system.h"
#include "../scale/SemanticProjection.h"
#include <algorithm>
#include <cstring>

namespace Van_Nueman {

CreaturePhysics::CreaturePhysics(const std::string& creature_type)
    : creature_type_(creature_type) {
    fusion_matrix_ = get_fusion_for_type(creature_type);
}

FusionMatrix CreaturePhysics::get_fusion_for_type(const std::string& type) {
    if (type == "wolf") {
        return {0.85f, 0.95f, 0.9f, 0.75f, 0.85f, 0.95f};
    } else if (type == "cat") {
        return {0.7f, 0.85f, 0.8f, 0.65f, 0.7f, 0.85f};
    } else if (type == "hybrid") {
        return {0.75f, 0.88f, 0.82f, 0.68f, 0.75f, 0.88f};
    }
    return FusionMatrix{};
}

GrowthStage CreaturePhysics::get_growth_stage(float age_ratio) const {
    return ::Van_Nueman::get_growth_stage_from_ratio(age_ratio);
}

float CreaturePhysics::get_scale(float age_ratio) const {
    return 1.0f - std::exp(-3.0f * age_ratio);
}

float CreaturePhysics::get_break_threshold(float age_ratio) const {
    return GrowthConfig::BASE_BREAK_THRESHOLD * (0.5f + age_ratio * 0.5f);
}

float CreaturePhysics::get_flexibility(float age_ratio) const {
    return 1.0f + (1.0f - age_ratio) * 0.5f;
}

bool CreaturePhysics::is_fused(const char* joint_name, float age_ratio) const {
    std::string name_lower = joint_name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
    
    auto check_region = [&](float fusion_value) -> bool {
        return age_ratio >= fusion_value;
    };
    
    if (name_lower.find("skull") != std::string::npos) {
        if (name_lower.find("suture") != std::string::npos)
            return check_region(fusion_matrix_.skull_sutures);
        return check_region(fusion_matrix_.skull);
    }
    if (name_lower.find("spine") != std::string::npos) {
        if (name_lower.find("disk") != std::string::npos)
            return check_region(fusion_matrix_.spine_disks);
        return check_region(fusion_matrix_.spine);
    }
    if (name_lower.find("pelvis") != std::string::npos)
        return check_region(fusion_matrix_.pelvis);
    if (name_lower.find("rib") != std::string::npos)
        return check_region(fusion_matrix_.ribs);
    
    return false;
}

void CreaturePhysics::get_constraints(float age_ratio, float& pitch_min, float& pitch_max,
                                       float& yaw_min, float& yaw_max, float& roll_min, float& roll_max) const {
    float base_pitch = 1.5f;
    float base_yaw = 1.5f;
    float base_roll = 0.5f;
    
    float mod_pitch = 1.0f, mod_yaw = 1.0f, mod_roll = 1.0f;
    if (creature_type_ == "wolf") {
        mod_pitch = 1.1f; mod_yaw = 1.2f;
    } else if (creature_type_ == "cat") {
        mod_pitch = 1.3f; mod_yaw = 1.4f; mod_roll = 1.2f;
    } else if (creature_type_ == "hybrid") {
        mod_pitch = 1.15f; mod_yaw = 1.1f; mod_roll = 0.9f;
    }
    
    float flexibility = get_flexibility(age_ratio);
    
    pitch_min = -base_pitch * mod_pitch * flexibility;
    pitch_max = base_pitch * mod_pitch * flexibility;
    yaw_min = -base_yaw * mod_yaw * flexibility;
    yaw_max = base_yaw * mod_yaw * flexibility;
    roll_min = -base_roll * mod_roll * flexibility;
    roll_max = base_roll * mod_roll * flexibility;
}

Bone::Bone(uint32_t id, const Vec3& start_pos, const Vec3& end_pos, float radius)
    : id_(id), start_pos_(start_pos), end_pos_(end_pos), radius_(radius) {
}

void Bone::apply_force(const Vec3& force, float dt) {
    if (fractured_) return;
    
    float force_mag = force.norm();
    if (force_mag > break_threshold_) {
        fracture();
        return;
    }
    
    current_strain_ = force_mag / break_threshold_;
}

void Bone::apply_growth(float growth_factor) {
    Vec3 diff = end_pos_ - start_pos_;
    float len = diff.norm();
    if (len > 0.001f) {
        Vec3 dir = diff * (1.0f / len);
        float new_len = len * (1.0f + growth_factor);
        end_pos_ = start_pos_ + dir * new_len;
    }
}

Skeleton::~Skeleton() {
}

SkeletonNode* Skeleton::add_node(SkeletonNode* parent,
                                               const Vec3& local_pos, const char* name) {
    auto* node = new SkeletonNode(
        static_cast<uint32_t>(nodes.size()), local_pos, parent, name);
    nodes.push_back(std::unique_ptr<SkeletonNode>(node));
    if (!root && !parent) root = node;
    return node;
}

Bone* Skeleton::add_bone(SkeletonNode* start, SkeletonNode* end,
                         float radius) {
    auto* bone = new Bone(static_cast<uint32_t>(bones.size()), 
                          start->local_pos, end->local_pos, radius);
    bones.push_back(std::unique_ptr<Bone>(bone));
    return bone;
}

Muscle* Skeleton::add_muscle(const char* name, SkeletonNode* origin,
                              SkeletonNode* insertion, uint32_t strand_count) {
    auto* muscle = new Muscle(static_cast<uint32_t>(muscles.size()), name, origin, insertion, strand_count);
    muscles.push_back(std::unique_ptr<Muscle>(muscle));
    return muscle;
}

Organ* Skeleton::add_organ(const char* name, OrganType type,
                            Bone* segment, float volume) {
    auto* organ = new Organ(static_cast<uint32_t>(organs.size()), name, type, nullptr, volume);
    organs.push_back(std::unique_ptr<Organ>(organ));
    return organ;
}

void Skeleton::update(float dt) {
    for (auto& muscle : muscles) {
        muscle->step(dt);
    }
    for (auto& organ : organs) {
        organ->operate_organ(dt, nullptr);
    }
}

void Skeleton::reset_positions() {
}

Creature::Creature(const std::string& name, const std::string& type, float age)
    : name_(name), type_(type), age_(age) {
    
    entity_id_ = compute_entity_id(pillar_state_);
    
    physics_ = std::make_unique<CreaturePhysics>(type);
    skeleton_ = std::make_unique<Skeleton>();
    
    scale_ = physics_->get_scale(age);
    pillar_state_.fill(vn::fp20_t(0.5f));
    
    build_systems();
}

Creature::~Creature() = default;

void Creature::build_systems() {
    auto* root = skeleton_->add_node(nullptr, {0, 0, 0}, "root");
    
    std::array<SkeletonNode*, 5> spine_nodes{};
    for (uint32_t i = 0; i < 5; i++) {
        spine_nodes[i] = skeleton_->add_node(
            i == 0 ? root : spine_nodes[i-1],
            {0, static_cast<float>((i + 1) * 4), 0},
            ("spine_" + std::to_string(i)).c_str());
    }
    
    auto* head = skeleton_->add_node(spine_nodes[4], {0, 5, 0}, "skull");
    auto* jaw = skeleton_->add_node(head, {0, -1, 0}, "jaw");
    
    for (uint32_t i = 0; i < 4; i++) {
        float x_offset = (i < 2) ? -3.0f : 3.0f;
        auto* leg_start = skeleton_->add_node(
            spine_nodes[i + 1],
            {x_offset, -2, 0},
            ("leg_" + std::to_string(i)).c_str());
        skeleton_->add_node(leg_start, {0, -5, 0}, ("foot_" + std::to_string(i)).c_str());
    }
    
    skeleton_->add_organ("heart", ORGAN_PUMP, nullptr, 10.0f);
    skeleton_->add_organ("brain", ORGAN_POWER_PLANT, nullptr, 8.0f);
    skeleton_->add_organ("liver", ORGAN_FACTORY, nullptr, 6.0f);
    
    for (size_t i = 0; i < skeleton_->nodes.size() - 1; i++) {
        if (i < skeleton_->bones.size()) continue;
        auto* node_a = skeleton_->nodes[i].get();
        auto* node_b = skeleton_->nodes[i + 1].get();
        if (node_a && node_b) {
            skeleton_->add_bone(node_a, node_b);
        }
    }
    
    link_systems();
}

void Creature::link_systems() {
}

void Creature::step(float dt) {
    if (!alive_) return;
    
    if (skeleton_) {
        skeleton_->update(dt);
    }
    
    update_pillars(dt);
    
    if (stress_ > 0.8f) {
        body_temperature_ += stress_ * Creature::TEMP_INCREASE_RATE * dt;
    }
    
    check_death_conditions();
}

void Creature::perform_action(const std::string& action, float intensity) {
    if (!alive_) return;
    
    state_ = action;
    
    static const std::pair<std::string, std::array<float, 3>> action_map[] = {
        {"idle", {0.1f, 0.1f, 0.1f}},
        {"rest", {0.2f, 0.2f, 0.1f}},
        {"sleep", {0.1f, 0.15f, 0.1f}},
        {"walk", {1.0f, 1.0f, 0.5f}},
        {"run", {2.5f, 2.5f, 0.8f}},
        {"attack", {3.0f, 3.0f, 0.9f}},
        {"hunt", {3.0f, 3.0f, 0.6f}},
        {"eat", {0.3f, 0.5f, 0.1f}},
        {"fight", {3.5f, 3.5f, 0.95f}},
        {"flee", {3.0f, 3.0f, 0.9f}},
    };
    
    float muscle_int = 0.1f;
    float metabolic = 0.1f;
    float nervous = 0.1f;
    
    for (const auto& [name, params] : action_map) {
        if (action.find(name) != std::string::npos) {
            muscle_int = params[0];
            metabolic = params[1];
            nervous = params[2];
            break;
        }
    }
    
    muscle_activation_ = muscle_int * intensity;
    alertness_ = nervous * intensity;
    
    if (action == "eat" && overall_health_ < 1.0f) {
        overall_health_ = std::fmin(1.0f, overall_health_ + 0.1f);
    }
}

void Creature::receive_stimulus(const std::string& stimulus, float intensity) {
    if (!alive_) return;
    
    if (stimulus == "pain" && intensity > 0.5f) {
        state_ = "in_pain";
        stress_ = std::fmin(1.0f, stress_ + intensity * 0.3f);
        overall_health_ = std::fmax(0.0f, overall_health_ - intensity * 0.1f);
        
        for (auto& organ : skeleton_->organs) {
            if (std::string(organ->name).find("heart") != std::string::npos) {
                organ->set_health(organ->get_health() - intensity * 0.1f);
            }
        }
    }
    
    if (stimulus == "smell" || stimulus == "sight") {
        alertness_ = std::fmin(1.0f, alertness_ + intensity * 0.5f);
    }
}

void Creature::apply_pillar_influence(const PillarStateVector& pillars, float influence_strength) {
    for (int i = 0; i < NumPillars; i++) {
        pillar_state_.pillars[i] += (pillars.pillars[i] - 0.5f) * influence_strength * 0.1f;
        pillar_state_.pillars[i] = std::fmax(0.0f, std::fmin(1.0f, pillar_state_.pillars[i]));
    }
}

void Creature::update_pillars(float dt) {
    BiologicalProjection bp = BiologicalProjection::project(pillar_state_);
    
    skin_tension_ = bp.stress_level * muscle_activation_ * 0.5f;
    
    float health_change = (bp.willpower_stamina * 0.1f - bp.stress_level * 0.05f) * dt;
    overall_health_ = std::fmax(0.0f, std::fmin(1.0f, overall_health_ + health_change));
}

void Creature::check_death_conditions() {
    if (body_temperature_ > GrowthConfig::MAX_TEMP || 
        body_temperature_ < GrowthConfig::MIN_TEMP) {
        alive_ = false;
        state_ = "dead";
    }
    
    if (overall_health_ <= 0.0f) {
        alive_ = false;
        state_ = "dead";
    }
    
    for (auto& organ : skeleton_->organs) {
        if (organ->get_health() <= 0.0f) {
            std::string name(organ->name);
            if (name.find("heart") != std::string::npos || 
                name.find("brain") != std::string::npos) {
                alive_ = false;
                state_ = "dead";
                break;
            }
        }
    }
}

CreatureState Creature::get_state() const {
    CreatureState state;
    state.age = age_;
    state.scale = scale_;
    state.body_temperature = body_temperature_;
    state.overall_health = overall_health_;
    state.muscle_activation = muscle_activation_;
    state.skin_tension = skin_tension_;
    state.alertness = alertness_;
    state.stress = stress_;
    state.pos_x = position_.x;
    state.pos_y = position_.y;
    state.pos_z = position_.z;
    state.stage = physics_ ? physics_->get_growth_stage(age_) : GrowthStage::Adult;
    state.alive = alive_;
    CognitiveProjection cp = CognitiveProjection::project(pillar_state_);
    state.is_dreaming = cp.imaginativeness > 0.7f;
    state.in_shadow = cp.cognitive_load > 0.5f;
    state.pillars = pillar_state_;
    return state;
}

const std::array<Creature::ActionConfig, Creature::MAX_ACTIONS>& Creature::get_action_configs() {
    static const std::array<ActionConfig, MAX_ACTIONS> configs = {{
        {0.1f, 0.1f, 0.1f},
        {0.2f, 0.2f, 0.1f},
        {0.1f, 0.15f, 0.1f},
        {1.0f, 1.0f, 0.5f},
        {2.5f, 2.5f, 0.8f},
        {3.0f, 3.0f, 0.9f},
        {3.0f, 3.0f, 0.6f},
        {0.3f, 0.5f, 0.1f},
        {3.5f, 3.5f, 0.95f},
        {3.0f, 3.0f, 0.9f},
    }};
    return configs;
}

GrowthSystem::GrowthSystem() = default;

void GrowthSystem::register_creature(Creature* creature) {
    if (!creature) return;
    uint32_t id = creature->get_entity_id();
    auto it = std::find_if(creatures_.begin(), creatures_.end(),
        [id](const std::unique_ptr<Creature>& c) { return c->get_entity_id() == id; });
    if (it == creatures_.end()) {
        creatures_.push_back(std::unique_ptr<Creature>(creature));
    }
}

void GrowthSystem::unregister_creature(uint32_t entity_id) {
    creatures_.erase(
        std::remove_if(creatures_.begin(), creatures_.end(),
            [entity_id](const std::unique_ptr<Creature>& c) {
                return c->get_entity_id() == entity_id;
            }),
        creatures_.end()
    );
}

Creature* GrowthSystem::get_creature(uint32_t entity_id) {
    auto it = std::find_if(creatures_.begin(), creatures_.end(),
        [entity_id](const std::unique_ptr<Creature>& c) {
            return c->get_entity_id() == entity_id;
        });
    return (it != creatures_.end()) ? it->get() : nullptr;
}

void GrowthSystem::update(float dt) {
    for (auto& creature : creatures_) {
        if (creature->is_alive()) {
            creature->step(dt);
        }
    }
}

void GrowthSystem::grow_creature_to_age(uint32_t entity_id, float target_age) {
    if (auto* creature = get_creature(entity_id)) {
        float old_scale = creature->get_scale();
        
        auto it = std::find_if(creatures_.begin(), creatures_.end(),
            [entity_id](const std::unique_ptr<Creature>& c) {
                return c->get_entity_id() == entity_id;
            });
        if (it != creatures_.end()) {
            GrowthStage old_stage = it->get()->get_state().stage;
            float growth_factor = target_age / std::fmax(0.01f, it->get()->get_age());
            
            if (auto* skel = it->get()->get_skeleton()) {
                for (auto& bone : skel->bones) {
                    bone->apply_growth(growth_factor - 1.0f);
                }
            }
        }
    }
}

std::vector<uint32_t> GrowthSystem::get_alive_creatures() const {
    std::vector<uint32_t> result;
    for (const auto& creature : creatures_) {
        if (creature->is_alive()) {
            result.push_back(creature->get_entity_id());
        }
    }
    return result;
}

std::vector<uint32_t> GrowthSystem::get_creatures_by_type(const std::string& type) const {
    std::vector<uint32_t> result;
    for (const auto& creature : creatures_) {
        if (creature->get_type() == type) {
            result.push_back(creature->get_entity_id());
        }
    }
    return result;
}

std::vector<uint32_t> GrowthSystem::get_creatures_in_stage(GrowthStage stage) const {
    std::vector<uint32_t> result;
    for (const auto& creature : creatures_) {
        if (creature->get_state().stage == stage) {
            result.push_back(creature->get_entity_id());
        }
    }
    return result;
}

Creature* create_creature(const std::string& name, const std::string& type, float age) {
    return new Creature(name, type, age);
}

}