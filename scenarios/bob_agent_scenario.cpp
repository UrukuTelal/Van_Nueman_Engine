#include <iostream>
#include "../agents/agent_login.h"
#include "../neuro/entity_neural_net.h"
#include "../physics/energy_system.h"
#include "../scene/star_cluster.h"
#include "../biology/biological_system.h"
#include "../rendering/opacity_renderer.h"

int main() {
    std::cout << "Bob Agent Scenario - Van Nueman Ecosystem" << std::endl;
    
    // Bob agent login with WHT-encoded pillars
    float bob_pillars[NumPillars];
    for (int i = 0; i < NumPillars; i++) bob_pillars[i] = 0.5f;
    uint32_t bob_uid = bob_agent_login("Bob", bob_pillars);
    std::cout << "Bob logged in, UID: " << bob_uid << std::endl;
    
    // Initialize neural net (GPU)
    EntityNeuralNet nn;
    if (!nn.init(nullptr, 500000)) {
        std::cerr << "Failed to init neural net" << std::endl;
        return 1;
    }
    
    // Initialize star cluster (energy source)
    StarCluster cluster(12345);
    cluster.generate_cluster(100);  // 100 stars
    
    std::cout << "Scenario initialized. Running main loop..." << std::endl;
    
    // Main loop (simplified)
    float dt = 0.016f;  // 60 FPS
    Entity bob_entity;
    bob_entity.id = bob_uid;
    bob_entity.flags.constrained = 0;
    
    BioState bob_bio = {50.0f, 0.0f, 0, false};
    
    int tick = 0;
    while (!bob_entity.flags.constrained) {
        // 1. Update biology (food, reproduction, death)
        update_biology(&bob_entity, dt, &bob_bio);
        
        // 2. Apply thermodynamics (energy from stars)
        if (cluster.get_stars().size() > 0) {
            const Star& nearest_star = cluster.get_stars()[0];  // Simplified: use first star
            RadiantFlux flux = compute_radiant_flux(nearest_star, 1.0f);  // 1 AU distance
            apply_thermodynamics(&bob_entity, dt, flux);
        }
        
        // 3. Neural net decides action (GPU compute with WHT pillars)
        EntityQTable qtable;
        qtable.epsilon = 1.0f;
        qtable.training_steps = 0;
        for (int i = 0; i < ACTION_COUNT; i++) qtable.q_values[i] = 0.0f;
        
        float input_pillars[NumPillars];
        for (int i = 0; i < NumPillars; i++) input_pillars[i] = bob_entity.pillars[i];
        uint32_t action = EntityNeuralNet::select_action_cpu(
            input_pillars, 
            qtable,
            true
        );
        
        // 4. Execute action
        switch (action) {
            case ACTION_MOVE:
                bob_entity.pos_x = bob_entity.pos_x + vn::fp20_t(0.1f) * bob_entity.pillars[Force];
                break;
            case ACTION_FIND_FOOD:
                find_food(&bob_entity, &bob_bio);
                break;
            case ACTION_REPRODUCE:
                std::cout << "Tick " << tick << ": Bob wants to reproduce!" << std::endl;
                break;
        }
        
        // 5. Render with WHT pillar opacity
        float px = bob_entity.pos_x;
        float py = bob_entity.pos_y;
        float pz = 0.0f;
        float pillar_floats[NumPillars];
    for (int i = 0; i < NumPillars; i++) pillar_floats[i] = bob_entity.pillars.pillars[i];
    render_voxel_with_pillar_opacity(px, py, pz, 1.0f, pillar_floats);
        
        tick++;
        if (tick % 100 == 0) {
            std::cout << "Tick " << tick 
                      << ", CogState=" << bob_entity.constraint_level.to_float()
                      << ", Food=" << bob_bio.food_level 
                      << std::endl;
        }
        
        if (tick > 10000) break;  // Stop after 10k ticks
    }
    
    bob_agent_logout(bob_uid);
    std::cout << "Bob logged out. Scenario complete." << std::endl;
    
    return 0;
}
