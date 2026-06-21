#include "scale_router.h"
#include "../agents/cognition.h"
#include <algorithm>
#include <numeric>
#include <cstring>

void ScaleRouter::set_entity_scale(vn::simulation::AgentECS::Index idx, ScaleExponent scale) {
    entity_scales_[idx] = scale;
}

ScaleExponent ScaleRouter::get_entity_scale(vn::simulation::AgentECS::Index idx) const {
    auto it = entity_scales_.find(idx);
    if (it != entity_scales_.end()) return it->second;
    return SCALE_ORGANISM;
}

void ScaleRouter::build_routing_table() {
    scale_groups_.clear();
    routing_table_.routes.clear();
    routing_table_.total_coherence = 0.0f;
    routing_table_.active_scale_count = 0;

    for (auto& [idx, scale] : entity_scales_) {
        scale_groups_[scale].push_back(idx);
    }

    for (auto& [scale, indices] : scale_groups_) {
        routing_table_.active_scale_count++;
        for (auto& [other, other_indices] : scale_groups_) {
            if (scale == other) continue;
            ScaleRoute r;
            r.source_scale = scale;
            r.target_scale = other;
            r.delta = scale_delta(scale, other);
            r.coupling_coefficient = scale_coupling(r.delta);
            r.attention_attenuation = 1.0f / (1.0f + std::abs(static_cast<float>(r.delta)));
            routing_table_.routes.push_back(r);
        }
    }

    if (!scale_groups_.empty()) {
        float coherence_sum = 0.0f;
        for (auto& [scale, indices] : scale_groups_) {
            coherence_sum += static_cast<float>(indices.size());
        }
        routing_table_.total_coherence = coherence_sum / static_cast<float>(scale_groups_.size());
    }
}

ScaleRoute ScaleRouter::route(ScaleExponent source, ScaleExponent target) const {
    ScaleRoute r;
    r.source_scale = source;
    r.target_scale = target;
    r.delta = scale_delta(source, target);
    r.coupling_coefficient = scale_coupling(r.delta);
    r.attention_attenuation = 1.0f / (1.0f + std::abs(static_cast<float>(r.delta)));
    return r;
}

void ScaleRouter::route_pillars(vn::simulation::AgentECS& ecs,
                                 ScaleExponent source_scale,
                                 float dt) {
    auto it = scale_groups_.find(source_scale);
    if (it == scale_groups_.end()) return;

    float scale_factor = SCALE_COUPLING_BASE * std::pow(SCALE_ROUTER_PHI,
        static_cast<float>(source_scale) / 10.0f);

    for (auto idx : it->second) {
        if (idx >= ecs.size() || !ecs.active(idx)) continue;

        for (int p = 0; p < NumPillars; ++p) {
            float val = static_cast<float>(ecs.pillar(idx, static_cast<PillarIndex>(p)));
            float theta = bloch_value_to_theta(val);
            float drift_theta = theta + (0.5f - val) * dt * scale_factor;
            float new_val = bloch_theta_to_value(drift_theta);
            ecs.pillar(idx, static_cast<PillarIndex>(p)) = vn::fp20_t(new_val);
        }

        float integrity = static_cast<float>(ecs.pillar(idx, Integrity));
        float harm = static_cast<float>(ecs.pillar(idx, Harm));
        if (harm > 0.01f) {
            float harm_reduce = harm * dt * integrity * scale_factor;
            float new_harm = std::max(0.0f, harm - harm_reduce);
            ecs.pillar(idx, Harm) = vn::fp20_t(new_harm);
        }

        float coherence = 0.0f;
        for (int p = 0; p < NumPillars; ++p) {
            coherence += static_cast<float>(ecs.pillar(idx, static_cast<PillarIndex>(p)));
        }
        ecs.pillar(idx, Cohesion) = vn::fp20_t(coherence / static_cast<float>(NumPillars));
    }
}

void ScaleRouter::cross_route_entities(vn::simulation::AgentECS& ecs,
                                        ScaleExponent source_scale,
                                        ScaleExponent target_scale,
                                        float dt) {
    auto src_it = scale_groups_.find(source_scale);
    auto tgt_it = scale_groups_.find(target_scale);
    if (src_it == scale_groups_.end() || tgt_it == scale_groups_.end()) return;

    ScaleRoute r = route(source_scale, target_scale);
    float coupling = r.coupling_coefficient * dt;

    for (auto src_idx : src_it->second) {
        if (src_idx >= ecs.size() || !ecs.active(src_idx)) continue;
        for (auto tgt_idx : tgt_it->second) {
            if (tgt_idx >= ecs.size() || !ecs.active(tgt_idx)) continue;

            float src_force = static_cast<float>(ecs.pillar(src_idx, Force));
            float src_influence = static_cast<float>(ecs.pillar(src_idx, Influence));
            float tgt_resistance = static_cast<float>(ecs.pillar(tgt_idx, Resistance));

            float influence = src_force * src_influence * coupling * r.attention_attenuation;

            for (int p = 0; p < NumPillars; ++p) {
                float src_val = static_cast<float>(ecs.pillar(src_idx, static_cast<PillarIndex>(p)));
                float tgt_val = static_cast<float>(ecs.pillar(tgt_idx, static_cast<PillarIndex>(p)));
                float theta_src = bloch_value_to_theta(src_val);
                float theta_tgt = bloch_value_to_theta(tgt_val);
                float rotated = bloch_rotate(src_val, (theta_tgt - theta_src) * influence);
                ecs.pillar(tgt_idx, static_cast<PillarIndex>(p)) =
                    vn::fp20_t(rotated * r.attention_attenuation +
                               tgt_val * (1.0f - r.attention_attenuation));
            }
        }
    }
}

void ScaleRouter::route_all(vn::simulation::AgentECS& ecs, float dt) {
    for (auto& [scale, indices] : scale_groups_) {
        route_pillars(ecs, scale, dt);
    }

    std::vector<std::pair<ScaleExponent, ScaleExponent>> pairs;
    for (auto& [s1, _] : scale_groups_) {
        for (auto& [s2, _] : scale_groups_) {
            if (s1 < s2) pairs.emplace_back(s1, s2);
        }
    }

    for (auto& [s1, s2] : pairs) {
        cross_route_entities(ecs, s1, s2, dt);
    }
}

float ScaleRouter::scale_coupling(ScaleExponent delta) {
    float abs_delta = static_cast<float>(std::abs(delta));
    return SCALE_COUPLING_BASE * std::pow(SCALE_ROUTER_PHI, -abs_delta / 10.0f);
}

float ScaleRouter::scale_interaction_radius(ScaleExponent scale) {
    return 10.0f * std::pow(SCALE_ROUTER_PHI, static_cast<float>(scale) / 10.0f);
}
