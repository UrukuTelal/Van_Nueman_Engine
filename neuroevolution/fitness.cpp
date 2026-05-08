#include <vector>
#include <cmath>
#include "../include/Entity.h"
#include "neuroevolution.h"

// Fitness evaluation for neuroevolution
// From FULL_ARCHITECTURE.md: neuroevolution/fitness - Fitness evaluation

#ifndef FROM_SCALED
#define FROM_SCALED(x) (x)
#endif

struct InteractionOutcome {
    uint32_t source_server;
    uint32_t target_server;
    float force_applied;
    int pillar_affected;
    float outcome_magnitude;
    float timestamp;
};

class FitnessEvaluator {
public:
    FitnessEvaluator() : outcomes(), total_fitness(0.0f) {}

    void record_outcome(const InteractionOutcome& outcome) {
        outcomes.push_back(outcome);
        if (outcomes.size() > 1000) {
            outcomes.erase(outcomes.begin());
        }
    }

    float evaluate_individual(float weights[NUM_PILLARS][NUM_PILLARS]) {
        if (outcomes.empty()) return 0.0f;

        float fitness = 0.0f;
        for (const auto& outcome : outcomes) {
            float predicted = weights[PILLAR_FORCE][outcome.pillar_affected] * outcome.force_applied;
            float error = fabsf(predicted - outcome.outcome_magnitude);
            fitness -= error;
        }

        fitness /= (float)outcomes.size();
        return fitness;
    }

    float evaluate_server(uint32_t server_id, const PillarStateVector* server, float dt) {
        if (!server) return 0.0f;
        float fitness = 0.0f;

        float awareness = FROM_SCALED(server->pillars[PILLAR_AWARENESS]);
        fitness += awareness * 0.1f;

        float willpower = FROM_SCALED(server->pillars[PILLAR_WILLPOWER]);
        fitness += willpower * 0.1f;

        float harm = FROM_SCALED(server->pillars[PILLAR_HARM]);
        fitness -= harm * 0.2f;

        float integrity = FROM_SCALED(server->pillars[PILLAR_INTEGRITY]);
        fitness += integrity * 0.1f;

        return fitness;
    }

    void sync_to_db(void* db, uint32_t server_id) {
        if (!db) return;
    }

private:
    std::vector<InteractionOutcome> outcomes;
    float total_fitness;
};
