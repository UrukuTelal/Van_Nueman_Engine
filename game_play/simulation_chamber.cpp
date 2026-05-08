// simulation_chamber.cpp - Simulation Chamber implementation
// Test custom creatures before deployment

#include "simulation_chamber.h"
#include <ctime>
#include <cstdlib>
#include <cmath>

SimulationChamber::SimulationChamber(uint32_t player_id) :
    player_id_(player_id), simulation_depth_(1), next_simulation_id_(4000), next_creature_id_(5000) {
}

uint32_t SimulationChamber::load_creature(uint32_t genome_id, const char* creature_name) {
    SimCreature sc;
    sc.id = next_creature_id_++;
    sc.genome_id = genome_id;
    strncpy(sc.name, creature_name, 127);
    sc.name[127] = '\0';
    
    // Default pillar template
    for (int i = 0; i < NUM_PILLARS; i++) {
        sc.pillars[i] = 0.5f;
    }
    sc.body_size = 0.5f;
    sc.metabolism = 0.5f;
    sc.adaptation = 0.3f;
    
    creatures_.push_back(sc);
    return sc.id;
}

uint32_t SimulationChamber::run_simulation(uint32_t creature_id, uint32_t ticks,
                                             float environment_harshness) {
    // Find creature
    SimCreature* creature = nullptr;
    for (auto& c : creatures_) {
        if (c.id == creature_id) {
            creature = &c;
            break;
        }
    }
    if (!creature) return 0;
    
    ActiveSimulation sim;
    sim.id = next_simulation_id_++;
    sim.creature_id = creature_id;
    sim.ticks_total = ticks;
    sim.ticks_elapsed = 0;
    sim.environment_harshness = environment_harshness;
    sim.complete = false;
    
    // Initialize result
    sim.result.creature_id = creature_id;
    sim.result.fitness_score = 0.0f;
    sim.result.survival_time = 0.0f;
    sim.result.energy_efficiency = 1.0f;
    sim.result.generation = 1;
    sim.result.deployment_ready = false;
    for (int i = 0; i < NUM_PILLARS; i++) {
        sim.result.pillar_avg[i] = creature->pillars[i];
        sim.result.pillar_min[i] = creature->pillars[i];
        sim.result.pillar_max[i] = creature->pillars[i];
    }
    sim.result.notes[0] = '\0';
    
    active_simulations_.push_back(sim);
    return sim.id;
}

bool SimulationChamber::get_result(uint32_t simulation_id, SimulationResult& out_result) const {
    for (const auto& sim : active_simulations_) {
        if (sim.id == simulation_id && sim.complete) {
            out_result = sim.result;
            return true;
        }
    }
    return false;
}

bool SimulationChamber::is_simulation_complete(uint32_t simulation_id) const {
    for (const auto& sim : active_simulations_) {
        if (sim.id == simulation_id) {
            return sim.complete;
        }
    }
    return false;
}

std::vector<SimulationResult> SimulationChamber::get_creature_results(uint32_t creature_id) const {
    std::vector<SimulationResult> results;
    for (const auto& sim : active_simulations_) {
        if (sim.creature_id == creature_id && sim.complete) {
            results.push_back(sim.result);
        }
    }
    return results;
}

bool SimulationChamber::is_deployment_ready(uint32_t creature_id) const {
    auto results = get_creature_results(creature_id);
    if (results.empty()) return false;
    
    // Check if any simulation passed criteria
    for (const auto& r : results) {
        if (r.deployment_ready) return true;
    }
    return false;
}

bool SimulationChamber::cancel_simulation(uint32_t simulation_id) {
    for (auto it = active_simulations_.begin(); it != active_simulations_.end(); ++it) {
        if (it->id == simulation_id) {
            active_simulations_.erase(it);
            return true;
        }
    }
    return false;
}

void SimulationChamber::simulate_tick(ActiveSimulation& sim, float delta_time) {
    // Find creature
    SimCreature* creature = nullptr;
    for (auto& c : creatures_) {
        if (c.id == sim.creature_id) {
            creature = &c;
            break;
        }
    }
    if (!creature) return;
    
    sim.ticks_elapsed++;
    
    // Simulate pillar performance under environmental stress
    float stress_factor = sim.environment_harshness * (1.0f + rand() % 100 / 100.0f);
    
    for (int i = 0; i < NUM_PILLARS; i++) {
        float pillar_val = creature->pillars[i];
        float stress_impact = stress_factor * (1.0f - pillar_val);  // Lower pillars = more impact
        
        float new_val = pillar_val - stress_impact * delta_time;
        if (new_val < sim.result.pillar_min[i]) sim.result.pillar_min[i] = new_val;
        if (new_val > sim.result.pillar_max[i]) sim.result.pillar_max[i] = new_val;
        
        // Update running average
        sim.result.pillar_avg[i] = (sim.result.pillar_avg[i] * (sim.ticks_elapsed - 1) + new_val) / sim.ticks_elapsed;
    }
    
    sim.result.survival_time += delta_time;
    
    // Check completion
    if (sim.ticks_elapsed >= sim.ticks_total) {
        sim.complete = true;
        
        // Calculate fitness
        float avg_fitness = 0.0f;
        for (int i = 0; i < NUM_PILLARS; i++) {
            avg_fitness += sim.result.pillar_avg[i];
        }
        avg_fitness /= NUM_PILLARS;
        
        // Adjust for environment
        sim.result.fitness_score = avg_fitness * (1.0f - sim.environment_harshness * 0.3f);
        sim.result.energy_efficiency = 1.0f - sim.environment_harshness;
        
        // Deployment ready if fitness > 0.6 and survived
        sim.result.deployment_ready = (sim.result.fitness_score > 0.6f);
        
        snprintf(sim.result.notes, 255, "Simulation complete. Ticks: %u, Fitness: %.2f",
                 sim.ticks_elapsed, sim.result.fitness_score);
        sim.result.notes[255] = '\0';
    }
}

float SimulationChamber::calculate_fitness(const float pillar_avg[NUM_PILLARS]) const {
    float fitness = 0.0f;
    for (int i = 0; i < NUM_PILLARS; i++) {
        fitness += pillar_avg[i];
    }
    return fitness / NUM_PILLARS;
}
