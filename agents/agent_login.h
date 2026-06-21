#ifndef AGENT_LOGIN_H
#define AGENT_LOGIN_H

#include "../include/Entity.h"
#include <string>

struct BobAgent {
    uint32_t uid;
    char name[64];
    float pillars[NumPillars];
    float energy;
    bool logged_in;
};

uint32_t bob_agent_login(const char* player_name, const float pillars[NumPillars]);

void bob_agent_logout(uint32_t uid);

#endif // AGENT_LOGIN_H
