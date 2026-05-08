#include "agent_login.h"
#include <cstring>
#include <ctime>
#include <cstdio>

uint32_t bob_agent_login(const char* player_name, const float pillars[NUM_PILLARS]) {
    BobAgent agent;
    agent.uid = (uint32_t)time(NULL);
    snprintf(agent.name, sizeof(agent.name), "%s", player_name);
    for(int i = 0; i < NUM_PILLARS; i++) agent.pillars[i] = pillars[i];
    agent.energy = 100.0f;
    agent.logged_in = true;
    
    // Save to Pillar_10_Memory/uid_matrices/
    char path[256];
    snprintf(path, sizeof(path), "Pillar_10_Memory/uid_matrices/Bob_%u.json", agent.uid);
    
    FILE* f = fopen(path, "w");
    if (f) {
        fprintf(f, "{\"uid\": %u, \"name\": \"%s\", \"energy\": %.1f, \"logged_in\": true}", 
                agent.uid, player_name, agent.energy);
        fclose(f);
    }
    
    return agent.uid;
}

void bob_agent_logout(uint32_t uid) {
    char path[256];
    snprintf(path, sizeof(path), "Pillar_10_Memory/uid_matrices/Bob_%u.json", uid);
    
    FILE* f = fopen(path, "r");
    if (f) {
        // Mark as logged out in file (simplified)
        fclose(f);
    }
}
