// star_cluster.cpp - Procedural star cluster with emergent lifeforms
#include "star_cluster.h"
#include <cmath>
#include <algorithm>
#include <ctime>

using namespace procedural;

// Initialize cluster
StarCluster::StarCluster(uint32_t seed) :
    seed_(seed ? seed : static_cast<uint32_t>(time(nullptr))),
    rng_(seed_),
    next_star_id_(1000),
    next_planet_id_(2000),
    next_lifeform_id_(10000),
    tick_count_(0),
    evolution_timer_(0) {

    // Initialize RNG properly
    rng_.seed(seed_);
}

void StarCluster::generate_cluster(uint32_t num_stars) {
    stars_.reserve(num_stars);

    std::uniform_real_distribution<float> dist_pos(-100.0f, 100.0f);  // 200 parsec cube
    std::discrete_distribution<int> star_type_dist{70, 7, 1, 2, 12};  // Real stellar distribution

    for (uint32_t i = 0; i < num_stars; i++) {
        Star star;
        star.id = next_star_id_++;
        star.x = dist_pos(rng_);
        star.y = dist_pos(rng_);
        star.z = dist_pos(rng_);
        star.type = static_cast<StarType>(star_type_dist(rng_));
        star.seed = rng_();

        generate_star_properties(star);
        generate_star_system(star);

        stars_.push_back(star);
    }
}

void procedural::generate_star_properties(Star& star) {
    // Mass, luminosity, radius based on type (simplified Harvard spectral)
    switch (star.type) {
        case STAR_TYPE_M:  // Red dwarf
            star.mass = 0.3f + (rand() % 100) / 200.0f;  // 0.3-0.8 solar masses
            star.luminosity = std::pow(star.mass, 3.5f);
            star.radius = 0.4f + (rand() % 30) / 100.0f;
            star.temperature = 3000.0f + (rand() % 2000);
            break;
        case STAR_TYPE_G:  // Sun-like
            star.mass = 0.8f + (rand() % 40) / 100.0f;  // 0.8-1.2
            star.luminosity = 1.0f;
            star.radius = 1.0f;
            star.temperature = 5778.0f;
            break;
        case STAR_TYPE_A:  // White
            star.mass = 1.4f + (rand() % 100) / 100.0f;  // 1.4-2.4
            star.luminosity = std::pow(star.mass, 3.5f);
            star.radius = 1.5f + (rand() % 50) / 100.0f;
            star.temperature = 7500.0f + (rand() % 5000);
            break;
        case STAR_TYPE_F:
            star.mass = 1.0f + (rand() % 40) / 100.0f;
            star.luminosity = std::pow(star.mass, 3.5f);
            star.radius = 1.1f + (rand() % 30) / 100.0f;
            star.temperature = 6000.0f + (rand() % 1500);
            break;
        case STAR_TYPE_K:  // Orange
            star.mass = 0.6f + (rand() % 30) / 100.0f;  // 0.6-0.9
            star.luminosity = std::pow(star.mass, 3.5f);
            star.radius = 0.7f + (rand() % 30) / 100.0f;
            star.temperature = 4000.0f + (rand() % 1500);
            break;
    }
}

void StarCluster::generate_star_system(Star& star) {
    std::uniform_int_distribution<int> num_planets_dist(2, 8);
    int num_planets = num_planets_dist(rng_);

    generate_planets(star);
}

void StarCluster::generate_planets(Star& star) {
    std::uniform_real_distribution<float> dist_orb(0.1f, 50.0f);  // AU
    std::uniform_real_distribution<float> dist_radius(0.3f, 5.0f);  // Earth radii

    // Generate 2-8 planets per star
    std::uniform_int_distribution<int> num_planets_dist(2, 8);
    int num_planets = num_planets_dist(rng_);

    for (int i = 0; i < num_planets; i++) {
        Planet planet;
        planet.id = next_planet_id_++;
        planet.star_id = star.id;
        planet.orbital_distance = 0.1f + (i * 0.8f) + dist_orb(rng_) * 0.1f;
        planet.orbital_period = std::sqrt(std::pow(planet.orbital_distance, 3) / star.mass);
        planet.radius = dist_radius(rng_);
        planet.seed = rng_();

        // Check if habitable (will be determined after generating conditions)
        planet.lifeform_count = 0;
        planet.biomass = 0.0f;

        // Initialize pillar state for planetary conditions (will be updated in generate_planet_conditions)
        planet.pillar_state = create_default_pillar_state(0.3f);

        // Generate planetary conditions with RNG
        procedural::generate_planet_conditions(planet, star, rng_);

        // Check habitable after conditions are generated
        planet.habitable = procedural::is_in_habitable_zone(planet.orbital_distance, star);
        planet.pillar_state[PILLAR_WARMTH] = planet.surface_temp > 200.0f && planet.surface_temp < 350.0f ? 0.7f : 0.2f;
        planet.pillar_state[PILLAR_RESISTANCE] = planet.atmosphere_density > 0.5f ? 0.8f : 0.4f;
        planet.pillar_state[PILLAR_ATTRACTION] = planet.habitable ? 0.9f : 0.3f;

        planets_.push_back(planet);
        star.planet_ids.push_back(planet.id);

        // Spawn life if habitable
        if (planet.habitable) {
            spawn_lifeforms(planet, 10 + (rng_() % 40));  // 10-50 initial lifeforms
        }
    }
}

void procedural::generate_planet_conditions(Planet& planet, const Star& parent, std::mt19937& rng) {
    // Surface temperature based on stellar luminosity and distance
    float luminosity = parent.luminosity;
    float distance = planet.orbital_distance;
    planet.surface_temp = 278.0f * std::pow(luminosity / (distance * distance), 0.25f);

    // Atmosphere (random for now, could be more complex)
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    planet.atmosphere_density = dist(rng);

    // Gravity
    planet.gravity = 9.8f * planet.radius;  // Simplified

    // Water coverage
    planet.water_coverage = dist(rng);
}

bool procedural::is_in_habitable_zone(float orbital_distance, const Star& star) {
    // Simplified habitable zone calculation
    // For Sun-like: 0.95-1.37 AU
    float inner, outer;
    switch (star.type) {
        case STAR_TYPE_M:
            inner = 0.1f; outer = 0.3f;
            break;
        case STAR_TYPE_G:
            inner = 0.95f; outer = 1.37f;
            break;
        case STAR_TYPE_K:
            inner = 0.5f; outer = 0.9f;
            break;
        default:
            inner = 0.3f; outer = 2.0f;
    }
    return orbital_distance >= inner && orbital_distance <= outer;
}

void StarCluster::spawn_lifeforms(Planet& planet, uint32_t count) {
    std::uniform_real_distribution<float> dist_pos(0.0f, 100.0f);
    std::uniform_real_distribution<float> dist_energy(50.0f, 150.0f);

    for (uint32_t i = 0; i < count; i++) {
        Lifeform lf;
        lf.id = next_lifeform_id_++;
        lf.planet_id = planet.id;
        lf.x = dist_pos(rng_);
        lf.y = dist_pos(rng_);
        lf.z = dist_pos(rng_);

        // Initialize random pillar state (will LEARN through evolution)
        procedural::randomize_lifeform_pillars(lf, rng_);

        lf.skelly_instance_id = 0;  // Will be assigned when physics initializes
        lf.energy = dist_energy(rng_);
        lf.age = 0.0f;
        lf.fitness = 0.0f;
        lf.generation = 0;
        lf.alive = true;

        // Initialize interaction memory
        lf.last_interaction = {0};
        lf.last_interaction.ticks_survived = 0;

        lifeforms_.push_back(lf);
        planet.lifeform_count++;
    }
}

void procedural::randomize_lifeform_pillars(Lifeform& lf, std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(0.1f, 0.9f);

    // Start with random-ish state (not hardcoded!)
    // They will LEARN optimal pillar configurations
    lf.pillars[PILLAR_AWARENESS] = dist(rng);
    lf.pillars[PILLAR_WILLPOWER] = dist(rng);
    lf.pillars[PILLAR_FORCE] = dist(rng);
    lf.pillars[PILLAR_INFLUENCE] = dist(rng);
    lf.pillars[PILLAR_RESISTANCE] = dist(rng);
    lf.pillars[PILLAR_INTEGRITY] = dist(rng);
    lf.pillars[PILLAR_COHESION] = dist(rng);
    lf.pillars[PILLAR_RELATION] = dist(rng);
    lf.pillars[PILLAR_PRESENCE] = dist(rng);
    lf.pillars[PILLAR_WARMTH] = dist(rng);
    lf.pillars[PILLAR_MEMORY] = dist(rng);
    lf.pillars[PILLAR_ATTRACTION] = dist(rng);
    lf.pillars[PILLAR_HARM] = 0.1f;  // Start low harm
    lf.pillars[PILLAR_DISTORTION] = 0.05f;  // Start low distortion
    lf.pillars[PILLAR_FLUX] = dist(rng);
    lf.pillars[PILLAR_DEPTH] = dist(rng);
}

void StarCluster::tick(float delta_time) {
    tick_count_++;
    evolution_timer_++;

    // Update all lifeforms
    for (auto& lf : lifeforms_) {
        if (lf.alive) {
            update_lifeform(lf, delta_time);
        }
    }

    // Neuroevolution step (every 10 seconds of simulation time)
    if (evolution_timer_ >= 300) {  // 300 ticks * 33ms ≈ 10s
        evolve_lifeforms();
        evolution_timer_ = 0;
    }

    // Update planets (environmental changes)
    for (auto& planet : planets_) {
        if (planet.habitable && planet.lifeform_count > 0) {
            // Biomass grows with surviving lifeforms
            planet.biomass = planet.lifeform_count * 10.0f;
        }
    }
}

void StarCluster::update_lifeform(Lifeform& lf, float delta_time) {
    // 1. Perception (gather environmental data)
    lifeform_perception(lf);

    // 2. Decision (use PillarAI for emergent behavior)
    lifeform_decision(lf);

    // 3. Action (apply to physics)
    lifeform_act(lf, delta_time);

    // 4. Physics interaction (lifeforms learn to use physics)
    apply_physics_to_lifeform(lf, delta_time);

    // 5. Survival check
    lf.energy -= delta_time * 0.5f;  // Energy cost
    lf.age += delta_time;
    lf.last_interaction.ticks_survived++;

    if (lf.energy <= 0.0f || lf.pillars[PILLAR_HARM] > 0.7f) {
        lf.alive = false;
        // Find planet and decrement count
        for (auto& planet : planets_) {
            if (planet.id == lf.planet_id) {
                planet.lifeform_count--;
                break;
            }
        }
    } else {
        // Survived - small energy bonus
        lf.fitness += delta_time * (1.0f - lf.pillars[PILLAR_HARM]);
    }
}

void StarCluster::lifeform_perception(Lifeform& lf) {
    // Get local environmental conditions
    Planet* planet = nullptr;
    for (auto& p : planets_) {
        if (p.id == lf.planet_id) {
            planet = &p;
            break;
        }
    }
    if (!planet) return;

    // Update awareness based on environmental perception
    float env_pressure = procedural::calculate_pressure(lf, *planet);

    // Awareness increases with successful perception
    lf.pillars[PILLAR_AWARENESS] = std::min(1.0f,
        lf.pillars[PILLAR_AWARENESS] + 0.001f * (1.0f - lf.pillars[PILLAR_DISTORTION]));

    // Store in memory
    lf.last_interaction.pressure_felt = env_pressure;
    lf.last_interaction.temperature = planet->surface_temp;
}

void StarCluster::lifeform_decision(Lifeform& lf) {
    // EMERGENT behavior - no hardcoded rules!
    // Pillar state vector determines behavior through interactions

    // Decision based on current pillar state and environmental pressure
    float harm_threshold = 0.7f;
    float current_harm = lf.pillars[PILLAR_HARM];

    // Emergent: High harm -> increase resistance (learning!)
    if (current_harm > 0.3f) {
        lf.pillars[PILLAR_RESISTANCE] = std::min(1.0f, lf.pillars[PILLAR_RESISTANCE] + 0.01f);
        lf.pillars[PILLAR_WILLPOWER] = std::min(1.0f, lf.pillars[PILLAR_WILLPOWER] + 0.02f);
    }

    // Emergent: Low energy -> seek warmth (survival instinct)
    if (lf.energy < 30.0f) {
        lf.pillars[PILLAR_ATTRACTION] = std::min(1.0f, lf.pillars[PILLAR_ATTRACTION] + 0.05f);
        lf.pillars[PILLAR_WARMTH] = std::min(1.0f, lf.pillars[PILLAR_WARMTH] + 0.03f);
    }

    // Emergent: Use Force to manipulate environment (physics learning!)
    if (lf.pillars[PILLAR_FORCE] > 0.5f && lf.energy > 20.0f) {
        // Apply force to physics system (will learn what works)
        float force[3] = {
            (lf.pillars[PILLAR_FORCE] - 0.5f) * 2.0f,
            0.0f,
            (lf.pillars[PILLAR_FORCE] - 0.5f) * 2.0f
        };
        lifeform_manipulate_physics(lf, force);
    }
}

void StarCluster::lifeform_act(Lifeform& lf, float delta_time) {
    // Apply force based on willpower and attraction
    float move_force = lf.pillars[PILLAR_WILLPOWER] * lf.pillars[PILLAR_ATTRACTION];

    // Movement (simplified)
    lf.x += move_force * delta_time * 10.0f;
    lf.y += 0.0f;
    lf.z += move_force * delta_time * 10.0f;

    // Energy cost for movement
    lf.energy -= move_force * delta_time * 2.0f;

    // Gain energy from successful actions (warmth, resources)
    if (lf.pillars[PILLAR_WARMTH] > 0.6f) {
        lf.energy += delta_time * 5.0f;  // Warmth provides energy
    }
}

void StarCluster::apply_physics_to_lifeform(Lifeform& lf, float delta_time) {
    // Lifeforms interact with the physics system
    // They LEARN to use the physics of the entire system to survive

    Planet* planet = nullptr;
    for (auto& p : planets_) {
        if (p.id == lf.planet_id) {
            planet = &p;
            break;
        }
    }
    if (!planet) return;

    // Physics interaction based on pillar state
    float gravity = planet->gravity;

    // If high Force pillar, lifeform tries to overcome gravity (physics manipulation!)
    if (lf.pillars[PILLAR_FORCE] > 0.7f) {
        // Apply upward force against gravity
        lf.energy -= gravity * delta_time * 0.1f;

        // Learning: If this works (energy preserved), reinforce Force
        if (lf.energy > 50.0f) {
            lf.pillars[PILLAR_FORCE] = std::min(1.0f, lf.pillars[PILLAR_FORCE] + 0.01f);
        }
    }

    // Flux pillar: controls transfer rate of energy/nutrients
    if (lf.pillars[PILLAR_FLUX] > 0.5f && planet->water_coverage > 0.5f) {
        // Efficient energy transfer from environment
        lf.energy += lf.pillars[PILLAR_FLUX] * delta_time * 3.0f;
    }

    // Depth pillar: latent capacity for survival
    if (lf.pillars[PILLAR_DEPTH] > 0.6f && lf.energy < 30.0f) {
        // Tap into reserves
        lf.energy += lf.pillars[PILLAR_DEPTH] * delta_time * 5.0f;
        lf.pillars[PILLAR_DEPTH] *= 0.99f;  // Consumption
    }
}

void StarCluster::lifeform_manipulate_physics(Lifeform& lf, const float force[3]) {
    // Record the force applied
    lf.last_interaction.force_applied[0] = force[0];
    lf.last_interaction.force_applied[1] = force[1];
    lf.last_interaction.force_applied[2] = force[2];

    // Energy cost for physics manipulation
    float force_magnitude = std::sqrt(force[0]*force[0] + force[1]*force[1] + force[2]*force[2]);
    lf.energy -= force_magnitude * 0.5f;

    // Learning: Did this help or hurt?
    if (lf.energy > 0.0f) {
        // Successful manipulation - reinforce
        lf.pillars[PILLAR_MEMORY] = std::min(1.0f, lf.pillars[PILLAR_MEMORY] + 0.02f);
    } else {
        // Failed - reduce force next time
        lf.pillars[PILLAR_FORCE] = std::max(0.1f, lf.pillars[PILLAR_FORCE] - 0.05f);
    }
}

float procedural::calculate_pressure(const Lifeform& lf, const Planet& planet) {
    // Environmental pressure calculation
    float temp_factor = std::abs(planet.surface_temp - 298.0f) / 100.0f;  // Deviation from comfortable
    float gravity_factor = planet.gravity / 9.8f;
    float atmosphere_factor = planet.atmosphere_density;

    return std::min(1.0f, (temp_factor + gravity_factor + atmosphere_factor) / 3.0f);
}

void StarCluster::evolve_lifeforms() {
    // Neuroevolution: Lifeforms that survived longer pass on traits
    std::vector<Lifeform*> alive;
    for (auto& lf : lifeforms_) {
        if (lf.alive && lf.fitness > 0.0f) {
            alive.push_back(&lf);
        }
    }

    if (alive.size() < 2) return;  // Need at least 2 for evolution

    // Sort by fitness (survival time)
    std::sort(alive.begin(), alive.end(),
        [](const Lifeform* a, const Lifeform* b) { return a->fitness > b->fitness; });

    // Keep top 50%, reproduce with mutation
    size_t survivors = alive.size() / 2;
    for (size_t i = 0; i < survivors && (survivors + i) < alive.size(); i++) {
        Lifeform* parent1 = alive[i];  // High fitness parent
        Lifeform* parent2 = alive[survivors + i];  // Random other parent

        // Create offspring (crossover + mutation)
        Lifeform offspring = *parent1;  // Start with parent1
        offspring.id = next_lifeform_id_++;
        offspring.generation = std::max(parent1->generation, parent2->generation) + 1;
        offspring.alive = true;
        offspring.fitness = 0.0f;
        offspring.age = 0.0f;
        offspring.energy = 100.0f;

        // Crossover: mix pillars from both parents
        for (int j = 0; j < NUM_PILLARS; j++) {
            if (rand() % 2 == 0) {
                offspring.pillars[j] = parent2->pillars[j];
            }
        }

        // Mutation: random changes (LEARNING!)
        std::uniform_real_distribution<float> dist(-0.1f, 0.1f);
        for (int j = 0; j < NUM_PILLARS; j++) {
            offspring.pillars[j] += dist(rng_);
            offspring.pillars[j] = std::max(0.0f, std::min(1.0f, offspring.pillars[j]));
        }

        lifeforms_.push_back(offspring);

        // Update planet count
        for (auto& planet : planets_) {
            if (planet.id == offspring.planet_id) {
                planet.lifeform_count++;
                break;
            }
        }
    }
}
