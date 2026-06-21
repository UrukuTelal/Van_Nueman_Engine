#ifndef ENTITY_NEURAL_NET_H
#define ENTITY_NEURAL_NET_H

#include "../include/Entity.h"
#include <cstdint>
#include <vector>

// Actions an entity can take (matches neural_agent.py)
enum EntityAction : uint32_t {
    ACTION_MOVE = 0,
    ACTION_FIND_FOOD,
    ACTION_REPRODUCE,
    ACTION_BUILD_SHELTER,
    ACTION_EAT,
    ACTION_SLEEP,
    ACTION_SOCIALIZE,
    ACTION_IDLE,
    ACTION_COUNT = 8
};

// Simplified Q-table for CPU fallback (GPU uses compute shader)
struct EntityQTable {
    float q_values[ACTION_COUNT];
    float epsilon;
    uint32_t training_steps;
};

// GPU Neural Network for entities (Vulkan compute)
class EntityNeuralNet {
public:
    EntityNeuralNet();
    ~EntityNeuralNet();
    
    // Initialize with Vulkan context
    bool init(class VulkanRenderer* renderer, uint32_t max_entities = 500000);
    
    // Upload entity pillar states to GPU
    void upload_pillar_states(const float* pillar_data, uint32_t num_entities);
    
    // Run inference on GPU (all entities in parallel)
    void compute_actions(uint32_t num_entities, float epsilon);
    
    // Download Q-values from GPU
    void download_q_values(float* q_data, uint32_t num_entities);
    
    // Select action for single entity (CPU fallback)
    static uint32_t select_action_cpu(const float pillars[NumPillars], 
                                      EntityQTable& qtable, 
                                      bool explore = true);
    
    // Observe and learn (simplified Q-learning update)
    static void observe_cpu(EntityQTable& qtable,
                               const float old_pillars[NumPillars],
                               uint32_t action,
                               float reward,
                               const float new_pillars[NumPillars]);

    static float compute_pillar_reward(const float pillars[NumPillars]);
    
private:
    class Impl;  // PIML pattern for Vulkan internals
    Impl* impl_;
};

#endif // ENTITY_NEURAL_NET_H
