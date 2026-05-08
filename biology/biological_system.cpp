#include "biological_system.h"
#include <cstdlib>
#include <ctime>

void find_food(Entity* entity, BioState* bio) {
    if (entity->energy < 20.0f) {
        // Find food (light/heat absorption based on Attraction pillar)
        bio->food_level += 10.0f * entity->pillars[PILLAR_ATTRACTION];
        entity->energy += bio->food_level * 0.5f;
        bio->last_meal_time = 0.0f;
    }
}

void reproduce(Entity* entity, BioState* parent_bio, Entity* child) {
    if (entity->energy > 80.0f && entity->pillars[PILLAR_MEMORY] > 0.5f) {
        // Create child with mutated PSV
        child->id = (uint32_t)rand();
        for (int i = 0; i < NUM_PILLARS; i++) {
            float mutation = ((float)(rand() % 100) - 50.0f) / 100.0f;
            child->pillars[i] = entity->pillars[i] + mutation;
            if (child->pillars[i] < 0.0f) child->pillars[i] = 0.0f;
            if (child->pillars[i] > 1.0f) child->pillars[i] = 1.0f;
        }
        child->energy = entity->energy * 0.5f;
        entity->energy *= 0.5f;
        parent_bio->offspring_count++;
    }
}

void build_shelter(Entity* entity, BioState* bio) {
    // Shelter uses Integrity + Resistance for durability
    if (entity->pillars[PILLAR_INTEGRITY] > 0.5f) {
        bio->has_shelter = true;
    }
}

void update_biology(Entity* entity, float dt, BioState* bio) {
    bio->last_meal_time += dt;
    
    // Starvation
    if (bio->last_meal_time > 10.0f) {
        bio->food_level = 0.0f;
        entity->energy -= 5.0f * dt;
    }
    
    // Death conditions
    if (entity->pillars[PILLAR_HARM] > 0.9f || entity->energy < 0.1f) {
        entity->flags.alive = 0;
    }
}
