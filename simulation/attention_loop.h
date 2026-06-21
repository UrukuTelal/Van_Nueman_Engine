#pragma once

#include "../scale/ScaleExponent.h"
#include "../scale/AttentionEvaluator.h"
#include "../scale/InfluenceBuffer.h"
#include "../audio/wht_scaled.h"
#include "../include/Entity.h"
#include <cstdint>
#include <vector>

struct AttentionStepResult {
    uint32_t entities_promoted;
    uint32_t entities_demoted;
    uint32_t regions_scanned;
};

class AttentionLoop {
public:
    AttentionLoop();

    AttentionStepResult step(
        const std::vector<Entity>& observers,
        std::vector<Entity>& all_entities,
        InfluenceBuffer& influence_buf,
        int32_t tick
    );

private:
    bool check_anomalous_coefficient(const RegionInfluence& region, vn::fp20_t threshold);
    void materialize_entity(const RegionInfluence& region,
                            const vn::fp20_t observer_pillars[16],
                            Entity& out_entity);
    void causal_handshake(uint32_t entity_id, ScaleExponent scale, uint32_t causality_token);
};
