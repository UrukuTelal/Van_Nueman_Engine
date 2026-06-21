#pragma once

#include "../biology/voxel_organism.h"
#include <functional>

namespace Van_Nueman {

std::function<float(const VoxelOrganismGenome&)> create_organism_fitness(
    uint32_t max_ticks = 500,
    vn::fp20_t light_level = vn::fp20_t(0.5f),
    vn::fp20_t nutrient_level = vn::fp20_t(0.5f),
    vn::fp20_t temperature = vn::fp20_t(37.0f),
    vn::fp20_t toxicity = vn::fp20_t(0.0f)
);

float evaluate_organism_fitness(const VoxelOrganismGenome& genome,
                                 uint32_t max_ticks,
                                 const EnvironmentalSample& env);

}
