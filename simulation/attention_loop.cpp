#include "attention_loop.h"
#include <cmath>

AttentionLoop::AttentionLoop() {}

AttentionStepResult AttentionLoop::step(
    const std::vector<Entity>& observers,
    std::vector<Entity>& all_entities,
    InfluenceBuffer& influence_buf,
    int32_t tick)
{
    AttentionStepResult result = {0, 0, 0};

    for (const auto& obs : observers) {
        ScaleExponent obs_scale = obs.native_scale;
        vn::fp20_t focus = vn::fp20_t(obs.pillars[Awareness]);

        for (auto& target : all_entities) {
            if (target.id == obs.id) continue;
            int32_t cx = target.grid_cell_x;
            int32_t cy = target.grid_cell_y;

            vn::fp20_t perceived[16];
            bool is_present = AttentionEvaluator::evaluate_presence(
                obs_scale, focus, target, perceived
            );

            if (is_present && target.flags.constrained) {
                target.flags.constrained = 0;
                for (int i = 0; i < NumPillars; i++) {
                    target.pillars[i] = perceived[i];
                }
                result.entities_promoted++;
                influence_buf.subtract(cx, cy, 0, perceived, vn::fp20_t(10));
            } else if (!is_present && !target.flags.constrained) {
                target.flags.constrained = 1;
            {
                vn::fp20_t fp20_pillars[16];
                for (int i = 0; i < 16; i++) fp20_pillars[i] = vn::fp20_t(target.pillars[i]);
                influence_buf.accumulate(cx, cy, 0, fp20_pillars, vn::fp20_t(10));
            }
                result.entities_demoted++;
            }
        }
        result.regions_scanned++;
    }

    return result;
}

bool AttentionLoop::check_anomalous_coefficient(const RegionInfluence& region, vn::fp20_t threshold) {
    for (uint32_t i = 0; i < INFLUENCE_COEFFS; i++) {
        if (threshold < region.wht_coefficients[i].abs()) {
            return true;
        }
    }
    return false;
}

void AttentionLoop::materialize_entity(const RegionInfluence& region,
                                        const vn::fp20_t observer_pillars[16],
                                        Entity& out_entity)
{
    vn::fp20_t coeffs[32];
    for (uint32_t i = 0; i < INFLUENCE_COEFFS; i++) {
        coeffs[i] = region.wht_coefficients[i];
    }
    ifwht_fp(coeffs, 5, false);

    for (int i = 0; i < 16 && i < 32; i++) {
        out_entity.pillars[i] = coeffs[i];
    }
    out_entity.update_id();
}

void AttentionLoop::causal_handshake(uint32_t entity_id, ScaleExponent scale, uint32_t causality_token) {
    (void)entity_id;
    (void)scale;
    (void)causality_token;
}
