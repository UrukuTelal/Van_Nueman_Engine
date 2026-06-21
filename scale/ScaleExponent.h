#pragma once

#include "../vn/ScaledInt.h"
#include <cstdint>

using ScaleExponent = int32_t;

constexpr ScaleExponent SCALE_MIN = -20;
constexpr ScaleExponent SCALE_MAX = 60;

constexpr ScaleExponent SCALE_ATOM = 0;
constexpr ScaleExponent SCALE_MOLECULE = 10;
constexpr ScaleExponent SCALE_CELL = 20;
constexpr ScaleExponent SCALE_ORGANISM = 30;
constexpr ScaleExponent SCALE_SOCIETY = 40;
constexpr ScaleExponent SCALE_PLANET = 50;
constexpr ScaleExponent SCALE_STAR_SYSTEM = 55;
constexpr ScaleExponent SCALE_GALAXY = 58;
constexpr ScaleExponent SCALE_UNIVERSE = 60;

inline ScaleExponent scale_delta(ScaleExponent observer, ScaleExponent target) {
    return observer - target;
}

inline vn::fp20_t scale_attention_attenuation(vn::fp20_t base_attention, ScaleExponent delta) {
    if (delta > 0) {
        return base_attention >> delta;
    } else if (delta < 0) {
        return base_attention << (-delta);
    }
    return base_attention;
}
