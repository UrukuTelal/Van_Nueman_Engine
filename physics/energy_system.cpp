#include "energy_system.h"
#include <cmath>
#include <algorithm>

RadiantFlux compute_radiant_flux(const Star& star, float distance_au) {
    // Convert AU to meters and clamp to minimum distance to prevent division by zero
    float r_m = distance_au * 149.6e9f;
    r_m = std::max(r_m, 1e5f); // Minimum 100 km for numerical stability
    
    // Use double precision for intermediate calculation to prevent overflow
    double r_sq = static_cast<double>(r_m) * static_cast<double>(r_m);
    double numerator = static_cast<double>(star.luminosity) * 3.828e26;
    double denominator = 4.0 * 3.141592653589793 * r_sq;
    double flux_d = numerator / denominator;
    
    // Clamp flux to reasonable range to prevent extreme values
    float flux = static_cast<float>(std::clamp(flux_d, 0.0, 1e20));
    
    RadiantFlux result;
    result.watts_per_m2 = flux;
    result.temperature = star.temperature;
    return result;
}

float compute_heat_transfer(float flux_wm2, float absorption, float surface_area) {
    return flux_wm2 * absorption * surface_area;
}

void apply_thermodynamics(Entity* entity, float dt, const RadiantFlux& flux) {
    // Metabolic cost (energy out)
    float metabolic_cost = 0.1f * entity->pillars[PILLAR_FLUX] * dt;
    entity->energy -= metabolic_cost;
    
    // Energy in from starlight (absorbed)
    float absorption = entity->pillars[PILLAR_DEPTH];
    float surface_area = 1.0f;  // Simplified: 1 m² surface area
    float heat_in = flux.watts_per_m2 * absorption * surface_area;
    
    // Convert watts to "energy units" (simplified scaling)
    entity->energy += heat_in * dt * 0.001f;
    
    // Clamp energy to reasonable range
    if (entity->energy < 0.0f) entity->energy = 0.0f;
    if (entity->energy > 100.0f) entity->energy = 100.0f;
}
