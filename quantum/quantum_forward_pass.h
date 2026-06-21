#pragma once

#include "amplitude_encoding.h"
#include "native_quantum_backend.h"
#include "wht_tokenizer.h"
#include "../include/HopfPID.h"
#include "../audio/wht_packet.h"

namespace vn {
namespace quantum {

// ── Quantum Cognitive Forward Pass ──────────────────────────
// Replaces query_model's text→model→text pipeline with
// PSV→quantum-amplitudes→WHT→Hopf→PSV processing.
//
// Pipeline:
//   1. Angle-encode PSV → 32 amplitudes (16 qubits)
//   2. WHT transform → 32 frequency coefficients
//   3. Decode WHT → 16-pillar base frame via inverse WHT
//   4. Construct 32 Hopf frames with phase-modulated variations
//      → 32×16 = 512D thought vector
//   5. Normalize to unit S^511 sphere
//   6. Hopf project → 32D membrane
//   7. Decode 32D membrane → 16 pillars via inverse WHT
//
inline PillarStateVector quantum_cognitive_pass(
    const PillarStateVector& input,
    NativeQuantumBackend& backend,
    const HopfProjectionMatrix& proj
) {
    // Step 1: PSV → 32 amplitudes (angle encoding)
    std::vector<float> amps = encode_pillars_to_amplitudes(input);

    // Step 2: WHT transform → 32 frequency coefficients
    float wht_coeffs[WHT_N];
    {
        float in32[WHT_N] = {0};
        for (int i = 0; i < (int)amps.size() && i < WHT_N; i++)
            in32[i] = amps[i];
        backend.execute_wht(in32, wht_coeffs);
    }

    // Step 3: Decode WHT → one 16-pillar base frame
    float base_frame[NumPillars];
    decode_pillar_vector(wht_coeffs, base_frame);

    // Step 4: Construct 32-frame thought vector (512D)
    // Each frame is a Bloch-rotated version of the base frame,
    // with sinusoidal phase modulation across frames to provide
    // fiber variation for the Hopf projection.
    float thought[HOPF_FIBER_DIM];
    for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
        float phase = 2.0f * 3.14159265f * (float)f / (float)HOPF_FRAME_COUNT;
        for (int p = 0; p < NumPillars; p++) {
            float modulation = 0.1f * std::sin(phase + (float)p * 0.2f);
            vn::fp20_t rotated = bloch_rotate(
                vn::fp20_t(base_frame[p]),
                vn::fp20_t(modulation)
            );
            thought[f * NumPillars + p] = rotated.to_float();
        }
    }

    // Step 5: Normalize to unit S^511
    normalize_thought(thought, HOPF_FIBER_DIM);

    // Step 6: Hopf project → 32D membrane
    float membrane[HOPF_BASE_DIM];
    proj.project(thought, membrane);

    // Step 7: Decode 32D membrane → 16 pillars
    PillarStateVector result;
    decode_pillar_vector(membrane, result.pillars);

    // Clamp output to [0, 1]
    for (int i = 0; i < NumPillars; i++) {
        if (result.pillars[i] < 0.0f) result.pillars[i] = 0.0f;
        if (result.pillars[i] > 1.0f) result.pillars[i] = 1.0f;
    }

    return result;
}

// ── Tokenized Quantum Cognitive Pass ───────────────────────
// Same as quantum_cognitive_pass but first encodes the input
// PSV to its nearest WHT token, then uses the canonical token
// signal as the starting point. This demonstrates discrete-token
// processing through the quantum pipeline.
//
inline PillarStateVector quantum_cognitive_pass_tokenized(
    const PillarStateVector& input,
    NativeQuantumBackend& backend,
    const HopfProjectionMatrix& proj,
    const WHTTokenizer& tokenizer
) {
    // Step 1: Encode PSV → WHT token ID
    int token_id = tokenizer.encode(input);

    // Step 2: Decode token ID → canonical PSV
    PillarStateVector token_psv = tokenizer.decode(token_id);

    // Step 3: Angle-encode token PSV → 32 amplitudes
    std::vector<float> amps = encode_pillars_to_amplitudes(token_psv);

    // Steps 4-10: Same as quantum_cognitive_pass
    float wht_coeffs[WHT_N];
    {
        float in32[WHT_N] = {0};
        for (int i = 0; i < (int)amps.size() && i < WHT_N; i++)
            in32[i] = amps[i];
        backend.execute_wht(in32, wht_coeffs);
    }

    float base_frame[NumPillars];
    decode_pillar_vector(wht_coeffs, base_frame);

    float thought[HOPF_FIBER_DIM];
    for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
        float phase = 2.0f * 3.14159265f * (float)f / (float)HOPF_FRAME_COUNT;
        for (int p = 0; p < NumPillars; p++) {
            float modulation = 0.1f * std::sin(phase + (float)p * 0.2f);
            vn::fp20_t rotated = bloch_rotate(
                vn::fp20_t(base_frame[p]),
                vn::fp20_t(modulation)
            );
            thought[f * NumPillars + p] = rotated.to_float();
        }
    }

    normalize_thought(thought, HOPF_FIBER_DIM);

    float membrane[HOPF_BASE_DIM];
    proj.project(thought, membrane);

    PillarStateVector result;
    decode_pillar_vector(membrane, result.pillars);

    for (int i = 0; i < NumPillars; i++) {
        if (result.pillars[i] < 0.0f) result.pillars[i] = 0.0f;
        if (result.pillars[i] > 1.0f) result.pillars[i] = 1.0f;
    }

    return result;
}

}} // namespace vn::quantum
