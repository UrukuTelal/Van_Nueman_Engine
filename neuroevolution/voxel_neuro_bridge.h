#pragma once

#include "../biology/voxel_organism.h"
#include "neuroevolution.h"
#include <vector>
#include <cstdint>
#include <functional>

namespace Van_Nueman {

void nn_forward_pass(const float inputs[16], const float weights[256], float outputs[16]);

void organism_to_nn_input(const VoxelOrganism& org, float inputs[16]);

struct GrowthDecision {
    float division_threshold_bias;
    float face_bias[14];
    float metabolism_bias;
};

void nn_output_to_decisions(const float outputs[16], GrowthDecision& decisions);

void genome_to_individual(const VoxelOrganismGenome& genome, Individual& individual);

void individual_to_genome(const Individual& individual, VoxelOrganismGenome& genome);

class VoxelNeuroPopulation {
public:
    VoxelNeuroPopulation(uint32_t population_size);
    ~VoxelNeuroPopulation();

    void initialize_random();

    void seed_from(const std::vector<VoxelOrganismGenome>& seeds, float ratio = 0.2f);

    void evolve_generation();

    void set_fitness_function(std::function<float(const VoxelOrganismGenome&)> fn);

    VoxelOrganismGenome get_best_genome() const;

    const std::vector<VoxelOrganismGenome>& get_population() const { return population_; }

    int get_generation() const { return generation_; }

    uint32_t size() const { return population_size_; }

    float get_best_fitness() const { return best_fitness_; }

private:
    uint32_t population_size_;
    int generation_ = 0;
    float best_fitness_ = 0.0f;
    std::vector<VoxelOrganismGenome> population_;
    std::vector<float> fitness_scores_;
    std::function<float(const VoxelOrganismGenome&)> fitness_fn_;

    std::vector<VoxelOrganismGenome> tournament_selection();

    void crossover(const VoxelOrganismGenome& a, const VoxelOrganismGenome& b,
                   VoxelOrganismGenome& child_a, VoxelOrganismGenome& child_b);

    void mutate(VoxelOrganismGenome& genome);
};

}
