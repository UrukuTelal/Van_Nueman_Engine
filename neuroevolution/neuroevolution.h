// neuroevolution.h - Header for neuroevolution system

#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// Number of pillars (from Entity.h)
#ifndef NUM_PILLARS
#define NUM_PILLARS 16
#endif

// Individual for neuroevolution
typedef struct Individual {
    float weights[NUM_PILLARS][NUM_PILLARS];  // Interaction matrix weights
    float fitness;
    int generation;
} Individual;

// Neuroevolution population handle
typedef struct NeuroevolutionPopulationImpl* NeuroevolutionPopulation;

// Create population
NeuroevolutionPopulation neuroevo_create(uint32_t pop_size, uint32_t server_id);

// Destroy population
void neuroevo_destroy(NeuroevolutionPopulation pop);

// Initialize with random weights
void neuroevo_init_random(NeuroevolutionPopulation pop);

// Evaluate fitness for all individuals
void neuroevo_evaluate_fitness(NeuroevolutionPopulation pop);

// Evolve one generation (selection + crossover + mutation)
void neuroevo_evolve(NeuroevolutionPopulation pop);

// Get best individual
Individual neuroevo_get_best(NeuroevolutionPopulation pop);

// Apply best individual's weights to interaction matrix
void neuroevo_apply_best(NeuroevolutionPopulation pop);

// Get population size
uint32_t neuroevo_get_size(NeuroevolutionPopulation pop);

// Get current generation
int neuroevo_get_generation(NeuroevolutionPopulation pop);

#ifdef __cplusplus
}
#endif
