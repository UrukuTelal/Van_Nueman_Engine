// fractal_spawner.cpp - Fractal AI agent scaling implementation
// Exponential growth: 1 -> 2 -> 4 -> 8 -> 16 -> 32 -> 64 -> 128

#include "fractal_spawner.h"
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <cmath>

FractalSpawner::FractalSpawner() : next_agent_id_(100) {
    srand((unsigned int)time(nullptr));
}

FractalSpawner::~FractalSpawner() {
    for (auto& agent : agents_) {
        if (agent.cognition) {
            delete agent.cognition;
        }
    }
    agents_.clear();
}

uint32_t FractalSpawner::spawn_generation(const PillarStateVector& parent_psv,
                                        uint32_t generation,
                                        uint32_t parent_id) {
    if (generation > MAX_FRACTAL_GENERATION) {
        return 0;
    }
    
    uint32_t expected_count = 1u << generation;  // 2^gen
    printf("[FRACTAL] Spawning generation %u (%u agents)...\n", generation, expected_count);
    
    float mutation_decay = powf(0.95f, (float)generation);
    
    uint32_t spawned = recursive_spawn(parent_psv, generation, parent_id, mutation_decay);
    
    printf("[FRACTAL] Generation %u complete: %u agents spawned.\n", generation, spawned);
    return spawned;
}

uint32_t FractalSpawner::recursive_spawn(const PillarStateVector& parent_psv,
                                          uint32_t generation,
                                          uint32_t parent_id,
                                          float mutation_decay) {
    if (generation > MAX_FRACTAL_GENERATION) {
        return 0;
    }
    
    uint32_t count = 1u << generation;  // 2^gen agents
    
    for (uint32_t i = 0; i < count; i++) {
        FractalAgent agent;
        agent.id = next_agent_id_++;
        agent.generation = generation;
        agent.parent_id = parent_id;
        agent.active = true;
        agent.mutation_rate = 0.05f * mutation_decay;
        
        // Mutate PSV from parent
        mutate_psv(parent_psv, agent.psv, generation, agent.mutation_rate);
        
        // Create cognition (agents/cognition.cpp)
        agent.cognition = new AgentCognition(agent.id);
        // Set initial PSV in cognition
        auto& cog_psv = agent.cognition->get_pillars();
        for (int j = 0; j < NUM_PILLARS; j++) {
            cog_psv[j] = agent.psv.pillars[j];
        }
        
        // Log to blackboard (PillarAIColab)
        log_to_blackboard(generation, agent.id, agent.psv);
        
        agents_.push_back(agent);
    }
    
    return count;
}

void FractalSpawner::mutate_psv(const PillarStateVector& parent,
                                PillarStateVector& child,
                                uint32_t generation,
                                float base_mutation_rate) {
    for (int i = 0; i < NUM_PILLARS; i++) {
        float mutation = ((rand() % 100) - 50) / 100.0f * base_mutation_rate;
        float new_val = parent.pillars[i] + mutation;
        // Clamp to [0.0, 1.0]
        if (new_val < 0.0f) new_val = 0.0f;
        else if (new_val > 1.0f) new_val = 1.0f;
        child.pillars[i] = new_val;
    }
    
    // Fractal Depth pillar (14) = generation / 10.0f
    child.pillars[14] = (float)generation / 10.0f;
    
    // Compute entity ID from PSV (Entity.h)
    // Same PSV = same entity identity
}

std::vector<FractalAgent*> FractalSpawner::get_generation(uint32_t gen) const {
    std::vector<FractalAgent*> result;
    for (auto& agent : agents_) {
        if (agent.generation == gen) {
            result.push_back(const_cast<FractalAgent*>(&agent));
        }
    }
    return result;
}

void FractalSpawner::log_to_blackboard(uint32_t gen, uint32_t agent_id, 
                                          const PillarStateVector& psv) const {
    // Log format: timestamp, gen, agent_id, PSV
    // Append to fractal_log.txt (in production use PillarAIColab bridge)
    char cmd[512];
    snprintf(cmd, sizeof(cmd), 
             "echo \"%lu,gen%u,agent%u,PSV:%.2f,%.2f,...\" >> fractal_log.txt",
             (unsigned long)time(nullptr), gen, agent_id, 
             psv.pillars[0], psv.pillars[1]);
    system(cmd);
}

const char* FractalSpawner::get_model_for_generation(uint32_t gen) {
    // Fractal scaling: higher generations = more capable models
    switch (gen) {
        case 0: return "qwen3.6:latest";       // Bob (player)
        case 1: return "qwen2.5:0.5b";       // Scouts
        case 2: return "qwen2.5:1.5b";       // Gatherers
        case 3: return "qwen2.5:3b";         // Builders
        case 4: return "qwen2.5:7b";         // Defenders
        case 5: return "qwen3.5:latest";     // Traders
        case 6: return "cogito:8b";           // Governors
        case 7: return "gemma4:latest";       // Judges
        default: return "qwen2.5:0.5b";
    }
}
