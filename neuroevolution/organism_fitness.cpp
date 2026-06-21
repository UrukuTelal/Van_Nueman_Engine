#include "organism_fitness.h"
#include "../voxel/BCCIndex.h"

namespace Van_Nueman {

float evaluate_organism_fitness(const VoxelOrganismGenome& genome,
                                 uint32_t max_ticks,
                                 const EnvironmentalSample& env) {
    srand(42);

    BCCCoord start_pos{0, 0, 0};

    VoxelOrganism* org = create_voxel_organism(start_pos, genome, vn::fp20_t(1.0f));
    if (!org) return 0.0f;

    for (uint32_t t = 0; t < max_ticks; t++) {
        bool alive = org->tick(vn::fp20_t(1.0f), &env);
        if (!alive) break;
    }

    float fitness = org->calculate_fitness();
    delete org;
    return fitness;
}

std::function<float(const VoxelOrganismGenome&)> create_organism_fitness(
    uint32_t max_ticks,
    vn::fp20_t light_level,
    vn::fp20_t nutrient_level,
    vn::fp20_t temperature,
    vn::fp20_t toxicity)
{
    return [max_ticks, light_level, nutrient_level, temperature, toxicity](const VoxelOrganismGenome& genome) -> float {
        EnvironmentalSample env;
        env.light_level = light_level;
        env.nutrient_level = nutrient_level;
        env.temperature = temperature;
        env.toxicity = toxicity;
        return evaluate_organism_fitness(genome, max_ticks, env);
    };
}

}
