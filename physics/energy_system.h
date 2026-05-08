#ifndef ENERGY_SYSTEM_H
#define ENERGY_SYSTEM_H

#include "../include/Entity.h"
#include "../scene/star_cluster.h"

struct RadiantFlux {
    float watts_per_m2;   // Energy flux at distance
    float temperature;     // Star temperature
};

// Compute radiant flux from a star at given distance (in AU)
RadiantFlux compute_radiant_flux(const Star& star, float distance_au);

// Compute heat transfer to entity surface
float compute_heat_transfer(float flux_wm2, float absorption, float surface_area);

// Apply thermodynamic updates to entity (called per tick)
void apply_thermodynamics(Entity* entity, float dt, const RadiantFlux& flux);

#endif // ENERGY_SYSTEM_H
