#include "voxel_neuro_bridge.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <random>

namespace Van_Nueman {

void nn_forward_pass(const float inputs[16], const float weights[256], float outputs[16]) {
    for (int i = 0; i < 16; i++) {
        float sum = 0.0f;
        for (int j = 0; j < 16; j++) {
            sum += inputs[j] * weights[i * 16 + j];
        }
        outputs[i] = 1.0f / (1.0f + std::exp(-sum));
    }
}

void organism_to_nn_input(const VoxelOrganism& org, float inputs[16]) {
    for (int p = 0; p < 16; p++) {
        inputs[p] = org.pillar_state()[p];
    }
}

void nn_output_to_decisions(const float outputs[16], GrowthDecision& decisions) {
    decisions.division_threshold_bias = (outputs[0] - 0.5f) * 0.4f;
    for (int f = 0; f < 14; f++) {
        decisions.face_bias[f] = (outputs[1 + f] - 0.5f) * 2.0f;
    }
    decisions.metabolism_bias = (outputs[15] - 0.5f) * 0.2f;
}

void genome_to_individual(const VoxelOrganismGenome& genome, Individual& individual) {
    individual.fitness = 0.0f;
    individual.generation = 0;
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            individual.weights[i][j] = genome.nn_weights[i * 16 + j];
        }
    }
}

void individual_to_genome(const Individual& individual, VoxelOrganismGenome& genome) {
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            genome.nn_weights[i * 16 + j] = individual.weights[i][j];
        }
    }
}

VoxelNeuroPopulation::VoxelNeuroPopulation(uint32_t population_size)
    : population_size_(population_size), generation_(0), best_fitness_(0.0f) {
    population_.resize(population_size_);
    fitness_scores_.resize(population_size_, 0.0f);
    fitness_fn_ = [](const VoxelOrganismGenome&) { return 0.0f; };
}

VoxelNeuroPopulation::~VoxelNeuroPopulation() {}

void VoxelNeuroPopulation::initialize_random() {
    for (auto& genome : population_) {
        genome = VoxelOrganismGenome();
    }
}

void VoxelNeuroPopulation::seed_from(const std::vector<VoxelOrganismGenome>& seeds, float ratio) {
    if (seeds.empty() || population_.empty()) return;

    uint32_t seed_count = static_cast<uint32_t>(population_.size() * ratio);
    if (seed_count < 1) seed_count = 1;
    if (seed_count > static_cast<uint32_t>(seeds.size())) seed_count = static_cast<uint32_t>(seeds.size());

    for (uint32_t i = 0; i < seed_count; i++) {
        population_[i] = seeds[i];
    }
}

void VoxelNeuroPopulation::set_fitness_function(std::function<float(const VoxelOrganismGenome&)> fn) {
    fitness_fn_ = std::move(fn);
}

void VoxelNeuroPopulation::evolve_generation() {
    for (uint32_t i = 0; i < population_size_; i++) {
        fitness_scores_[i] = fitness_fn_(population_[i]);
    }

    std::vector<VoxelOrganismGenome> selected = tournament_selection();

    for (uint32_t i = 0; i + 1 < population_size_; i += 2) {
        VoxelOrganismGenome child_a, child_b;
        crossover(selected[i], selected[i + 1], child_a, child_b);
        mutate(child_a);
        mutate(child_b);
        population_[i] = child_a;
        population_[i + 1] = child_b;
    }

    generation_++;

    float max_fitness = 0.0f;
    for (uint32_t i = 0; i < population_size_; i++) {
        if (fitness_scores_[i] > max_fitness) {
            max_fitness = fitness_scores_[i];
        }
    }
    best_fitness_ = max_fitness;
}

VoxelOrganismGenome VoxelNeuroPopulation::get_best_genome() const {
    uint32_t best_idx = 0;
    for (uint32_t i = 1; i < population_size_; i++) {
        if (fitness_scores_[i] > fitness_scores_[best_idx]) {
            best_idx = i;
        }
    }
    return population_[best_idx];
}

std::vector<VoxelOrganismGenome> VoxelNeuroPopulation::tournament_selection() {
    std::vector<VoxelOrganismGenome> selected;
    selected.reserve(population_size_);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, population_size_ - 1);

    for (uint32_t i = 0; i < population_size_; i++) {
        uint32_t a = dis(gen);
        uint32_t b = dis(gen);
        selected.push_back(fitness_scores_[a] >= fitness_scores_[b] ? population_[a] : population_[b]);
    }

    return selected;
}

void VoxelNeuroPopulation::crossover(const VoxelOrganismGenome& a, const VoxelOrganismGenome& b,
                                      VoxelOrganismGenome& child_a, VoxelOrganismGenome& child_b) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> cp_dis(0, 255);
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);

    int cp = cp_dis(gen);

    for (int i = 0; i < cp; i++) {
        child_a.nn_weights[i] = a.nn_weights[i];
        child_b.nn_weights[i] = b.nn_weights[i];
    }
    for (int i = cp; i < 256; i++) {
        child_a.nn_weights[i] = b.nn_weights[i];
        child_b.nn_weights[i] = a.nn_weights[i];
    }

    if (prob(gen) < 0.5f) {
        child_a.metabolism_rate = a.metabolism_rate;
        child_b.metabolism_rate = b.metabolism_rate;
    } else {
        child_a.metabolism_rate = b.metabolism_rate;
        child_b.metabolism_rate = a.metabolism_rate;
    }

    if (prob(gen) < 0.5f) {
        child_a.photosynthesis_efficiency = a.photosynthesis_efficiency;
        child_b.photosynthesis_efficiency = b.photosynthesis_efficiency;
    } else {
        child_a.photosynthesis_efficiency = b.photosynthesis_efficiency;
        child_b.photosynthesis_efficiency = a.photosynthesis_efficiency;
    }

    if (prob(gen) < 0.5f) {
        child_a.mutation_rate = a.mutation_rate;
        child_b.mutation_rate = b.mutation_rate;
    } else {
        child_a.mutation_rate = b.mutation_rate;
        child_b.mutation_rate = a.mutation_rate;
    }

    if (prob(gen) < 0.5f) {
        child_a.max_cells = a.max_cells;
        child_b.max_cells = b.max_cells;
    } else {
        child_a.max_cells = b.max_cells;
        child_b.max_cells = a.max_cells;
    }

    if (prob(gen) < 0.5f) {
        child_a.memory_retention = a.memory_retention;
        child_b.memory_retention = b.memory_retention;
    } else {
        child_a.memory_retention = b.memory_retention;
        child_b.memory_retention = a.memory_retention;
    }

    child_a.lineage_hash = a.lineage_hash ^ b.lineage_hash;
    child_b.lineage_hash = b.lineage_hash ^ a.lineage_hash;

    child_a.ancestor_hashes = a.ancestor_hashes;
    child_b.ancestor_hashes = b.ancestor_hashes;
    if (!a.ancestor_hashes.empty()) {
        child_a.ancestor_hashes.push_back(a.lineage_hash);
    }
    if (!b.ancestor_hashes.empty()) {
        child_b.ancestor_hashes.push_back(b.lineage_hash);
    }
}

void VoxelNeuroPopulation::mutate(VoxelOrganismGenome& genome) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);
    std::uniform_real_distribution<float> delta(-0.1f, 0.1f);

    for (int i = 0; i < 256; i++) {
        if (prob(gen) < 0.05f) {
            genome.nn_weights[i] += delta(gen);
            if (genome.nn_weights[i] < -1.0f) genome.nn_weights[i] = -1.0f;
            if (genome.nn_weights[i] > 1.0f) genome.nn_weights[i] = 1.0f;
        }
    }

    if (prob(gen) < 0.05f) {
        genome.metabolism_rate *= (1.0f + (prob(gen) - 0.5f) * 0.2f);
        if (genome.metabolism_rate < 0.001f) genome.metabolism_rate = 0.001f;
        if (genome.metabolism_rate > 1.0f) genome.metabolism_rate = 1.0f;
    }

    if (prob(gen) < 0.05f) {
        genome.photosynthesis_efficiency *= (1.0f + (prob(gen) - 0.5f) * 0.2f);
        if (genome.photosynthesis_efficiency < 0.0f) genome.photosynthesis_efficiency = 0.0f;
        if (genome.photosynthesis_efficiency > 1.0f) genome.photosynthesis_efficiency = 1.0f;
    }

    if (prob(gen) < 0.05f) {
        genome.mutation_rate += (prob(gen) - 0.5f) * 0.02f;
        if (genome.mutation_rate < 0.001f) genome.mutation_rate = 0.001f;
        if (genome.mutation_rate > 0.5f) genome.mutation_rate = 0.5f;
    }

    if (prob(gen) < 0.05f) {
        float delta_max = (prob(gen) - 0.5f) * 0.2f;
        int new_max = static_cast<int>(static_cast<float>(genome.max_cells) * (1.0f + delta_max));
        if (new_max < 1) new_max = 1;
        if (new_max > 4096) new_max = 4096;
        genome.max_cells = static_cast<uint32_t>(new_max);
    }

    if (prob(gen) < 0.05f) {
        genome.memory_retention += (prob(gen) - 0.5f) * 0.1f;
        if (genome.memory_retention < 0.0f) genome.memory_retention = 0.0f;
        if (genome.memory_retention > 1.0f) genome.memory_retention = 1.0f;
    }
}

}
