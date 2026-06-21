// alife_growth.h - ALife organism spawning from cellular automata grid
// Connects cellular_automata.h to creature_system.h

#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <array>
#include "../simulation/cellular_automata.h"
#include "../biology/creature_system.h"
#include "../include/Entity.h"
#include "../include/Constraint.h"

namespace Van_Nueman {

struct OrganismSpawnConfig {
    float min_energy_threshold;
    float min_matter_threshold;
    float min_health_threshold;
    float spawn_radius;
    uint32_t max_organisms_per_cell;
    bool respect_pillar_constraints;
};

struct SpawnResult {
    uint32_t entity_id;
    bool success;
    const char* failure_reason;
};

class ALifeGrowthManager {
public:
    ALifeGrowthManager();
    ~ALifeGrowthManager();
    
    bool initialize();
    void shutdown();
    
    void link_cellular_automata(CAGridSoA* ca_grid, PillarGridSoA* pillar_grid);
    void link_growth_system(GrowthSystem* growth_system);
    
    void update(float dt);
    
    void spawn_organisms_from_grid();
    SpawnResult try_spawn_at(size_t grid_x, size_t y, size_t z);
    
    void update_organism_from_cell(uint32_t entity_id, size_t cell_index);
    
    // ── Constraint Integration ────────────────────────────────
    float get_organism_constraint(uint32_t entity_id) const;
    void set_organism_constraint(uint32_t entity_id, float level);
    void apply_constraint_recovery(float dt);
    float calculate_cell_constraint(size_t cell_index) const;
    
    void set_config(const OrganismSpawnConfig& config);
    const OrganismSpawnConfig& get_config() const { return config_; }
    
    size_t get_active_organism_count() const { return active_organisms_.size(); }
    std::vector<uint32_t> get_organism_ids() const;
    
    bool is_valid_spawn_location(size_t x, size_t y, size_t z) const;
    PillarStateVector extract_pillar_state(size_t cell_index) const;

private:
    CAGridSoA* ca_grid_ = nullptr;
    PillarGridSoA* pillar_grid_ = nullptr;
    GrowthSystem* growth_system_ = nullptr;
    
    OrganismSpawnConfig config_;
    
    std::vector<uint32_t> active_organisms_;
    ConstraintState world_constraint_;  // global constraint tracker
    
    static constexpr float DEFAULT_MIN_ENERGY = 10.0f;
    static constexpr float DEFAULT_MIN_MATTER = 5.0f;
    static constexpr float DEFAULT_MIN_HEALTH = 0.3f;
    static constexpr float DEFAULT_SPAWN_RADIUS = 5.0f;
    static constexpr uint32_t DEFAULT_MAX_ORGANISMS = 8;
    
    // Constraint thresholds
    static constexpr float CONSTRAINT_LOW = 0.3f;
    static constexpr float CONSTRAINT_MED = 0.6f;
    static constexpr float CONSTRAINT_HIGH = 0.85f;
    
    bool can_spawn(size_t x, size_t y, size_t z) const;
    uint32_t determine_creature_type(size_t cell_index) const;
    std::string get_creature_type_name(uint32_t type_id) const;
    float calculate_spawn_priority(size_t cell_index) const;
};

struct OrganismGridCoupling {
    uint32_t organism_entity_id;
    size_t coupled_grid_x;
    size_t coupled_grid_y;
    size_t coupled_grid_z;
    float influence_radius;
    float energy_drain_rate;
    float matter_contribution;
};

class ALifePillarCoupler {
public:
    ALifePillarCoupler();
    
    void link_to_grid(PillarGridSoA* grid);
    void link_to_creatures(GrowthSystem* creatures);
    
    void sync_pillars_to_organisms(float dt);
    void sync_organisms_to_pillars(float dt);
    
    void apply_pillar_influence_to_grid(const PillarStateVector& pillar_state,
                                         size_t center_x, size_t center_y, size_t center_z,
                                         float radius);
    
    float calculate_pillar_pressure_at(size_t x, size_t y, size_t z) const;
    
    enum PillarInfluenceDirection : uint32_t {
        GRID_TO_ORGANISM = 0,
        ORGANISM_TO_GRID = 1,
        BIDIRECTIONAL = 2
    };
    
    void set_influence_direction(PillarInfluenceDirection dir);
    PillarInfluenceDirection get_influence_direction() const { return influence_direction_; }

private:
    PillarGridSoA* pillar_grid_ = nullptr;
    GrowthSystem* creatures_ = nullptr;
    PillarInfluenceDirection influence_direction_ = BIDIRECTIONAL;
    
    static constexpr float DEFAULT_SYNC_STRENGTH = 0.1f;
    static constexpr float DEFAULT_PROPAGATION_RADIUS = 3.0f;
    
    PillarStateVector sample_pillars_at(size_t x, size_t y, size_t z) const;
    void write_pillars_at(size_t x, size_t y, size_t z, const PillarStateVector& pillars);
};

class ALifeSpawnController {
public:
    ALifeSpawnController();
    
    void set_manager(ALifeGrowthManager* manager);
    void set_max_spawns_per_frame(uint32_t max_spawns);
    void set_spawn_interval(float interval_seconds);
    
    void trigger_burst_spawn(size_t center_x, size_t center_y, size_t center_z, uint32_t count);
    void pause_spawning();
    void resume_spawning();
    bool is_spawning_paused() const { return spawning_paused_; }
    
    void update(float dt);
    
    struct SpawnStatistics {
        uint32_t total_spawns;
        uint32_t failed_spawns;
        uint32_t active_organisms;
        float average_spawn_priority;
    };
    
    SpawnStatistics get_statistics() const;
    void reset_statistics();

private:
    ALifeGrowthManager* manager_ = nullptr;
    uint32_t max_spawns_per_frame_ = 4;
    float spawn_interval_ = 0.1f;
    float time_since_last_spawn_ = 0.0f;
    bool spawning_paused_ = false;
    
    SpawnStatistics stats_ = {};
    
    std::vector<std::tuple<size_t, size_t, size_t>> pending_spawns_;
};

class DreamStateCreatureEffects {
public:
    DreamStateCreatureEffects();
    
    void set_dream_intensity(float intensity);
    float get_dream_intensity() const { return dream_intensity_; }
    
    void apply_dream_effects_to_creature(Creature* creature, float dt);
    void apply_dream_effects_to_skeleton(Skeleton* skeleton, float dt);
    
    enum DreamEffectType : uint32_t {
        EFFECT_GLOW = 0,
        EFFECT_DISTORTION = 1,
        EFFECT_FLOAT = 2,
        EFFECT_COLOR_SHIFT = 3,
        EFFECT_SIZE_PULSE = 4
    };
    
    void enable_effect(DreamEffectType effect);
    void disable_effect(DreamEffectType effect);
    bool is_effect_enabled(DreamEffectType effect) const;

private:
    float dream_intensity_ = 0.0f;
    uint32_t enabled_effects_ = 0;
    
    static constexpr float GLOW_COLOR_R = 0.6f;
    static constexpr float GLOW_COLOR_G = 0.3f;
    static constexpr float GLOW_COLOR_B = 0.9f;
    
    void apply_glow_effect(Creature* creature);
    void apply_distortion_effect(Skeleton* skeleton);
    void apply_float_effect(Skeleton* skeleton, float dt);
    void apply_color_shift_effect(Creature* creature);
    void apply_size_pulse_effect(Creature* creature, float dt);
};

class ShadowStateCreatureEffects {
public:
    ShadowStateCreatureEffects();
    
    void set_shadow_intensity(float intensity);
    float get_shadow_intensity() const { return shadow_intensity_; }
    
    void apply_shadow_effects_to_creature(Creature* creature, float dt);
    void apply_shadow_effects_to_skeleton(Skeleton* skeleton, float dt);
    
    enum ShadowEffectType : uint32_t {
        EFFECT_DARKEN = 0,
        EFFECT_FRACTURE = 1,
        EFFECT_WEAKEN = 2,
        EFFECT_STIFFEN = 3,
        EFFECT_DECAY = 4
    };
    
    void enable_effect(ShadowEffectType effect);
    void disable_effect(ShadowEffectType effect);
    bool is_effect_enabled(ShadowEffectType effect) const;

private:
    float shadow_intensity_ = 0.0f;
    uint32_t enabled_effects_ = 0;
    
    static constexpr float SHADOW_COLOR_R = 0.15f;
    static constexpr float SHADOW_COLOR_G = 0.15f;
    static constexpr float SHADOW_COLOR_B = 0.25f;
    
    void apply_darken_effect(Creature* creature);
    void apply_fracture_effect(Skeleton* skeleton);
    void apply_weaken_effect(Skeleton* skeleton);
    void apply_stiffen_effect(Skeleton* skeleton);
    void apply_decay_effect(Creature* creature, float dt);
};

ALifeGrowthManager* create_alife_growth_manager();
void destroy_alife_growth_manager(ALifeGrowthManager* manager);

}