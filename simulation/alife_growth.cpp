// alife_growth.cpp - ALife organism spawning implementation

#include "alife_growth.h"
#include <algorithm>
#include <cmath>

namespace Van_Nueman {

ALifeGrowthManager::ALifeGrowthManager() {
    config_.min_energy_threshold = DEFAULT_MIN_ENERGY;
    config_.min_matter_threshold = DEFAULT_MIN_MATTER;
    config_.min_health_threshold = DEFAULT_MIN_HEALTH;
    config_.spawn_radius = DEFAULT_SPAWN_RADIUS;
    config_.max_organisms_per_cell = DEFAULT_MAX_ORGANISMS;
    config_.respect_pillar_constraints = true;
}

ALifeGrowthManager::~ALifeGrowthManager() {
    shutdown();
}

bool ALifeGrowthManager::initialize() {
    return true;
}

void ALifeGrowthManager::shutdown() {
    active_organisms_.clear();
}

void ALifeGrowthManager::link_cellular_automata(CAGridSoA* ca_grid, PillarGridSoA* pillar_grid) {
    ca_grid_ = ca_grid;
    pillar_grid_ = pillar_grid;
}

void ALifeGrowthManager::link_growth_system(GrowthSystem* growth_system) {
    growth_system_ = growth_system;
}

void ALifeGrowthManager::update(float dt) {
    spawn_organisms_from_grid();
    
    for (uint32_t organism_id : active_organisms_) {
        if (auto* creature = growth_system_->get_creature(organism_id)) {
            if (creature->is_alive()) {
                creature->step(dt);
            }
        }
    }
    
    active_organisms_.erase(
        std::remove_if(active_organisms_.begin(), active_organisms_.end(),
            [this](uint32_t id) {
                auto* creature = growth_system_ ? growth_system_->get_creature(id) : nullptr;
                return creature == nullptr || !creature->is_alive();
            }),
        active_organisms_.end()
    );
}

void ALifeGrowthManager::spawn_organisms_from_grid() {
    if (!ca_grid_ || !growth_system_) return;
    
    for (size_t z = 0; z < ca_grid_->depth; z++) {
        for (size_t y = 0; y < ca_grid_->height; y++) {
            for (size_t x = 0; x < ca_grid_->width; x++) {
                size_t idx = get_cell_index(*ca_grid_, x, y, z);
                
                if (can_spawn(x, y, z)) {
                    float priority = calculate_spawn_priority(idx);
                    if (priority > 0.5f) {
                        try_spawn_at(x, y, z);
                    }
                }
            }
        }
    }
}

SpawnResult ALifeGrowthManager::try_spawn_at(size_t grid_x, size_t y, size_t z) {
    SpawnResult result = {};
    result.success = false;
    
    if (!ca_grid_ || !growth_system_) {
        result.failure_reason = "Systems not linked";
        return result;
    }
    
    if (!is_valid_spawn_location(grid_x, y, z)) {
        result.failure_reason = "Invalid spawn location";
        return result;
    }
    
    size_t idx = get_cell_index(*ca_grid_, grid_x, y, z);
    
    if (ca_grid_->energy[idx] < config_.min_energy_threshold) {
        result.failure_reason = "Insufficient energy";
        return result;
    }
    
    if (ca_grid_->matter[idx] < config_.min_matter_threshold) {
        result.failure_reason = "Insufficient matter";
        return result;
    }
    
    uint32_t creature_type = determine_creature_type(idx);
    std::string creature_name = "Organism_" + std::to_string(idx);
    
    float age = 0.1f + ca_grid_->matter[idx] / 100.0f;
    auto* creature = create_creature(creature_name, get_creature_type_name(creature_type), age);
    
    if (!creature) {
        result.failure_reason = "Creature creation failed";
        return result;
    }
    
    PillarStateVector pillars = extract_pillar_state(idx);
    creature->apply_pillar_influence(pillars, 0.5f);
    
    PillarStateVector psv;
    psv.fill(0.5f);
    if (pillar_grid_) {
        for (int i = 0; i < 8 && i < NUM_PILLARS; i++) {
            float* pillar_ptrs[] = {
                pillar_grid_->alignment, pillar_grid_->resonance,
                pillar_grid_->stability, pillar_grid_->entropy,
                pillar_grid_->coherence, pillar_grid_->flux,
                pillar_grid_->manifestation, pillar_grid_->dissolution
            };
            if (pillar_ptrs[i]) {
                psv.pillars[i] = pillar_ptrs[i][idx];
            }
        }
    }
    creature->apply_pillar_influence(psv, 0.3f);
    
    growth_system_->register_creature(creature);
    
    result.entity_id = creature->get_entity_id();
    result.success = true;
    result.failure_reason = nullptr;
    
    active_organisms_.push_back(result.entity_id);
    
    ca_grid_->entity_id[idx] = result.entity_id;
    ca_grid_->flags[idx].alive = 1;
    
    return result;
}

void ALifeGrowthManager::update_organism_from_cell(uint32_t entity_id, size_t cell_index) {
    if (!ca_grid_ || !growth_system_) return;
    
    auto* creature = growth_system_->get_creature(entity_id);
    if (!creature) return;
    
    if (ca_grid_->flags[cell_index].in_dream) {
        float lucid = ca_grid_->dream_state[cell_index] / 255.0f;
        PillarStateVector dream_pillars;
        dream_pillars.fill(0.5f + lucid * 0.5f);
        dream_pillars[PILLAR_MEMORY] = lucid;
        creature->apply_pillar_influence(dream_pillars, lucid * 0.2f);
    }
    
    if (ca_grid_->flags[cell_index].in_shadow) {
        float shadow = ca_grid_->shadow_state[cell_index] / 255.0f;
        PillarStateVector shadow_pillars;
        shadow_pillars.fill(0.5f - shadow * 0.3f);
        shadow_pillars[PILLAR_DISTORTION] = shadow;
        creature->apply_pillar_influence(shadow_pillars, shadow * 0.15f);
    }
}

void ALifeGrowthManager::set_config(const OrganismSpawnConfig& config) {
    config_ = config;
}

bool ALifeGrowthManager::is_valid_spawn_location(size_t x, size_t y, size_t z) const {
    if (!ca_grid_) return false;
    
    if (x >= ca_grid_->width || y >= ca_grid_->height || z >= ca_grid_->depth) {
        return false;
    }
    
    return true;
}

PillarStateVector ALifeGrowthManager::extract_pillar_state(size_t cell_index) const {
    PillarStateVector pillars;
    pillars.fill(0.5f);
    
    if (!pillar_grid_) return pillars;
    
    float* pillar_ptrs[] = {
        pillar_grid_->alignment, pillar_grid_->resonance,
        pillar_grid_->stability, pillar_grid_->entropy,
        pillar_grid_->coherence, pillar_grid_->flux,
        pillar_grid_->manifestation, pillar_grid_->dissolution
    };
    
    for (int i = 0; i < 8 && i < NUM_PILLARS; i++) {
        if (pillar_ptrs[i]) {
            pillars.pillars[i] = pillar_ptrs[i][cell_index];
        }
    }
    
    return pillars;
}

bool ALifeGrowthManager::can_spawn(size_t x, size_t y, size_t z) const {
    if (!ca_grid_) return false;
    
    size_t idx = get_cell_index(*ca_grid_, x, y, z);
    
    if (ca_grid_->flags[idx].alive) return false;
    
    if (ca_grid_->life_state[idx] == 0) return false;
    
    if (ca_grid_->energy[idx] < config_.min_energy_threshold) return false;
    if (ca_grid_->matter[idx] < config_.min_matter_threshold) return false;
    
    return true;
}

uint32_t ALifeGrowthManager::determine_creature_type(size_t cell_index) const {
    if (!ca_grid_) return 0;
    
    uint8_t org_type = ca_grid_->organism_type[cell_index];
    
    return org_type % 8;
}

std::string ALifeGrowthManager::get_creature_type_name(uint32_t type_id) const {
    static const std::array<std::string, 8> type_names = {
        "mineral", "plant", "animal", "sentient", "phantom", "hybrid", "plant", "animal"
    };
    
    if (type_id < type_names.size()) {
        return type_names[type_id];
    }
    return "animal";
}

float ALifeGrowthManager::calculate_spawn_priority(size_t cell_index) const {
    if (!ca_grid_) return 0.0f;
    
    float energy_factor = ca_grid_->energy[cell_index] / 100.0f;
    float matter_factor = ca_grid_->matter[cell_index] / 50.0f;
    float life_factor = ca_grid_->life_state[cell_index] / 255.0f;
    
    float pillar_influence = 0.0f;
    if (pillar_grid_ && pillar_grid_->coherence) {
        pillar_influence = pillar_grid_->coherence[cell_index];
    }
    
    float priority = (energy_factor * 0.4f + matter_factor * 0.3f + 
                      life_factor * 0.2f + pillar_influence * 0.1f);
    
    return std::fmax(0.0f, std::fmin(1.0f, priority));
}

std::vector<uint32_t> ALifeGrowthManager::get_organism_ids() const {
    return active_organisms_;
}

ALifePillarCoupler::ALifePillarCoupler() {
}

void ALifePillarCoupler::link_to_grid(PillarGridSoA* grid) {
    pillar_grid_ = grid;
}

void ALifePillarCoupler::link_to_creatures(GrowthSystem* creatures) {
    creatures_ = creatures;
}

void ALifePillarCoupler::sync_pillars_to_organisms(float dt) {
    if (!pillar_grid_ || !creatures_) return;
    
    for (uint32_t entity_id : creatures_->get_alive_creatures()) {
        auto* creature = creatures_->get_creature(entity_id);
        if (!creature) continue;
        
        float x = 0.0f, y = 0.0f, z = 0.0f;
        
        size_t grid_x = static_cast<size_t>(std::fmax(0.0f, x));
        size_t grid_y = static_cast<size_t>(std::fmax(0.0f, y));
        size_t grid_z = static_cast<size_t>(std::fmax(0.0f, z));
        
        PillarStateVector grid_pillars = sample_pillars_at(grid_x, grid_y, grid_z);
        creature->apply_pillar_influence(grid_pillars, dt * DEFAULT_SYNC_STRENGTH);
    }
}

void ALifePillarCoupler::sync_organisms_to_pillars(float dt) {
    if (!pillar_grid_ || !creatures_) return;
    
    for (uint32_t entity_id : creatures_->get_alive_creatures()) {
        auto* creature = creatures_->get_creature(entity_id);
        if (!creature) continue;
        
        float x = 0.0f, y = 0.0f, z = 0.0f;
        
        size_t grid_x = static_cast<size_t>(std::fmax(0.0f, x));
        size_t grid_y = static_cast<size_t>(std::fmax(0.0f, y));
        size_t grid_z = static_cast<size_t>(std::fmax(0.0f, z));
        
        auto state = creature->get_state();
        
        if (pillar_grid_->alignment) {
            pillar_grid_->alignment[grid_x + grid_y * 64] += state.alertness * dt * 0.1f;
        }
        if (pillar_grid_->coherence) {
            pillar_grid_->coherence[grid_x + grid_y * 64] += state.overall_health * dt * 0.05f;
        }
    }
}

void ALifePillarCoupler::apply_pillar_influence_to_grid(const PillarStateVector& pillar_state,
                                                        size_t center_x, size_t center_y, size_t center_z,
                                                        float radius) {
    if (!pillar_grid_) return;
    
    int radius_i = static_cast<int>(radius);
    
    for (int dz = -radius_i; dz <= radius_i; dz++) {
        for (int dy = -radius_i; dy <= radius_i; dy++) {
            for (int dx = -radius_i; dx <= radius_i; dx++) {
                size_t x = center_x + dx;
                size_t y = center_y + dy;
                size_t z = center_z + dz;
                
                if (x >= 64 || y >= 64 || z >= 1) continue;
                
                float dist = std::sqrtf(static_cast<float>(dx*dx + dy*dy + dz*dz));
                if (dist > radius) continue;
                
                float falloff = 1.0f - (dist / radius);
                
                size_t idx = x + y * 64 + z * 4096;
                
                if (pillar_grid_->alignment) {
                    pillar_grid_->alignment[idx] += pillar_state[PILLAR_AWARENESS] * falloff * 0.1f;
                }
                if (pillar_grid_->coherence) {
                    pillar_grid_->coherence[idx] += pillar_state[PILLAR_COHESION] * falloff * 0.1f;
                }
            }
        }
    }
}

float ALifePillarCoupler::calculate_pillar_pressure_at(size_t x, size_t y, size_t z) const {
    if (!pillar_grid_) return 0.0f;
    
    size_t idx = x + y * 64 + z * 4096;
    
    float total = 0.0f;
    float count = 0.0f;
    
    if (pillar_grid_->flux) { total += pillar_grid_->flux[idx]; count++; }
    if (pillar_grid_->pressure) { total += pillar_grid_->pressure[idx]; count++; }
    if (pillar_grid_->resonance) { total += pillar_grid_->resonance[idx]; count++; }
    
    return count > 0.0f ? total / count : 0.0f;
}

void ALifePillarCoupler::set_influence_direction(PillarInfluenceDirection dir) {
    influence_direction_ = dir;
}

PillarStateVector ALifePillarCoupler::sample_pillars_at(size_t x, size_t y, size_t z) const {
    PillarStateVector pillars;
    pillars.fill(0.5f);
    
    if (!pillar_grid_) return pillars;
    
    size_t idx = x + y * 64 + z * 4096;
    
    float* pillar_ptrs[] = {
        pillar_grid_->alignment, pillar_grid_->resonance,
        pillar_grid_->stability, pillar_grid_->entropy,
        pillar_grid_->coherence, pillar_grid_->flux,
        pillar_grid_->manifestation, pillar_grid_->dissolution
    };
    
    for (int i = 0; i < 8 && i < NUM_PILLARS; i++) {
        if (pillar_ptrs[i]) {
            pillars.pillars[i] = pillar_ptrs[i][idx];
        }
    }
    
    return pillars;
}

void ALifePillarCoupler::write_pillars_at(size_t x, size_t y, size_t z, const PillarStateVector& pillars) {
    if (!pillar_grid_) return;
    
    size_t idx = x + y * 64 + z * 4096;
    
    if (pillar_grid_->alignment) pillar_grid_->alignment[idx] = pillars.pillars[0];
    if (pillar_grid_->resonance) pillar_grid_->resonance[idx] = pillars.pillars[1];
    if (pillar_grid_->stability) pillar_grid_->stability[idx] = pillars.pillars[2];
    if (pillar_grid_->entropy) pillar_grid_->entropy[idx] = pillars.pillars[3];
    if (pillar_grid_->coherence) pillar_grid_->coherence[idx] = pillars.pillars[4];
    if (pillar_grid_->flux) pillar_grid_->flux[idx] = pillars.pillars[5];
    if (pillar_grid_->manifestation) pillar_grid_->manifestation[idx] = pillars.pillars[6];
    if (pillar_grid_->dissolution) pillar_grid_->dissolution[idx] = pillars.pillars[7];
}

ALifeSpawnController::ALifeSpawnController()
    : manager_(nullptr), max_spawns_per_frame_(4), spawn_interval_(0.1f),
      time_since_last_spawn_(0.0f), spawning_paused_(false) {
    reset_statistics();
}

void ALifeSpawnController::set_manager(ALifeGrowthManager* manager) {
    manager_ = manager;
}

void ALifeSpawnController::set_max_spawns_per_frame(uint32_t max_spawns) {
    max_spawns_per_frame_ = max_spawns;
}

void ALifeSpawnController::set_spawn_interval(float interval_seconds) {
    spawn_interval_ = interval_seconds;
}

void ALifeSpawnController::trigger_burst_spawn(size_t center_x, size_t center_y, size_t center_z, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        int offset_x = static_cast<int>((i % 3) - 1);
        int offset_y = static_cast<int>((i / 3) % 3 - 1);
        pending_spawns_.push_back({
            static_cast<size_t>(center_x + offset_x),
            static_cast<size_t>(center_y + offset_y),
            center_z
        });
    }
}

void ALifeSpawnController::pause_spawning() {
    spawning_paused_ = true;
}

void ALifeSpawnController::resume_spawning() {
    spawning_paused_ = false;
}

void ALifeSpawnController::update(float dt) {
    if (spawning_paused_ || !manager_) return;
    
    time_since_last_spawn_ += dt;
    
    if (time_since_last_spawn_ >= spawn_interval_) {
        time_since_last_spawn_ = 0.0f;
        
        uint32_t spawned_this_frame = 0;
        
        for (const auto& spawn_pos : pending_spawns_) {
            if (spawned_this_frame >= max_spawns_per_frame_) break;
            
            auto result = manager_->try_spawn_at(
                std::get<0>(spawn_pos),
                std::get<1>(spawn_pos),
                std::get<2>(spawn_pos)
            );
            
            if (result.success) {
                stats_.total_spawns++;
            } else {
                stats_.failed_spawns++;
            }
            spawned_this_frame++;
        }
        
        pending_spawns_.clear();
    }
}

ALifeSpawnController::SpawnStatistics ALifeSpawnController::get_statistics() const {
    SpawnStatistics stats = stats_;
    if (manager_) {
        stats.active_organisms = static_cast<uint32_t>(manager_->get_active_organism_count());
    }
    return stats;
}

void ALifeSpawnController::reset_statistics() {
    stats_ = {};
    stats_.total_spawns = 0;
    stats_.failed_spawns = 0;
    stats_.active_organisms = 0;
    stats_.average_spawn_priority = 0.0f;
}

DreamStateCreatureEffects::DreamStateCreatureEffects()
    : dream_intensity_(0.0f), enabled_effects_(0) {
    enable_effect(EFFECT_GLOW);
}

void DreamStateCreatureEffects::set_dream_intensity(float intensity) {
    dream_intensity_ = std::fmax(0.0f, std::fmin(1.0f, intensity));
}

void DreamStateCreatureEffects::apply_dream_effects_to_creature(Creature* creature, float dt) {
    if (!creature || dream_intensity_ <= 0.0f) return;
    
    if (is_effect_enabled(EFFECT_GLOW)) apply_glow_effect(creature);
    if (is_effect_enabled(EFFECT_COLOR_SHIFT)) apply_color_shift_effect(creature);
    if (is_effect_enabled(EFFECT_SIZE_PULSE)) apply_size_pulse_effect(creature, dt);
    
    PillarStateVector dream_pillars;
    dream_pillars.fill(0.5f + dream_intensity_ * 0.5f);
    dream_pillars[PILLAR_MEMORY] = dream_intensity_;
    dream_pillars[PILLAR_DISTORTION] = dream_intensity_ * 0.3f;
    creature->apply_pillar_influence(dream_pillars, dt * 0.1f);
}

void DreamStateCreatureEffects::apply_dream_effects_to_skeleton(Skeleton* skeleton, float dt) {
    if (!skeleton || dream_intensity_ <= 0.0f) return;
    
    if (is_effect_enabled(EFFECT_DISTORTION)) apply_distortion_effect(skeleton);
    if (is_effect_enabled(EFFECT_FLOAT)) apply_float_effect(skeleton, dt);
}

void DreamStateCreatureEffects::enable_effect(DreamEffectType effect) {
    enabled_effects_ |= (1u << effect);
}

void DreamStateCreatureEffects::disable_effect(DreamEffectType effect) {
    enabled_effects_ &= ~(1u << effect);
}

bool DreamStateCreatureEffects::is_effect_enabled(DreamEffectType effect) const {
    return (enabled_effects_ & (1u << effect)) != 0;
}

void DreamStateCreatureEffects::apply_glow_effect(Creature* creature) {
}

void DreamStateCreatureEffects::apply_distortion_effect(Skeleton* skeleton) {
    if (!skeleton) return;
    
    float distortion_amount = dream_intensity_ * 0.1f;
    
    for (auto& bone : skeleton->bones) {
        if (bone && !bone->is_fractured()) {
        }
    }
}

void DreamStateCreatureEffects::apply_float_effect(Skeleton* skeleton, float dt) {
    if (!skeleton) return;
    
    for (auto& node : skeleton->nodes) {
        if (node) {
        }
    }
}

void DreamStateCreatureEffects::apply_color_shift_effect(Creature* creature) {
}

void DreamStateCreatureEffects::apply_size_pulse_effect(Creature* creature, float dt) {
}

ShadowStateCreatureEffects::ShadowStateCreatureEffects()
    : shadow_intensity_(0.0f), enabled_effects_(0) {
    enable_effect(EFFECT_DARKEN);
    enable_effect(EFFECT_WEAKEN);
}

void ShadowStateCreatureEffects::set_shadow_intensity(float intensity) {
    shadow_intensity_ = std::fmax(0.0f, std::fmin(1.0f, intensity));
}

void ShadowStateCreatureEffects::apply_shadow_effects_to_creature(Creature* creature, float dt) {
    if (!creature || shadow_intensity_ <= 0.0f) return;
    
    if (is_effect_enabled(EFFECT_DARKEN)) apply_darken_effect(creature);
    if (is_effect_enabled(EFFECT_DECAY)) apply_decay_effect(creature, dt);
    
    PillarStateVector shadow_pillars;
    shadow_pillars.fill(0.5f - shadow_intensity_ * 0.3f);
    shadow_pillars[PILLAR_HARM] = shadow_intensity_;
    shadow_pillars[PILLAR_DISTORTION] = shadow_intensity_ * 1.5f;
    creature->apply_pillar_influence(shadow_pillars, dt * 0.15f);
}

void ShadowStateCreatureEffects::apply_shadow_effects_to_skeleton(Skeleton* skeleton, float dt) {
    if (!skeleton || shadow_intensity_ <= 0.0f) return;
    
    if (is_effect_enabled(EFFECT_FRACTURE)) apply_fracture_effect(skeleton);
    if (is_effect_enabled(EFFECT_STIFFEN)) apply_stiffen_effect(skeleton);
}

void ShadowStateCreatureEffects::enable_effect(ShadowEffectType effect) {
    enabled_effects_ |= (1u << effect);
}

void ShadowStateCreatureEffects::disable_effect(ShadowEffectType effect) {
    enabled_effects_ &= ~(1u << effect);
}

bool ShadowStateCreatureEffects::is_effect_enabled(ShadowEffectType effect) const {
    return (enabled_effects_ & (1u << effect)) != 0;
}

void ShadowStateCreatureEffects::apply_darken_effect(Creature* creature) {
}

void ShadowStateCreatureEffects::apply_fracture_effect(Skeleton* skeleton) {
    if (!skeleton) return;
    
    float fracture_chance = shadow_intensity_ * 0.01f;
    
    for (auto& bone : skeleton->bones) {
        if (bone && !bone->is_fractured() && fracture_chance > 0.001f) {
            if (((float)rand() / RAND_MAX) < fracture_chance) {
                bone->fracture();
            }
        }
    }
}

void ShadowStateCreatureEffects::apply_weaken_effect(Skeleton* skeleton) {
    if (!skeleton) return;
    
    float weaken_factor = 1.0f - shadow_intensity_ * 0.2f;
    
    for (auto& bone : skeleton->bones) {
        if (bone) {
            float new_threshold = bone->get_break_threshold() * weaken_factor;
        }
    }
}

void ShadowStateCreatureEffects::apply_stiffen_effect(Skeleton* skeleton) {
    if (!skeleton) return;
    
    float stiffen_factor = 1.0f + shadow_intensity_ * 0.5f;
    
    for (auto& bone : skeleton->bones) {
        if (bone) {
            bone->set_flexibility(bone->get_flexibility() / stiffen_factor);
        }
    }
}

void ShadowStateCreatureEffects::apply_decay_effect(Creature* creature, float dt) {
    if (!creature) return;
    
    float decay_rate = shadow_intensity_ * 0.05f;
    auto state = creature->get_state();
    
    if (state.overall_health > 0.0f) {
    }
}

ALifeGrowthManager* create_alife_growth_manager() {
    return new ALifeGrowthManager();
}

void destroy_alife_growth_manager(ALifeGrowthManager* manager) {
    if (manager) {
        manager->shutdown();
        delete manager;
    }
}

}