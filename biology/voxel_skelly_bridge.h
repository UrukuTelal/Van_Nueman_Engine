#pragma once

#include "../include/SkellyTypes.h"
#include "../include/Entity.h"
#include "../include/Constraint.h"
#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace Van_Nueman {

struct VoxelBoneMapping {
    uint32_t bone_id;
    std::vector<BCCCoord> cell_coords;
    uint32_t cell_count;
    vn::fp20_t avg_integrity;
    vn::fp20_t flexibility;
    vn::fp20_t break_threshold;
    vn::fp20_t length;

    SkeletonNode* start_node;
    SkeletonNode* end_node;
};

struct VoxelJointMapping {
    uint32_t joint_id;
    BCCCoord joint_cell;
    uint32_t bone_a_id;
    uint32_t bone_b_id;
    vn::fp20_t angular_displacement;
    vn::fp20_t pitch_min, pitch_max;
    vn::fp20_t yaw_min, yaw_max;
    vn::fp20_t roll_min, roll_max;

    SkeletonNode* node;
};

struct VoxelMultiJointMapping {
    uint32_t multi_joint_id;
    BCCCoord center_cell;
    std::vector<uint32_t> bone_ids;
    vn::fp20_t torsion_tension;

    ConstraintState constraint;
};

struct VoxelMuscleMapping {
    uint32_t muscle_id;
    std::vector<BCCCoord> cell_coords;
    uint32_t bone_a_id;
    uint32_t bone_b_id;
    vn::fp20_t activation;
    vn::fp20_t max_force;

    MuscleGroup* muscle;
};

struct VoxelOrganMapping {
    uint32_t organ_id;
    BCCCoord center_cell;
    OrganType type;
    vn::fp20_t volume;
    vn::fp20_t active_state;
    std::vector<BCCCoord> constituent_cells;

    Organ* organ;
};

class VoxelSkellyBridge {
public:
    VoxelSkellyBridge();

    uint32_t map_cells_to_skeleton(
        const std::vector<VoxelCell>& cells,
        ModularSkeleton& out_skeleton);

    uint32_t detect_bone_chains(
        const std::vector<VoxelCell>& cells,
        std::vector<VoxelBoneMapping>& out_bones);

    uint32_t detect_joints(
        const std::vector<VoxelBoneMapping>& bones,
        const std::vector<VoxelCell>& cells,
        std::vector<VoxelJointMapping>& out_joints,
        std::vector<VoxelMultiJointMapping>& out_multi_joints);

    uint32_t detect_muscle_groups(
        const std::vector<VoxelBoneMapping>& bones,
        const std::vector<VoxelJointMapping>& joints,
        const std::vector<VoxelCell>& cells,
        std::vector<VoxelMuscleMapping>& out_muscles);

    uint32_t detect_organs(
        const std::vector<VoxelCell>& cells,
        std::vector<VoxelOrganMapping>& out_organs);

    void update_skeleton_from_cells(
        const std::vector<VoxelCell>& cells,
        ModularSkeleton& skeleton);

    void apply_skelly_forces_to_cells(
        const ModularSkeleton& skeleton,
        std::vector<VoxelCell>& cells);

    static vn::fp20_t compute_chain_flexibility(
        const std::vector<BCCCoord>& chain_coords,
        const std::vector<VoxelCell>& cells);

    static vn::fp20_t compute_break_threshold(
        const std::vector<BCCCoord>& chain_coords,
        const std::vector<VoxelCell>& cells);

    static vn::fp20_t apply_joint_torsion(
        VoxelJointMapping& joint,
        vn::fp20_t applied_angular_shift,
        vn::fp20_t willpower,
        vn::fp20_t depth);

    static bool cells_are_adjacent(const BCCCoord& a, const BCCCoord& b);

    static int find_shared_face(const BCCCoord& a, const BCCCoord& b);

private:
    uint32_t next_bone_id_ = 0;
    uint32_t next_joint_id_ = 0;
    uint32_t next_muscle_id_ = 0;
    uint32_t next_organ_id_ = 0;

    void trace_cell_chain(
        BCCCoord seed,
        const std::vector<VoxelCell>& cells,
        std::vector<BCCCoord>& out_chain);
};

}
