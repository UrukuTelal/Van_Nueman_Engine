// simulation_chamber.h - Test custom creatures before deployment
// From GAMEPLAY.md: "Test your creatures in the simulation chamber before deployment."
// Tiered depth based on upgrades/tech/research

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cstring>
#include <PillarEnum.h>

// Simulation result
struct SimulationResult {
    uint32_t creature_id;
    float fitness_score;
    float survival_time;       // seconds
    float energy_efficiency;
    uint32_t generation;
    bool deployment_ready;
    
    // Pillar performance during simulation
    float pillar_avg[NumPillars];
    float pillar_min[NumPillars];
    float pillar_max[NumPillars];
    
    char notes[256];            // Performance notes
};

// Creature template for simulation
struct CreatureTemplate {
    uint32_t genome_id;           // From CRISPR Vault
    char name[128];
    float pillar_template[NumPillars];
    
    // Physical specs
    float body_size;
    float metabolism;
    float adaptation;
    
    // Simulation parameters
    uint32_t simulation_ticks;
    float environment_harshness;    // 0.0 - 1.0
};

// Simulation Chamber system
class SimulationChamber {
public:
    SimulationChamber(uint32_t player_id);
    
    // Load creature from CRISPR Vault for simulation
    uint32_t load_creature(uint32_t genome_id, const char* creature_name);
    
    // Run simulation (returns simulation ID)
    uint32_t run_simulation(uint32_t creature_id, uint32_t ticks, 
                              float environment_harshness = 0.5f);
    
    // Get simulation result
    bool get_result(uint32_t simulation_id, SimulationResult& out_result) const;
    
    // Check if simulation complete
    bool is_simulation_complete(uint32_t simulation_id) const;
    
    // Get all simulation results for a creature
    std::vector<SimulationResult> get_creature_results(uint32_t creature_id) const;
    
    // Determine if creature is ready for deployment
    bool is_deployment_ready(uint32_t creature_id) const;
    
    // Tiered simulation depth (based on upgrades)
    void set_simulation_depth(uint32_t depth) { simulation_depth_ = depth; }
    uint32_t get_simulation_depth() const { return simulation_depth_; }
    
    // Cancel running simulation
    bool cancel_simulation(uint32_t simulation_id);
    
    // Get active simulation count
    uint32_t get_active_simulation_count() const { return active_simulations_.size(); }
    
private:
    uint32_t player_id_;
    uint32_t simulation_depth_;     // Upgrade tier: 1-3
    uint32_t next_simulation_id_;
    uint32_t next_creature_id_;
    
    // Loaded creatures
    struct SimCreature {
        uint32_t id;
        uint32_t genome_id;
        char name[128];
        float pillars[NumPillars];
        float body_size;
        float metabolism;
        float adaptation;
    };
    std::vector<SimCreature> creatures_;
    
    // Active simulations
    struct ActiveSimulation {
        uint32_t id;
        uint32_t creature_id;
        uint32_t ticks_total;
        uint32_t ticks_elapsed;
        float environment_harshness;
        bool complete;
        SimulationResult result;
    };
    std::vector<ActiveSimulation> active_simulations_;
    
    // Run single tick of simulation
    void simulate_tick(ActiveSimulation& sim, float delta_time);
    
    // Calculate fitness from pillar performance
    float calculate_fitness(const float pillar_avg[NumPillars]) const;
};
