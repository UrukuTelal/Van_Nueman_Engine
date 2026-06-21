// fractal_spawner.h - Fractal AI agent scaling system
// Fractal growth: Gen 0=1 (Bob), Gen 1=2, Gen 2=4, Gen 3=8, etc.
// Each agent's PSV is mutated from parent, identity = hash(PSV)

#pragma once

#include <cstdint>
#include <cstddef>
#include "../include/Entity.h"
#include "../agents/cognition.h"

#ifndef MAX_FRACTAL_GENERATION
#define MAX_FRACTAL_GENERATION 7  // 2^7 = 128 agents max per branch
#endif

// Fractal Agent structure
struct FractalAgent {
    uint32_t id;
    uint32_t generation;       // Fractal generation (0=Bob, 1, 2, ...)
    uint32_t parent_id;
    PillarStateVector psv;    // 16D Pillar State Vector
    float mutation_rate;
    AgentCognition* cognition;
    bool active;
};

class FractalSpawner {
public:
    FractalSpawner();
    FractalSpawner(const FractalSpawner&) = delete;
    FractalSpawner& operator=(const FractalSpawner&) = delete;
    ~FractalSpawner();
    
    // Spawn fractal generation (recursive doubling)
    // Returns count of agents spawned (1 << generation)
    uint32_t spawn_generation(const PillarStateVector& parent_psv, 
                                   uint32_t generation,
                                   uint32_t parent_id = 0);
    
    // Mutate PSV from parent to child
    void mutate_psv(const PillarStateVector& parent, 
                     PillarStateVector& child, 
                     uint32_t generation, 
                     float base_mutation_rate = 0.05f);
    
    // Get all spawned agents
    const std::vector<FractalAgent>& get_agents() const { return agents_; }
    
    // Get agents by generation
    std::vector<FractalAgent*> get_generation(uint32_t gen) const;
    
    // Log fractal state to blackboard
    void log_to_blackboard(uint32_t gen, uint32_t agent_id, const PillarStateVector& psv) const;
    
    // Assign model based on generation (fractal scaling)
    static const char* get_model_for_generation(uint32_t gen);
    
private:
    std::vector<FractalAgent> agents_;
    uint32_t next_agent_id_;
    
    // Recursive spawn helper
    uint32_t recursive_spawn(const PillarStateVector& parent_psv,
                                 uint32_t generation,
                                 uint32_t parent_id,
                                 float mutation_decay);
};
