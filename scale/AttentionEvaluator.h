#pragma once

#include "ScaleExponent.h"
#include "../include/Entity.h"
#include <cstdint>

class AttentionEvaluator {
public:
    static constexpr vn::fp20_t NYQUIST_PILLAR_LIMIT = vn::fp20_t(0.15);

    static bool evaluate_presence(ScaleExponent observer_scale,
                                   vn::fp20_t attention_focus,
                                   const Entity& target,
                                   vn::fp20_t out_perceived_state[16])
    {
        ScaleExponent delta = scale_delta(observer_scale, target.native_scale);
        vn::fp20_t scaled_attention = scale_attention_attenuation(attention_focus, delta);

        vn::fp20_t total_magnitude(0);
        for (int i = 0; i < NumPillars; i++) {
            total_magnitude = total_magnitude + vn::fp20_t(std::abs(target.pillars[i]));
        }

        vn::fp20_t promotion_threshold = (NYQUIST_PILLAR_LIMIT * total_magnitude);

        if (promotion_threshold < scaled_attention) {
            for (int i = 0; i < NumPillars; i++) {
                out_perceived_state[i] = vn::fp20_t(target.pillars[i]);
            }
            return true;
        }

        vn::fp20_t attenuation_factor = scaled_attention;
        if (vn::fp20_t(1) < attenuation_factor) attenuation_factor = vn::fp20_t(1);
        // Attenuate by rotating toward the equator (θ=π/2, value=0.5)
        // Full attention (factor=1) → no rotation, no change.
        // No attention (factor=0) → full rotation to equator.
        for (int i = 0; i < NumPillars; i++) {
            float theta = pillar_value_to_theta(vn::fp20_t(target.pillars[i])).to_float();
            float equator = 1.5707963f;  // π/2
            float diff = equator - theta;
            float pull = diff * (1.0f - attenuation_factor.to_float());
            if (fabsf(pull) > 0.001f) {
                out_perceived_state[i] = apply_bloch_rotation(vn::fp20_t(target.pillars[i]), vn::fp20_t(pull));
            } else {
                out_perceived_state[i] = vn::fp20_t(target.pillars[i]);
            }
        }
        return false;
    }
};
