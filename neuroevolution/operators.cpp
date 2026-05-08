#include <vector>
#include <algorithm>
#include <random>
#include "../include/Entity.h"
#include "neuroevolution.h"

// Mutation/crossover operators for neuroevolution
// From FULL_ARCHITECTURE.md: neuroevolution/operators - Mutation/crossover operators

#ifndef FROM_SCALED
#define FROM_SCALED(x) (x)
#define TO_SCALED(x) (x)
#endif

struct OperatorBoundary {
    int pillar;
    float min_val;
    float max_val;
};

class NeuroevolutionOperators {
public:
    NeuroevolutionOperators(uint32_t server_id) : server_id(server_id) {}

    void set_boundary(int pillar, float min_val, float max_val) {
        // Find or create boundary
        for (auto& b : boundaries) {
            if (b.pillar == pillar) {
                b.min_val = min_val;
                b.max_val = max_val;
                return;
            }
        }
        boundaries.push_back({pillar, min_val, max_val});
    }

    void apply_boundaries(float weights[NUM_PILLARS][NUM_PILLARS]) {
        for (int i = 0; i < NUM_PILLARS; i++) {
            // Check if pillar i has a boundary
            for (const auto& b : boundaries) {
                if (b.pillar == i) {
                    for (int j = 0; j < NUM_PILLARS; j++) {
                        weights[i][j] = std::max(b.min_val, std::min(b.max_val, weights[i][j]));
                    }
                    break;
                }
            }
        }
    }

    void mutate(float weights[NUM_PILLARS][NUM_PILLARS], float mutation_rate = 0.1f) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-0.5f, 0.5f);
        std::uniform_real_distribution<float> prob(0.0f, 1.0f);

        for (int i = 0; i < NUM_PILLARS; i++) {
            for (int j = 0; j < NUM_PILLARS; j++) {
                if (prob(gen) < mutation_rate) {
                    weights[i][j] += dis(gen);
                }
            }
        }
        apply_boundaries(weights);
    }

    void crossover(const float parent1[NUM_PILLARS][NUM_PILLARS],
                   const float parent2[NUM_PILLARS][NUM_PILLARS],
                   float child1[NUM_PILLARS][NUM_PILLARS],
                   float child2[NUM_PILLARS][NUM_PILLARS]) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, NUM_PILLARS - 1);

        int cp = dis(gen);  // Crossover point

        for (int i = 0; i < NUM_PILLARS; i++) {
            for (int j = 0; j < NUM_PILLARS; j++) {
                if (i < cp) {
                    child1[i][j] = parent1[i][j];
                    child2[i][j] = parent2[i][j];
                } else {
                    child1[i][j] = parent2[i][j];
                    child2[i][j] = parent1[i][j];
                }
            }
        }
    }

    void save_boundaries(void* db) {
        if (!db) return;
        // Would save to database
    }

private:
    uint32_t server_id;
    std::vector<OperatorBoundary> boundaries;
};
