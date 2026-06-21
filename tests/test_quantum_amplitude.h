#pragma once

#include "../quantum/amplitude_encoding.h"
#include "../include/Entity.h"
#include <cmath>
#include <cstdio>

namespace vn {
namespace tests {

static int test_amplitude_roundtrip() {
    int failures = 0;
    for (int i = 0; i < 100; ++i) {
        float raw = (float)i / 99.0f;
        fp20_t original(raw);
        float amp = vn::quantum::pillar_to_amplitude(original);
        float back = vn::quantum::amplitude_to_pillar(amp);
        fp20_t diff = original - fp20_t(back);
        if (diff < fp20_t(0)) diff = fp20_t(0) - diff;
        if (diff > fp20_t(0.001f)) {
            printf("FAIL amplitude_roundtrip[%d]: %.6f -> amp %.6f -> %.6f (err %.6f)\n",
                   i, (float)original, amp, back, (float)diff);
            ++failures;
        }
    }
    if (failures == 0)
        printf("PASS: amplitude_roundtrip (100 values)\n");
    return failures;
}

static int test_angle_encoding_roundtrip() {
    int failures = 0;
    PillarStateVector psv;
    for (int i = 0; i < NumPillars; ++i) {
        psv.pillars[i] = fp20_t(float(i) / float(NumPillars - 1));
    }
    std::vector<float> amps = vn::quantum::encode_pillars_to_amplitudes(psv);
    PillarStateVector decoded = vn::quantum::decode_amplitudes_to_pillars(amps, NumPillars);
    for (int i = 0; i < NumPillars; ++i) {
        fp20_t diff = fp20_t(psv.pillars[i]) - fp20_t(decoded.pillars[i]);
        if (diff < fp20_t(0)) diff = fp20_t(0) - diff;
        if (diff > fp20_t(0.02f)) {
            printf("FAIL angle_encoding[%d]: %.4f -> decoded %.4f (err %.4f)\n",
                   i, psv.pillars[i], decoded.pillars[i], diff.to_float());
            ++failures;
        }
    }
    if (failures == 0)
        printf("PASS: angle_encoding round-trip (16 pillars)\n");
    return failures;
}

static int test_complex_encoding_roundtrip() {
    int failures = 0;
    PillarStateVector psv;
    for (int i = 0; i < NumPillars; ++i) {
        psv.pillars[i] = fp20_t(float(i) / float(NumPillars - 1));
    }
    auto amps = vn::quantum::encode_pillars_to_complex(psv);
    PillarStateVector decoded = vn::quantum::decode_complex_to_pillars(amps, NumPillars);
    for (int i = 0; i < NumPillars; ++i) {
        fp20_t diff = fp20_t(psv.pillars[i]) - fp20_t(decoded.pillars[i]);
        if (diff < fp20_t(0)) diff = fp20_t(0) - diff;
        if (diff > fp20_t(0.02f)) {
            printf("FAIL complex_encoding[%d]: %.4f -> decoded %.4f (err %.4f)\n",
                   i, psv.pillars[i], decoded.pillars[i], diff.to_float());
            ++failures;
        }
    }
    if (failures == 0)
        printf("PASS: complex_encoding round-trip (16 pillars)\n");
    return failures;
}

static int test_qubit_norm() {
    int failures = 0;
    PillarStateVector psv;
    for (int i = 0; i < NumPillars; ++i) {
        psv.pillars[i] = fp20_t(float(i) / float(NumPillars - 1));
    }
    std::vector<float> amps = vn::quantum::encode_pillars_to_amplitudes(psv);
    // Each pair of amplitudes [cos(θ/2), sin(θ/2)] should have L2 norm = 1
    for (int i = 0; i < NumPillars; ++i) {
        float a0 = amps[2 * i];
        float a1 = amps[2 * i + 1];
        float norm2 = a0 * a0 + a1 * a1;
        if (std::abs(norm2 - 1.0f) > 0.001f) {
            printf("FAIL qubit_norm[%d]: norm2 = %.6f\n", i, norm2);
            ++failures;
        }
    }
    if (failures == 0)
        printf("PASS: qubit_norm (16 qubits, each L2=1)\n");
    return failures;
}

static int test_quantum_amplitude() {
    int failures = 0;
    failures += test_amplitude_roundtrip();
    failures += test_angle_encoding_roundtrip();
    failures += test_complex_encoding_roundtrip();
    failures += test_qubit_norm();
    if (failures == 0)
        printf("ALL quantum amplitude tests PASS\n");
    else
        printf("quantum amplitude tests: %d FAILURES\n", failures);
    return failures;
}

} // namespace tests
} // namespace vn

inline void run_quantum_amplitude_tests() {
    vn::tests::test_quantum_amplitude();
}
