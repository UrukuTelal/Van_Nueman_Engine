#pragma once

#include "../include/Entity.h"
#include <cstdint>
#include <vector>
#include <cmath>
#include <complex>

namespace vn {
namespace quantum {

// ── fp20_t → Quantum Amplitude ─────────────────────────────────
// Single-qubit angle encoding: value [0,1] → θ ∈ [0,π],
// amplitudes = [cos(θ/2), sin(θ/2)].
// Perfect round-trip: decode(square(encode))) ≈ original.

inline void value_to_qubit_amps(vn::fp20_t value, float& amp0, float& amp1) {
    vn::fp20_t theta = pillar_value_to_theta(value); // [0, π]
    float half = theta.to_float() * 0.5f;
    amp0 = std::cos(half);
    amp1 = std::sin(half);
}

inline vn::fp20_t qubit_amps_to_value(float amp0, float amp1) {
    float prob0 = amp0 * amp0;
    float prob1 = amp1 * amp1;
    float total = prob0 + prob1;
    if (total < 1e-12f) return vn::fp20_t(0.5f);
    float theta = 2.0f * std::acos(std::sqrt(prob0 / total));
    return theta_to_pillar_value(vn::fp20_t(theta));
}

// ── Single fp20_t conversion (simple sqrt encoding, no angle) ──

inline float pillar_to_amplitude(vn::fp20_t value) {
    float v = vn::clamp(value, vn::fp20_t(0.0f), vn::fp20_t(1.0f)).to_float();
    return std::sqrt(v);
}

inline float amplitude_to_pillar(float amp) {
    float v = amp * amp;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return v;
}

// ── PillarStateVector → Amplitude Encoding ─────────────────────
// Angle-encode each pillar to a pair of amplitudes [cos(θ/2), sin(θ/2)].
// Output: 2*NumPillars amplitudes, valid for 1-qubit-per-pillar processing.

inline std::vector<float> encode_pillars_to_amplitudes(
    const PillarStateVector& psv,
    uint32_t /*num_qubits unused in angle encoding*/ = 4
) {
    std::vector<float> amps;
    amps.reserve(2 * NumPillars);
    for (uint32_t i = 0; i < NumPillars; i++) {
        float a0, a1;
        value_to_qubit_amps(vn::fp20_t(psv.pillars[i]), a0, a1);
        amps.push_back(a0);
        amps.push_back(a1);
    }
    return amps;
}

// ── Amplitude → PillarStateVector ──────────────────────────────
// Decode angle-encoded amplitude pairs back to pillar values.

inline PillarStateVector decode_amplitudes_to_pillars(
    const std::vector<float>& amplitudes,
    uint32_t num_pillars = NumPillars
) {
    PillarStateVector result;
    for (uint32_t i = 0; i < num_pillars; i++) {
        size_t idx = 2 * i;
        float a0 = (idx < amplitudes.size()) ? amplitudes[idx] : 1.0f;
        float a1 = (idx + 1 < amplitudes.size()) ? amplitudes[idx + 1] : 0.0f;
        result.pillars[i] = qubit_amps_to_value(a0, a1);
    }
    return result;
}

// ── Complex Amplitude Support ──────────────────────────────────
// Angle encoding with complex phase (for full quantum state tomography).

using ComplexAmplitude = std::complex<float>;

inline std::vector<ComplexAmplitude> encode_pillars_to_complex(
    const PillarStateVector& psv,
    uint32_t /*num_qubits unused*/ = 4
) {
    std::vector<ComplexAmplitude> amps;
    amps.reserve(2 * NumPillars);
    for (uint32_t i = 0; i < NumPillars; i++) {
        float a0, a1;
        value_to_qubit_amps(vn::fp20_t(psv.pillars[i]), a0, a1);
        amps.emplace_back(a0, 0.0f);
        amps.emplace_back(a1, 0.0f);
    }
    return amps;
}

inline PillarStateVector decode_complex_to_pillars(
    const std::vector<ComplexAmplitude>& amplitudes,
    uint32_t num_pillars = NumPillars
) {
    PillarStateVector result;
    for (uint32_t i = 0; i < num_pillars; i++) {
        size_t idx = 2 * i;
        float a0 = (idx < amplitudes.size()) ? std::abs(amplitudes[idx]) : 1.0f;
        float a1 = (idx + 1 < amplitudes.size()) ? std::abs(amplitudes[idx + 1]) : 0.0f;
        result.pillars[i] = qubit_amps_to_value(a0, a1);
    }
    return result;
}

} // namespace quantum
} // namespace vn
