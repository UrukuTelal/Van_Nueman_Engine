#ifndef VAN_NUEMAN_STAR_CLUSTER_H
#define VAN_NUEMAN_STAR_CLUSTER_H

// star_cluster.h - Procedural star cluster with habitable planets and emergent life
// Uses PillarAIColab for all AI decisions

#include <cstdint>
#include <cstddef>
#include <vector>
#include <random>
#include "../include/Entity.h"
// Forward declarations to avoid pulling in problematic SkellyTypes.h
struct BoneSegment;
struct MuscleGroup;
struct Organ;
#include "../vn/PillarTypes.h"

// Star types
enum StarType : uint32_t {
    STAR_TYPE_M = 0,  // Red dwarf (70% of stars)
    STAR_TYPE_G = 1,  // Sun-like (7.5%)
    STAR_TYPE_A = 2,  // White (1%)
    STAR_TYPE_F = 3,  // Yellow-white (2%)
    STAR_TYPE_K = 4   // Orange (12%)
};

// Planet climate zones
enum ClimateZone : uint8_t {
    ZONE_ARCTIC = 0,
    ZONE_BOREAL = 1,
    ZONE_TEMPERATE = 2,
    ZONE_SUBTROPICAL = 3,
    ZONE_TROPICAL = 4,
    ZONE_ARID = 5
};

// Procedural Star
struct Star {
    uint32_t id;
    float x, y, z;          // Position in cluster (parsecs)
    StarType type;
    float mass;               // Solar masses
    float luminosity;          // Relative to Sun
    float radius;              // Solar radii
    float temperature;         // Kelvin
    uint32_t seed;             // Procedural seed
    std::vector<uint32_t> planet_ids;
};

// Procedural Planet
struct Planet {
    uint32_t id;
    uint32_t star_id;
    float orbital_distance;     // AU
    float orbital_period;       // Earth years
    float radius;              // Earth radii
    float gravity;             // m/s²
    float surface_temp;         // Kelvin
    float atmosphere_density;  // atm
    float water_coverage;       // 0-1
    uint32_t seed;             // Procedural seed
    bool habitable;            // Can support life

    // Pillar state for planetary conditions
    PillarStateVector pillar_state;

    // Lifeform population on this planet
    uint32_t lifeform_count;
    float biomass;             // Total biomass
};

// Emergent Lifeform - learns physics to survive
struct Lifeform {
    uint32_t id;
    uint32_t planet_id;
    float x, y, z;          // Position on planet surface

    // Core survival pillars (MUST learn to balance)
    PillarStateVector pillars;

    // Physical body (Skelly structure)
    uint32_t skelly_instance_id;

    // Learning state
    float energy;
    float age;
    float fitness;
    uint32_t generation;

    // Physics interaction memory
    struct PhysicsMemory {
        float force_applied[3];    // Last force vector
        float pressure_felt;        // Environmental pressure
        float temperature;           // Local temperature
        float resource_found;       // Resource value at last location
        uint32_t ticks_survived;
    } last_interaction;

    bool alive;
};

// Star Cluster simulation
class StarCluster {
public:
    StarCluster(uint32_t seed = 0);

    // Procedural generation
    void generate_cluster(uint32_t num_stars);
    void generate_star_system(Star& star);
    void generate_planets(Star& star);
    void spawn_lifeforms(Planet& planet, uint32_t count);

    // Simulation tick (30 Hz)
    void tick(float delta_time);

    // Lifeform AI - uses PillarAIColab for decisions
    void update_lifeform(Lifeform& lf, float delta_time);
    void lifeform_perception(Lifeform& lf);
    void lifeform_decision(Lifeform& lf);
    void lifeform_act(Lifeform& lf, float delta_time);

    // Physics interaction - lifeforms learn to use physics
    void apply_physics_to_lifeform(Lifeform& lf, float delta_time);
    void lifeform_manipulate_physics(Lifeform& lf, const float force[3]);

    // Get state for rendering/network
    const std::vector<Star>& get_stars() const { return stars_; }
    const std::vector<Planet>& get_planets() const { return planets_; }
    const std::vector<Lifeform>& get_lifeforms() const { return lifeforms_; }

    // Neuroevolution for lifeform learning
    void evolve_lifeforms();

private:
    uint32_t seed_;
    std::mt19937 rng_;

    std::vector<Star> stars_;
    std::vector<Planet> planets_;
    std::vector<Lifeform> lifeforms_;

    uint32_t next_star_id_;
    uint32_t next_planet_id_;
    uint32_t next_lifeform_id_;

    // Tick counters
    uint32_t tick_count_;
    uint32_t evolution_timer_;
};

// Utility: Procedural generation helpers
namespace procedural {

    // Generate star properties based on type
    void generate_star_properties(Star& star);

    // Generate planetary conditions
    void generate_planet_conditions(Planet& planet, const Star& parent, std::mt19937& rng);

    // Calculate habitable zone
    bool is_in_habitable_zone(float orbital_distance, const Star& star);

    // Generate initial lifeform pillars (random, will evolve)
    void randomize_lifeform_pillars(Lifeform& lf, std::mt19937& rng);

    // Calculate environmental pressure on lifeform
    float calculate_pressure(const Lifeform& lf, const Planet& planet);

} // namespace procedural

#endif // VAN_NUEMAN_STAR_CLUSTER_H
