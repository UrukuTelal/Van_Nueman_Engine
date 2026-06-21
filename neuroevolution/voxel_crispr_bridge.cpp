#include "voxel_crispr_bridge.h"
#include <cstdlib>
#include <ctime>
#include <random>

namespace Van_Nueman {

VoxelCRISPRBridge::VoxelCRISPRBridge(uint32_t max_entries)
    : max_entries_(max_entries), best_fitness_(0.0f), next_gen_id_(0) {}

void VoxelCRISPRBridge::store_genome(const VoxelOrganismGenome& genome, float fitness, uint64_t environment_hash) {
    if (fitness <= 0.0f) return;

    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->fitness < fitness * 0.1f) {
            entries_.erase(it);
            break;
        }
    }

    if (entries_.size() >= max_entries_) {
        auto it = std::min_element(entries_.begin(), entries_.end(),
            [](const VaultEntry& a, const VaultEntry& b) { return a.fitness < b.fitness; });
        if (it != entries_.end() && it->fitness < fitness) {
            *it = { genome, fitness, environment_hash, next_gen_id_++ };
        }
        return;
    }

    entries_.push_back({ genome, fitness, environment_hash, next_gen_id_++ });

    if (fitness > best_fitness_) best_fitness_ = fitness;

    std::sort(entries_.begin(), entries_.end(),
        [](const VaultEntry& a, const VaultEntry& b) { return a.fitness > b.fitness; });
}

std::vector<VoxelOrganismGenome> VoxelCRISPRBridge::get_top_genomes(uint32_t n) const {
    std::vector<VoxelOrganismGenome> result;
    uint32_t count = std::min(n, static_cast<uint32_t>(entries_.size()));
    result.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        result.push_back(entries_[i].genome);
    }
    return result;
}

const VoxelOrganismGenome* VoxelCRISPRBridge::get_best_genome() const {
    if (entries_.empty()) return nullptr;
    return &entries_[0].genome;
}

void VoxelCRISPRBridge::seed_population(std::vector<VoxelOrganismGenome>& population, float seed_ratio) const {
    if (entries_.empty() || population.empty()) return;

    std::srand(static_cast<unsigned>(std::time(nullptr)));

    uint32_t seed_count = static_cast<uint32_t>(population.size() * seed_ratio);
    if (seed_count < 1) seed_count = 1;

    for (uint32_t i = 0; i < seed_count && i < population.size(); i++) {
        const VaultEntry& src = entries_[i % entries_.size()];
        population[i] = src.genome;

        for (int w = 0; w < 256; w++) {
            float r = static_cast<float>(std::rand()) / RAND_MAX;
            if (r < 0.01f) {
                population[i].nn_weights[w] += (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * 0.05f;
                if (population[i].nn_weights[w] < -1.0f) population[i].nn_weights[w] = -1.0f;
                if (population[i].nn_weights[w] > 1.0f) population[i].nn_weights[w] = 1.0f;
            }
        }
    }
}

}
