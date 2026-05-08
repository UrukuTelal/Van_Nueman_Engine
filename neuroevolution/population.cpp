#include <vector>
#include <algorithm>
#include <random>
#include "../include/Entity.h"
#include "neuroevolution.h"

// Neuroevolution population for server PSV (Pillar State Vector)
// From FULL_ARCHITECTURE.md: neuroevolution/population - Server PSV population

struct NeuroevolutionPopulationImpl {
    uint32_t population_size;
    uint32_t server_id;
    int current_gen;
    std::vector<Individual> individuals;

    NeuroevolutionPopulationImpl(uint32_t pop_size, uint32_t srv_id)
        : population_size(pop_size), server_id(srv_id), current_gen(0) {
        individuals.resize(pop_size);
        initialize_random();
    }

    void initialize_random() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

        for (auto& ind : individuals) {
            for (int i = 0; i < NUM_PILLARS; i++) {
                for (int j = 0; j < NUM_PILLARS; j++) {
                    ind.weights[i][j] = dis(gen);
                }
            }
            ind.fitness = 0.0f;
            ind.generation = 0;
        }
    }

    void evaluate_fitness() {
        for (auto& ind : individuals) {
            float sum = 0.0f;
            for (int i = 0; i < NUM_PILLARS; i++) {
                for (int j = 0; j < NUM_PILLARS; j++) {
                    sum += fabsf(ind.weights[i][j]);
                }
            }
            ind.fitness = 1.0f / (1.0f + sum * 0.1f);
            ind.generation = current_gen;
        }
    }

    void evolve() {
        std::vector<Individual> new_pop = tournament_selection();
        crossover_mutation(new_pop);
        individuals = new_pop;
        current_gen++;
    }

    Individual get_best() {
        return *std::max_element(individuals.begin(), individuals.end(),
            [](const Individual& a, const Individual& b) { return a.fitness < b.fitness; });
    }

    void apply_to_interaction_matrix(const Individual& ind) {
        for (int i = 0; i < NUM_PILLARS; i++) {
            for (int j = 0; j < NUM_PILLARS; j++) {
                // Would copy to device memory
            }
        }
    }

private:
    std::vector<Individual> tournament_selection() {
        std::vector<Individual> selected;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, population_size - 1);

        for (uint32_t i = 0; i < population_size; i++) {
            Individual& a = individuals[dis(gen)];
            Individual& b = individuals[dis(gen)];
            selected.push_back(a.fitness >= b.fitness ? a : b);
        }
        return selected;
    }

    void crossover_mutation(std::vector<Individual>& pop) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-0.1f, 0.1f);
        std::uniform_int_distribution<> pillar_dis(0, NUM_PILLARS - 1);

        for (uint32_t i = 0; i < population_size; i += 2) {
            if (i + 1 >= population_size) break;
            int cp = pillar_dis(gen);
            for (int p = cp; p < NUM_PILLARS; p++) {
                for (int q = 0; q < NUM_PILLARS; q++) {
                    float temp = pop[i].weights[p][q];
                    pop[i].weights[p][q] = pop[i+1].weights[p][q];
                    pop[i+1].weights[p][q] = temp;
                }
            }
            for (int j = 0; j < 5; j++) {
                int pi = pillar_dis(gen);
                int pj = pillar_dis(gen);
                pop[i].weights[pi][pj] += dis(gen);
                pop[i+1].weights[pi][pj] += dis(gen);
            }
        }
    }
};

// C API implementations
NeuroevolutionPopulation neuroevo_create(uint32_t pop_size, uint32_t server_id) {
    return new NeuroevolutionPopulationImpl(pop_size, server_id);
}

void neuroevo_destroy(NeuroevolutionPopulation pop) {
    delete static_cast<NeuroevolutionPopulationImpl*>(pop);
}

void neuroevo_init_random(NeuroevolutionPopulation pop) {
    auto* impl = static_cast<NeuroevolutionPopulationImpl*>(pop);
    if (impl) impl->initialize_random();
}

void neuroevo_evaluate_fitness(NeuroevolutionPopulation pop) {
    auto* impl = static_cast<NeuroevolutionPopulationImpl*>(pop);
    if (impl) impl->evaluate_fitness();
}

void neuroevo_evolve(NeuroevolutionPopulation pop) {
    auto* impl = static_cast<NeuroevolutionPopulationImpl*>(pop);
    if (impl) impl->evolve();
}

Individual neuroevo_get_best(NeuroevolutionPopulation pop) {
    auto* impl = static_cast<NeuroevolutionPopulationImpl*>(pop);
    if (impl) return impl->get_best();
    Individual empty{};
    return empty;
}

void neuroevo_apply_best(NeuroevolutionPopulation pop) {
    auto* impl = static_cast<NeuroevolutionPopulationImpl*>(pop);
    if (impl) impl->apply_to_interaction_matrix(impl->get_best());
}

uint32_t neuroevo_get_size(NeuroevolutionPopulation pop) {
    auto* impl = static_cast<NeuroevolutionPopulationImpl*>(pop);
    return impl ? impl->population_size : 0;
}

int neuroevo_get_generation(NeuroevolutionPopulation pop) {
    auto* impl = static_cast<NeuroevolutionPopulationImpl*>(pop);
    return impl ? impl->current_gen : 0;
}
