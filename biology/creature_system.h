// creature_system.h - 16-pillar driven creature growth system
// Adapted from Sov_Eng/renderer/creature_growth.py and creature_builder.py

#pragma once

#include <cstdint>
#include <cmath>
#include <array>
#include <vector>
#include <string>
#include <memory>
#include "../include/Entity.h"
#include "../include/SkellyTypes.h"

namespace Van_Nueman {

enum class GrowthStage : uint32_t {
    Embryo = 0,
    Neonate = 1,
    Infant = 2,
    Juvenile = 3,
    Subadult = 4,
    Adult = 5,
    Elder = 6
};

struct GrowthConfig {
    static constexpr float DEFAULT_SCALE = 1.0f;
    static constexpr float EMBRYO_RATIO = 0.1f;
    static constexpr float NEONATE_RATIO = 0.25f;
    static constexpr float INFANT_RATIO = 0.4f;
    static constexpr float JUVENILE_RATIO = 0.6f;
    static constexpr float SUBADULT_RATIO = 0.8f;
    static constexpr float ADULT_RATIO = 1.0f;
    
    static constexpr float MIN_TEMP = 35.0f;
    static constexpr float MAX_TEMP = 42.0f;
    static constexpr float BASE_BREAK_THRESHOLD = 100.0f;
    static constexpr float BASE_FLEXIBILITY = 0.1f;
};

struct FusionMatrix {
    float skull = 0.8f;
    float spine = 0.9f;
    float pelvis = 0.85f;
    float ribs = 0.7f;
    float skull_sutures = 0.8f;
    float spine_disks = 0.9f;
};

class CreaturePhysics {
public:
    explicit CreaturePhysics(const std::string& creature_type);
    
    GrowthStage get_growth_stage(float age_ratio) const;
    float get_scale(float age_ratio) const;
    float get_break_threshold(float age_ratio) const;
    float get_flexibility(float age_ratio) const;
    bool is_fused(const char* joint_name, float age_ratio) const;
    
    void get_constraints(float age_ratio, float& pitch_min, float& pitch_max,
                         float& yaw_min, float& yaw_max, float& roll_min, float& roll_max) const;
    
    const std::string& get_creature_type() const { return creature_type_; }
    const FusionMatrix& get_fusion_matrix() const { return fusion_matrix_; }

private:
    std::string creature_type_;
    FusionMatrix fusion_matrix_;
    
    static FusionMatrix get_fusion_for_type(const std::string& type);
};

struct CreatureState {
    float age;
    float scale;
    float body_temperature;
    float overall_health;
    float muscle_activation;
    float skin_tension;
    float alertness;
    float stress;
    float pos_x;
    float pos_y;
    float pos_z;
    GrowthStage stage;
    bool alive;
    bool is_dreaming;
    bool in_shadow;
    PillarStateVector pillars;
};

class Organ : public ::Organ {
public:
    using ::Organ::Organ;
    
    float get_health() const { return health_; }
    void set_health(float h) { health_ = h; }
    float get_volume() const { return volume; }
    
    void operate_organ(float dt, TransportSystem* transport = nullptr) {
        operate(transport);
    }

private:
    float health_ = 1.0f;
};

class Muscle : public MuscleGroup {
public:
    using MuscleGroup::MuscleGroup;
    
    void apply_activation(float activation_level) {
        activation = std::fmax(0.0f, std::fmin(1.0f, activation_level));
        update_volume();
    }
    
    float get_activation() const { return activation; }
    
    void step(float dt) {
        update_volume();
    }
};

class Bone {
public:
    Bone(uint32_t id, const Vec3& start_pos, const Vec3& end_pos, float radius = 2.0f);
    
    uint32_t get_id() const { return id_; }
    const Vec3& get_start() const { return start_pos_; }
    const Vec3& get_end() const { return end_pos_; }
    void set_end(const Vec3& pos) { end_pos_ = pos; }
    
    float get_radius() const { return radius_; }
    void set_radius(float r) { radius_ = r; }
    float get_flexibility() const { return flexibility_; }
    void set_flexibility(float f) { flexibility_ = f; }
    float get_break_threshold() const { return break_threshold_; }
    void set_break_threshold(float t) { break_threshold_ = t; }
    
    bool is_fractured() const { return fractured_; }
    void fracture() { fractured_ = true; }
    
    float get_length() const {
        Vec3 diff = end_pos_ - start_pos_;
        return diff.norm();
    }
    
    void apply_force(const Vec3& force, float dt);
    void apply_growth(float growth_factor);

private:
    uint32_t id_;
    Vec3 start_pos_;
    Vec3 end_pos_;
    float radius_;
    float flexibility_ = 0.1f;
    float break_threshold_ = 100.0f;
    bool fractured_ = false;
    float current_strain_ = 0.0f;
};

struct Skeleton {
    std::vector<std::unique_ptr<SkeletonNode>> nodes;
    std::vector<std::unique_ptr<Bone>> bones;
    std::vector<std::unique_ptr<Muscle>> muscles;
    std::vector<std::unique_ptr<Organ>> organs;
    std::vector<std::unique_ptr<TransportSystem>> transports;
    
    SkeletonNode* root = nullptr;
    
    ~Skeleton();
    
    SkeletonNode* add_node(SkeletonNode* parent, 
                                        const Vec3& local_pos, const char* name);
    Bone* add_bone(SkeletonNode* start, SkeletonNode* end,
                   float radius = 2.0f);
    Muscle* add_muscle(const char* name, SkeletonNode* origin,
                       SkeletonNode* insertion, uint32_t strand_count = 8);
    Organ* add_organ(const char* name, OrganType type,
                     Bone* segment, float volume = 5.0f);
    
    void update(float dt);
    void reset_positions();
};

class Creature {
public:
    Creature(const std::string& name, const std::string& type, float age = 1.0f);
    ~Creature();
    
    void step(float dt);
    void perform_action(const std::string& action, float intensity = 1.0f);
    void receive_stimulus(const std::string& stimulus, float intensity);
    void apply_pillar_influence(const PillarStateVector& pillars, float influence_strength);
    
    const std::string& get_name() const { return name_; }
    const std::string& get_type() const { return type_; }
    float get_age() const { return age_; }
    float get_scale() const { return scale_; }
    bool is_alive() const { return alive_; }
    const Vec3& get_position() const { return position_; }
    void set_position(const Vec3& pos) { position_ = pos; }
    void set_position(float x, float y, float z) { position_ = Vec3(x, y, z); }
    
    Skeleton* get_skeleton() { return skeleton_.get(); }
    const Skeleton* get_skeleton() const { return skeleton_.get(); }
    
    CreatureState get_state() const;
    
    uint32_t get_entity_id() const { return entity_id_; }
    void set_entity_id(uint32_t id) { entity_id_ = id; }
    
    void get_pillar_state(PillarStateVector& out) const { out = pillar_state_; }
    void set_pillar_state(const PillarStateVector& psv) { pillar_state_ = psv; }
    
    static constexpr uint32_t MAX_ACTIONS = 16;
    struct ActionConfig {
        float muscle_intensity;
        float metabolic_rate;
        float nervous_state;
    };
    
    static const std::array<ActionConfig, MAX_ACTIONS>& get_action_configs();

private:
    void build_systems();
    void link_systems();
    void check_death_conditions();
    void update_pillars(float dt);
    
    std::string name_;
    std::string type_;
    float age_ = 1.0f;
    float scale_ = 1.0f;
    bool alive_ = true;
    bool active_ = true;
    std::string state_ = "idle";
    
    uint32_t entity_id_ = 0;
    Vec3 position_;
    
    std::unique_ptr<CreaturePhysics> physics_;
    std::unique_ptr<Skeleton> skeleton_;
    
    PillarStateVector pillar_state_;
    
    float body_temperature_ = 37.0f;
    float overall_health_ = 1.0f;
    float muscle_activation_ = 0.0f;
    float skin_tension_ = 0.0f;
    float alertness_ = 0.5f;
    float stress_ = 0.1f;
    
    static constexpr float TEMP_INCREASE_RATE = 0.1f;
};

class GrowthSystem {
public:
    GrowthSystem();
    
    void register_creature(Creature* creature);
    void unregister_creature(uint32_t entity_id);
    Creature* get_creature(uint32_t entity_id);
    
    void update(float dt);
    void grow_creature_to_age(uint32_t entity_id, float target_age);
    
    size_t get_creature_count() const { return creatures_.size(); }
    
    std::vector<uint32_t> get_alive_creatures() const;
    std::vector<uint32_t> get_creatures_by_type(const std::string& type) const;
    std::vector<uint32_t> get_creatures_in_stage(GrowthStage stage) const;

private:
    std::vector<std::unique_ptr<Creature>> creatures_;
};

inline GrowthStage get_growth_stage_from_ratio(float ratio) {
    if (ratio < GrowthConfig::EMBRYO_RATIO) return GrowthStage::Embryo;
    if (ratio < GrowthConfig::NEONATE_RATIO) return GrowthStage::Neonate;
    if (ratio < GrowthConfig::INFANT_RATIO) return GrowthStage::Infant;
    if (ratio < GrowthConfig::JUVENILE_RATIO) return GrowthStage::Juvenile;
    if (ratio < GrowthConfig::SUBADULT_RATIO) return GrowthStage::Subadult;
    if (ratio < GrowthConfig::ADULT_RATIO) return GrowthStage::Adult;
    return GrowthStage::Elder;
}

Creature* create_creature(const std::string& name, const std::string& type, float age = 1.0f);

}