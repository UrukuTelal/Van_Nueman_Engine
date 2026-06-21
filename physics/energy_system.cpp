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
    // Both heat input and metabolic cost independently rotate Warmth on the Bloch sphere.
    // Heat_in → rotate toward north pole (theta=0, value=1.0).
    // Metabolic_cost → rotate toward south pole (theta=pi, value=0.0).
    // The composition of two small rotations correctly models simultaneous heating and cooling
    // without scalar cancellation.
    const float PI = 3.14159265f;
    
    // Energy in from starlight (absorbed via Depth)
    float absorption = entity->pillars[Depth];
    float surface_area = 1.0f;
    float heat_in = flux.watts_per_m2 * absorption * surface_area * dt * 1e-4f;
    
    // Metabolic cost (from Flux pillar — cost of living)
    float flux_f = entity->pillars[Flux];
    float metabolic_cost = flux_f * 0.1f * dt;
    
    vn::fp20_t warmth = vn::fp20_t(entity->pillars[Warmth]);
    
    // Heat rotation: toward north pole (theta → 0)
    float warmth_theta = pillar_value_to_theta(warmth).to_float();
    float heat_rotation = -warmth_theta * heat_in;
    warmth = apply_bloch_rotation(warmth, vn::fp20_t(heat_rotation));
    
    // Metabolic rotation: toward south pole (theta → pi) from current theta
    warmth_theta = pillar_value_to_theta(warmth).to_float();
    float meta_rotation = (PI - warmth_theta) * metabolic_cost;
    warmth = apply_bloch_rotation(warmth, vn::fp20_t(meta_rotation));
    
    // Clamp to valid range
    if (warmth < vn::fp20_t(0.0f)) warmth = vn::fp20_t(0.0f);
    if (vn::fp20_t(1.0f) < warmth) warmth = vn::fp20_t(1.0f);
    entity->pillars[Warmth] = warmth;
    
    // Extreme warmth drifts Integrity (structural health)
    // (warmth toward equator = healthiest, poles = extreme states)
    float center_distance = std::fabs(warmth.to_float() - 0.5f) * 2.0f;
    float integrity_loss = center_distance * dt * 0.001f;
    entity->pillars[Integrity] = std::max(0.0f, entity->pillars[Integrity] - integrity_loss);
}
