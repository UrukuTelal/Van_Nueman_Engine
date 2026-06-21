#pragma once

#include "../scale/ScaleExponent.h"
#include "../include/Entity.h"
#include <cstdint>

struct CausalHandshakeMsg {
    uint32_t entity_uid_hash;
    uint32_t sid;
    uint32_t hid;
    ScaleExponent scale;
    uint32_t psv_hash;
    uint32_t timestamp;
    uint32_t causality_token;
    uint8_t promotion_flag;
};

class CausalHandshake {
public:
    static CausalHandshakeMsg build_promotion(const Entity& entity, uint32_t timestamp, uint32_t token);
    static CausalHandshakeMsg build_demotion(const Entity& entity, uint32_t timestamp, uint32_t token);

    static bool verify_token(const CausalHandshakeMsg& msg, uint32_t expected_token);
};
