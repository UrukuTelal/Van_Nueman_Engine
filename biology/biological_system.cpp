#include "biological_system.h"
#include "../scale/SemanticProjection.h"
#include <cstdlib>
#include <ctime>

static PillarStateVector ent_to_psv(const Entity* e) {
    PillarStateVector psv;
    for (int i = 0; i < NumPillars; i++) psv.pillars[i] = e->pillars[i];
    return psv;
}

void find_food(Entity* entity, BioState* bio) {
    if (entity->constraint_level > vn::fp20_t(0.8f)) {
        BiologicalProjection bp = BiologicalProjection::project(ent_to_psv(entity));
        bio->food_level += 10.0f * bp.resource_affinity;
        entity->constraint_level = entity->constraint_level - vn::fp20_t(bio->food_level * 0.005f);
        if (entity->constraint_level < vn::fp20_t()) entity->constraint_level = vn::fp20_t();
        bio->last_meal_time = 0.0f;
    }
}

void reproduce(Entity* entity, BioState* parent_bio, Entity* child) {
    if (entity->constraint_level < vn::fp20_t(0.2f)) {
        CognitiveProjection cp = CognitiveProjection::project(ent_to_psv(entity));
        if (cp.experiential_memory > 0.5f) {
            child->id = (uint32_t)rand();
            for (int i = 0; i < NumPillars; i++) {
                float mutation = ((float)(rand() % 100) - 50.0f) / 100.0f;
                child->pillars[i] = entity->pillars[i] + mutation;
                if (child->pillars[i] < 0.0f) child->pillars[i] = 0.0f;
                if (child->pillars[i] > 1.0f) child->pillars[i] = 1.0f;
            }
            vn::fp20_t orig_cl = entity->constraint_level;
            child->constraint_level = (orig_cl + vn::fp20_t(1.0f)) * vn::fp20_t(0.5f);
            entity->constraint_level = (orig_cl + vn::fp20_t(1.0f)) * vn::fp20_t(0.5f);
            parent_bio->offspring_count++;
        }
    }
}

void build_shelter(Entity* entity, BioState* bio) {
    BiologicalProjection bp = BiologicalProjection::project(ent_to_psv(entity));
    if (bp.membrane_integrity > 0.5f) {
        bio->has_shelter = true;
    }
}

void update_biology(Entity* entity, float dt, BioState* bio) {
    bio->last_meal_time += dt;
    
    if (bio->last_meal_time > 10.0f) {
        bio->food_level = 0.0f;
        entity->constraint_level = entity->constraint_level + vn::fp20_t(0.05f * dt);
    }
    
    BiologicalProjection bp = BiologicalProjection::project(ent_to_psv(entity));
    if (bp.stress_level > 0.9f || entity->constraint_level > vn::fp20_t(0.999f)) {
        entity->flags.constrained = 1;
    }
}
