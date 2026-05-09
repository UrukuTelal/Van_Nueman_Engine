#ifndef VAN_NUEMAN_AGENTS_AGENT_INFO_H
#define VAN_NUEMAN_AGENTS_AGENT_INFO_H

#include <cstdint>
#include <array>
#include "../include/Entity.h"

using PillarVector = std::array<float, NUM_PILLARS>;

struct AgentInfo {
    uint32_t id;
    float x, y, z;
    float vx, vy, vz;
    PillarVector pillars;
    PillarVector shadow_state;
    bool active;
};

#endif // VAN_NUEMAN_AGENTS_AGENT_INFO_H
