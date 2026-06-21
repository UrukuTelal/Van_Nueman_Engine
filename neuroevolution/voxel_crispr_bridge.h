#pragma once

#include "../biology/voxel_organism.h"
#include <vector>
#include <cstdint>
#include <algorithm>

namespace Van_Nueman {

class VoxelCRISPRBridge {
public:
    VoxelCRISPRBridge(uint32_t max_entries = 128);

    void store_genome(const VoxelOrganismGenome& genome, float fitness, uint64_t environment_hash = 0);

    std::vector<VoxelOrganismGenome> get_top_genomes(uint32_t n) const;

    void seed_population(std::vector<VoxelOrganismGenome>& population, float seed_ratio = 0.2f) const;

    float get_best_fitness() const { return best_fitness_; }

    const VoxelOrganismGenome* get_best_genome() const;

    uint32_t size() const { return static_cast<uint32_t>(entries_.size()); }

    uint32_t max_entries() const { return max_entries_; }

private:
    struct VaultEntry {
        VoxelOrganismGenome genome;
        float fitness;
        uint64_t environment_hash;
        uint32_t generation_stored;
    };

    std::vector<VaultEntry> entries_;
    uint32_t max_entries_;
    float best_fitness_;
    uint32_t next_gen_id_;
};

}
