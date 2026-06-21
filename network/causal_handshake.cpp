#include "causal_handshake.h"

CausalHandshakeMsg CausalHandshake::build_promotion(const Entity& entity, uint32_t timestamp, uint32_t token) {
    CausalHandshakeMsg msg;
    msg.entity_uid_hash = entity.uid.psv_hash;
    msg.sid = entity.uid.sid;
    msg.hid = entity.uid.hid;
    msg.scale = entity.native_scale;
    msg.psv_hash = compute_entity_id(entity.baseline_pillars);
    msg.timestamp = timestamp;
    msg.causality_token = token;
    msg.promotion_flag = 1;
    return msg;
}

CausalHandshakeMsg CausalHandshake::build_demotion(const Entity& entity, uint32_t timestamp, uint32_t token) {
    CausalHandshakeMsg msg = build_promotion(entity, timestamp, token);
    msg.promotion_flag = 0;
    return msg;
}

bool CausalHandshake::verify_token(const CausalHandshakeMsg& msg, uint32_t expected_token) {
    return msg.causality_token == expected_token;
}
