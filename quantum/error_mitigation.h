#pragma once

#include "../include/Entity.h"
#include <vector>
#include <cmath>

namespace vn {
namespace quantum {

struct NoiseModel {
    double depolarizing_rate = 0.0;
    double amplitude_damping = 0.0;
    double dephasing_rate = 0.0;

    bool has_noise() const {
        return depolarizing_rate > 0.0 ||
               amplitude_damping > 0.0 ||
               dephasing_rate > 0.0;
    }
};

inline NoiseModel estimate_noise() {
    NoiseModel nm;
    nm.depolarizing_rate = 0.001;
    nm.amplitude_damping = 0.0005;
    nm.dephasing_rate = 0.002;
    return nm;
}

inline std::vector<float> correct_readout_errors(
    const std::vector<float>& raw_amplitudes,
    const NoiseModel& noise = NoiseModel{}
) {
    if (!noise.has_noise() || raw_amplitudes.empty())
        return raw_amplitudes;
    std::vector<float> corrected(raw_amplitudes.size());
    double total = 0.0;
    for (size_t i = 0; i < raw_amplitudes.size(); i++) {
        corrected[i] = raw_amplitudes[i] / (1.0f + (float)noise.depolarizing_rate);
        if (corrected[i] < 0.0f) corrected[i] = 0.0f;
        total += corrected[i];
    }
    if (total > 0.0) {
        double inv = 1.0 / total;
        for (auto& v : corrected) v = (float)(v * inv);
    }
    return corrected;
}

inline std::vector<float> zero_noise_extrapolation(
    const std::vector<float>& noisy,
    const std::vector<float>& noisier,
    double noise_factor = 2.0
) {
    if (noisy.size() != noisier.size() || noisy.empty())
        return noisy;
    std::vector<float> corrected(noisy.size());
    double inv = 1.0 / (noise_factor - 1.0);
    for (size_t i = 0; i < noisy.size(); i++) {
        double val = (noise_factor * noisy[i] - noisier[i]) * inv;
        corrected[i] = (float)std::max(0.0, val);
    }
    return corrected;
}

inline PillarStateVector mitigate_pillar_measurement(
    const PillarStateVector& measured,
    const NoiseModel& noise = NoiseModel{}
) {
    if (!noise.has_noise()) return measured;
    PillarStateVector result;
    for (int i = 0; i < NumPillars; i++) {
        float raw = measured.p[i].to_float();
        float corrected = raw / (1.0f + (float)noise.depolarizing_rate);
        if (corrected < 0.0f) corrected = 0.0f;
        if (corrected > 1.0f) corrected = 1.0f;
        result.p[i] = vn::fp20_t(corrected);
    }
    return result;
}

} // namespace quantum
} // namespace vn
