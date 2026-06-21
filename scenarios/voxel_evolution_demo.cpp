#include <iostream>
#include <iomanip>
#include <vector>
#include "../neuroevolution/voxel_neuro_bridge.h"
#include "../neuroevolution/organism_fitness.h"
#include "../neuroevolution/voxel_crispr_bridge.h"

static void run_evolution(Van_Nueman::VoxelNeuroPopulation& pop,
                           Van_Nueman::VoxelCRISPRBridge& vault,
                           const char* label,
                           int generations,
                           std::vector<float>& out_fitness_curve) {
    std::cout << "--- " << label << " ---" << std::endl;
    out_fitness_curve.clear();
    out_fitness_curve.reserve(generations);

    for (int gen = 0; gen < generations; gen++) {
        pop.evolve_generation();
        float best = pop.get_best_fitness();
        out_fitness_curve.push_back(best);

        vault.store_genome(pop.get_best_genome(), best, 0);

        if ((gen + 1) % 10 == 0) {
            std::cout << "Gen " << std::setw(3) << (gen + 1)
                      << "  best=" << std::fixed << std::setprecision(4) << best
                      << std::endl;
        }
    }

    VoxelOrganismGenome best_genome = pop.get_best_genome();
    std::cout << "  Final best fitness: " << pop.get_best_fitness() << std::endl;
    std::cout << "  Vault entries: " << vault.size()
              << ", vault best: " << vault.get_best_fitness() << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "=== Voxel Organism Evolution Demo ===" << std::endl;
    std::cout << "Phase 5 + 6: Neuroevolution + CRISPR Memory" << std::endl;
    std::cout << std::endl;

    const uint32_t POP_SIZE = 50;
    const int GENERATIONS = 100;
    const uint32_t SIM_TICKS = 200;

    // Run 1: random initialization
    Van_Nueman::VoxelNeuroPopulation pop1(POP_SIZE);
    pop1.set_fitness_function(Van_Nueman::create_organism_fitness(
        SIM_TICKS, vn::fp20_t(0.5f), vn::fp20_t(0.5f), vn::fp20_t(37.0f), vn::fp20_t(0.0f)));
    pop1.initialize_random();

    Van_Nueman::VoxelCRISPRBridge vault;
    std::vector<float> curve1;
    run_evolution(pop1, vault, "Phase 1: Random initialization", GENERATIONS, curve1);

    // Run 2: seed from vault (accelerated re-adaptation)
    Van_Nueman::VoxelNeuroPopulation pop2(POP_SIZE);
    pop2.set_fitness_function(Van_Nueman::create_organism_fitness(
        SIM_TICKS, vn::fp20_t(0.5f), vn::fp20_t(0.5f), vn::fp20_t(37.0f), vn::fp20_t(0.0f)));
    pop2.initialize_random();

    std::vector<VoxelOrganismGenome> seeds = vault.get_top_genomes(POP_SIZE / 5);
    pop2.seed_from(seeds, 0.2f);

    std::vector<float> curve2;
    run_evolution(pop2, vault, "Phase 2: CRISPR-seeded", GENERATIONS, curve2);

    // Print CSV fitness curves
    std::cout << "=== Fitness Curves ===" << std::endl;
    std::cout << "Generation,Random,CRISPR-Seeded" << std::endl;
    int max_gen = std::max(curve1.size(), curve2.size());
    for (int g = 0; g < max_gen; g++) {
        float v1 = g < (int)curve1.size() ? curve1[g] : 0;
        float v2 = g < (int)curve2.size() ? curve2[g] : 0;
        std::cout << (g + 1) << "," << std::fixed << std::setprecision(4)
                  << v1 << "," << v2 << std::endl;
    }

    // Compare
    float peak1 = 0, peak2 = 0;
    for (auto f : curve1) if (f > peak1) peak1 = f;
    for (auto f : curve2) if (f > peak2) peak2 = f;

    float start2 = curve2.empty() ? 0 : curve2[0];
    float start1 = curve1.empty() ? 0 : curve1[0];
    float end1 = curve1.empty() ? 0 : curve1.back();
    float end2 = curve2.empty() ? 0 : curve2.back();

    std::cout << std::endl;
    std::cout << "=== Comparison ===" << std::endl;
    std::cout << "Random init:     start=" << start1 << " end=" << end1 << " peak=" << peak1 << std::endl;
    std::cout << "CRISPR-seeded:   start=" << start2 << " end=" << end2 << " peak=" << peak2 << std::endl;
    std::cout << "Improvement:     " << ((peak2 - peak1) / (peak1 > 0 ? peak1 : 1) * 100) << "%" << std::endl;

    std::cout << std::endl;
    std::cout << "Demo complete." << std::endl;

    return 0;
}
