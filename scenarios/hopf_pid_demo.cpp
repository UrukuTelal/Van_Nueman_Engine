#include <iostream>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include "../simulation/tick_loop.h"

static void print_hopf_state(const HopfPIDState& s, int id) {
    float f0[NumPillars];
    for (int p = 0; p < NumPillars; p++)
        f0[p] = s.frames[0][p];

    printf("[Agent %3d] rc=%.3f rupture=%s mem=[%.3f %.3f ... %.3f] "
           "pillars=[A=%.2f W=%.2f F=%.2f I=%.2f R=%.2f Int=%.2f "
           "Coh=%.2f Rel=%.2f Pr=%.2f Wa=%.2f Me=%.2f "
           "Att=%.2f H=%.2f D=%.2f Fl=%.2f Dep=%.2f]\n",
           id, s.relational_complexity, s.in_rupture ? "YES" : "no",
           s.membrane[0], s.membrane[1], s.membrane[HOPF_BASE_DIM - 1],
           f0[0], f0[1], f0[2], f0[3], f0[4], f0[5],
           f0[6], f0[7], f0[8], f0[9], f0[10],
           f0[11], f0[12], f0[13], f0[14], f0[15]);
}

int main() {
    std::cout << "=== Hopf-PID Interaction Engine Demo ===" << std::endl;

    SimulationTickLoop sim;
    if (!sim.initialize(100)) {
        std::cerr << "Failed to initialize simulation" << std::endl;
        return 1;
    }

    int numAgents = 10;
    std::cout << "Spawning " << numAgents << " agents..." << std::endl;
    for (int i = 0; i < numAgents; i++) {
        float x = static_cast<float>(std::rand()) / RAND_MAX * 100.0f;
        float y = static_cast<float>(std::rand()) / RAND_MAX * 100.0f;
        float z = static_cast<float>(std::rand()) / RAND_MAX * 100.0f;
        sim.add_agent(x, y, z);
    }

    sim.hopf_ensure_state_size();

    // Initialize each agent's HopfPID state with varied pillar values
    for (int i = 0; i < numAgents; i++) {
        HopfPIDState& hs = sim.get_hopf_state(static_cast<vn::simulation::AgentECS::Index>(i));
        hs.init();
        for (int p = 0; p < NumPillars; p++) {
            float val = 0.3f + static_cast<float>(std::rand()) / RAND_MAX * 0.4f;
            hs.frames[0][p] = vn::fp20_t(val);
        }
        hs.encode_all_frames();

        // Spawn a fiber for each agent
        hs.fiber_count = 1;
        hs.fibers[0].init(0, static_cast<uint32_t>(i),
                           static_cast<float>(std::rand()) / RAND_MAX * 6.2832f);
    }

    std::cout << "Running Hopf-PID ticks..." << std::endl;
    int ticks = 100;

    // Print initial state
    printf("\n--- Tick 0 (initial) ---\n");
    for (int i = 0; i < numAgents; i++) {
        print_hopf_state(sim.get_hopf_state(static_cast<vn::simulation::AgentECS::Index>(i)), i);
    }

    for (int t = 1; t <= ticks; t++) {
        float hazard = 0.1f + 0.05f * std::sin(t * 0.1f);
        float resources = 0.7f + 0.1f * std::cos(t * 0.05f);

        sim.hopf_tick_all(0.016f, hazard, resources);

        if (t % 25 == 0) {
            printf("\n--- Tick %d (hazard=%.3f, resources=%.3f) ---\n",
                   t, hazard, resources);
            for (int i = 0; i < numAgents; i++) {
                print_hopf_state(sim.get_hopf_state(
                    static_cast<vn::simulation::AgentECS::Index>(i)), i);
            }
        }
    }

    // Summary: compute average relational complexity across agents
    float avg_rc = 0.0f;
    int rupture_count = 0;
    for (int i = 0; i < numAgents; i++) {
        const HopfPIDState& hs = sim.get_hopf_state(
            static_cast<vn::simulation::AgentECS::Index>(i));
        avg_rc += hs.relational_complexity;
        if (hs.in_rupture) rupture_count++;
    }
    avg_rc /= numAgents;

    printf("\n=== Summary ===\n");
    printf("Agents: %d\n", numAgents);
    printf("Ticks: %d\n", ticks);
    printf("Avg Relational Complexity: %.4f\n", avg_rc);
    printf("Agents in Rupture: %d\n", rupture_count);

    // Check for soliton stabilization (agents with baseline integrity)
    int soliton_active = 0;
    for (int i = 0; i < numAgents; i++) {
        const HopfPIDState& hs = sim.get_hopf_state(
            static_cast<vn::simulation::AgentECS::Index>(i));
        if (hs.frames[0][Integrity] < 0.2f)
            soliton_active++;
    }
    printf("Soliton-stabilized agents: %d\n", soliton_active);

    printf("=== Demo Complete ===\n");
    return 0;
}
