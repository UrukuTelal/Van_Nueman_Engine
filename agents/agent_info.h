#ifndef VAN_NUEMAN_AGENTS_AGENT_INFO_H
#define VAN_NUEMAN_AGENTS_AGENT_INFO_H

#include <cstdint>
#include <array>
#include "../include/Entity.h"

using PillarVector = std::array<float, NUM_PILLARS>;

struct AgentInfo {
    uint32_t id;
    float x, y, z;          // Position
    float vx, vy, vz;       // Velocity
    PillarVector pillars;    // 16-dimensional pillar state
    bool active;              // Whether agent is active
};

#endif // VAN_NUEMAN_AGENTS_AGENT_INFO_H
