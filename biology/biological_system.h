#ifndef BIOLOGICAL_SYSTEM_H
#define BIOLOGICAL_SYSTEM_H

#include "../include/Entity.h"
#include "organism_types.h"

using Van_Nueman::OrganismBase;

enum BioAction {
    FIND_FOOD,
    REPRODUCE,
    BUILD_SHELTER,
    EAT,
    REST,
    SELECT_BASE  // Player must choose Carbon/Silicon/Hybrid
};

struct BioState {
    float food_level;       // 0.0 - 100.0
    float last_meal_time;   // Time since last meal
    uint32_t offspring_count; // Total offspring created
    bool has_shelter;      // Currently in shelter
    OrganismBase organism_base;  // Carbon/Silicon/Hybrid - player choice
};

void update_biology(Entity* entity, float dt, BioState* bio);
void find_food(Entity* entity, BioState* bio);
void reproduce(Entity* entity, BioState* parent_bio, Entity* child);
void build_shelter(Entity* entity, BioState* bio);

#endif // BIOLOGICAL_SYSTEM_H
