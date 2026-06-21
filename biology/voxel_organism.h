#pragma once

#include "../voxel/VoxelCell.h"
#include "../voxel/GrowthPipeline.h"
#include "../voxel/BCCIndex.h"
#include "../include/Entity.h"
#include "../include/Constraint.h"
#include "../include/SkellyTypes.h"
#include "voxel_skelly_bridge.h"
#include "creature_system.h"
#include <cstdint>
#include <vector>
#include <cstdlib>

struct EnvironmentalSample {
    vn::fp20_t light_level;
    vn::fp20_t nutrient_level;
    vn::fp20_t temperature;
    vn::fp20_t toxicity;
};

struct VoxelOrganismGenome {
    float nn_weights[256];
    uint64_t lineage_hash;
    float metabolism_rate;
    float photosynthesis_efficiency;
    float mutation_rate;
    uint32_t max_cells;
    float memory_retention;
    std::vector<uint64_t> ancestor_hashes;

    VoxelOrganismGenome();
};

class VoxelOrganism {
public:
    VoxelOrganism();

    void init_zygote(const BCCCoord& position, const YieldMatrix& cell_material,
                     const VoxelOrganismGenome& genome,
                     const PillarStateVector& baseline);

    bool tick(vn::fp20_t dt, const EnvironmentalSample* env);

    uint32_t cell_count() const { return static_cast<uint32_t>(body_cells_.size()); }
    bool is_alive() const { return alive_; }
    uint32_t get_id() const { return organism_id; }
    float calculate_fitness() const;
    const PillarStateVector& pillar_state() const { return pillar_state_; }
    Van_Nueman::GrowthStage growth_stage() const;
    vn::fp20_t body_size() const;
    uint32_t boundary_cell_count() const;

    VoxelOrganism* reproduce();

    static void mutate_genome(VoxelOrganismGenome& base);

    const VoxelCell& get_cell(uint32_t index) const { return body_cells_[index]; }
    BCCCoord get_cell_coord(uint32_t index) const { return body_cells_[index].coord; }
    std::vector<uint32_t> find_boundary_cells() const;

private:
    bool metabolize(vn::fp20_t dt);
    void sense_environment(const EnvironmentalSample* env);
    void diffuse_pillars(vn::fp20_t dt);
    void circulate_warmth(vn::fp20_t dt);
    void process_harm(vn::fp20_t dt);
    uint32_t try_grow(vn::fp20_t dt);
    void repair_damage(vn::fp20_t dt);
    void apply_body_axes();
    void rebuild_skelly_mapping();
    void apply_skelly_physics(vn::fp20_t dt);
    void die();
    void aggregate_pillar_state();
    void harvest_energy(vn::fp20_t dt);
    void rebuild_if_dirty();

    bool is_occupied(const BCCCoord& coord) const;
    void compute_face_stresses(const VoxelCell& cell, vn::fp20_t out_stresses[VoxelCell::FACE_COUNT]) const;
    std::vector<BCCCoord> get_neighbors(const BCCCoord& coord) const;

    std::vector<VoxelCell> body_cells_;
    VoxelOrganismGenome genome_;
    uint32_t organism_id;
    PillarStateVector pillar_state_;
    PillarStateVector baseline_pillars_;
    ConstraintState constraint_;
    ModularSkeleton skeleton_;
    Van_Nueman::VoxelSkellyBridge skelly_bridge_;

    vn::fp20_t age_;
    vn::fp20_t energy_reserve_;
    vn::fp20_t overall_health_;
    vn::fp20_t body_temperature_;
    vn::fp20_t growth_progress_;
    vn::fp20_t accumulated_harm_;
    bool alive_;
    bool skelly_dirty_;
    uint32_t reproduction_count_;

    vn::fp20_t env_light_level_;
    vn::fp20_t env_nutrient_level_;
    vn::fp20_t env_temperature_;

    std::vector<Van_Nueman::VoxelBoneMapping> bone_mappings_;
    std::vector<Van_Nueman::VoxelJointMapping> joint_mappings_;
    std::vector<Van_Nueman::VoxelMultiJointMapping> multi_joint_mappings_;
    std::vector<Van_Nueman::VoxelMuscleMapping> muscle_mappings_;
    std::vector<Van_Nueman::VoxelOrganMapping> organ_mappings_;

    static uint32_t next_organism_id_;
};

VoxelOrganism* create_voxel_organism(const BCCCoord& position,
                                     const VoxelOrganismGenome& dna,
                                     vn::fp20_t scale);
