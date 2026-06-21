#pragma once

#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"
#include "../biology/voxel_organism.h"
#include "../scale/ScaleExponent.h"
#include "voxel_mesh_generator.h"
#include <cstdint>
#include <vector>

namespace Van_Nueman {

struct LODCell {
    BCCCoord coord;
    vn::fp20_t avg_pillars[NumPillars];
    float alpha;
    float size;
    bool is_boundary;
    bool all_fractured;
    uint32_t child_count;
    uint32_t merged_from;
};

class ScaleOrganismRenderer {
public:
    ScaleOrganismRenderer();

    void set_observer_scale(ScaleExponent scale) { observer_scale_ = scale; }
    ScaleExponent get_observer_scale() const { return observer_scale_; }

    void generate_organism_mesh(const VoxelOrganism& organism,
                                 std::vector<VoxelMeshData>& out_meshes);

    float get_lod_blend_factor(ScaleExponent organism_scale) const;

private:
    ScaleExponent observer_scale_;

    void generate_lod_mesh(const std::vector<LODCell>& lod_cells,
                            float blend_factor,
                            std::vector<VoxelMeshData>& out_meshes);

    void build_lod_cells(const VoxelOrganism& organism,
                          float blend_factor,
                          std::vector<LODCell>& out_cells);

    void merge_adjacent_cells(const std::vector<LODCell>& input_cells,
                               float merge_threshold,
                               std::vector<LODCell>& out_merged);


};

}
