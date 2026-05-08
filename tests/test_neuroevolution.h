// test_neuroevolution.h - Unit tests for Neuroevolution
#pragma once

#include <cassert>
#include <cstdio>
#include "../neuroevolution/neuroevolution.h"
#include "../include/Entity.h"

inline void test_population_creation() {
    printf("Test: Population creation... ");
    NeuroevolutionPopulation pop = neuroevo_create(100, 1);  // 100 individuals, server_id=1
    assert(pop != nullptr);
    uint32_t size = neuroevo_get_size(pop);
    assert(size == 100);
    printf("PASS (size=%d)\n", size);
    neuroevo_destroy(pop);
}

inline void test_fitness_evaluation() {
    printf("Test: Fitness evaluation... ");
    NeuroevolutionPopulation pop = neuroevo_create(50, 1);
    neuroevo_init_random(pop);
    neuroevo_evaluate_fitness(pop);

    Individual best = neuroevo_get_best(pop);
    printf("PASS (fitness=%.4f)\n", best.fitness);
    neuroevo_destroy(pop);
}

inline void test_evolution_step() {
    printf("Test: Evolution step... ");
    NeuroevolutionPopulation pop = neuroevo_create(50, 1);
    neuroevo_init_random(pop);

    int gen_before = neuroevo_get_generation(pop);
    neuroevo_evolve(pop);
    int gen_after = neuroevo_get_generation(pop);

    assert(gen_after > gen_before);
    printf("PASS (gen: %d -> %d)\n", gen_before, gen_after);
    neuroevo_destroy(pop);
}

inline void test_apply_best_weights() {
    printf("Test: Apply best weights... ");
    NeuroevolutionPopulation pop = neuroevo_create(50, 1);
    neuroevo_init_random(pop);
    neuroevo_evaluate_fitness(pop);
    neuroevo_apply_best(pop);

    printf("PASS\n");
    neuroevo_destroy(pop);
}

inline void run_neuroevolution_tests() {
    printf("=== Neuroevolution Tests ===\n");
    test_population_creation();
    test_fitness_evaluation();
    test_evolution_step();
    test_apply_best_weights();
    printf("=== All Neuroevolution Tests PASSED ===\n\n");
}
