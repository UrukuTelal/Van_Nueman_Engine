// BlochMath.h — Canonical CPU Bloch sphere math for pillar value mapping.
//
// Maps pillar value v ∈ [0,1] to Bloch angle θ ∈ [0,π] and back using
// the symmetric folded mapping: θ = acos(2v - 1), v = (cos θ + 1)/2.
//
// This is the ONE canonical CPU implementation. All CPU code (Entity.h,
// Transform.h, TransformCore.h, HopfPID.h, cognition.cpp, tests, etc.)
// should include this header instead of inlining their own expansions.
//
// GPU kernels (CUDA __device__, GLSL, vncc SPIR-V targets) must maintain
// their own copies since they compile to different backends.
//
// Round-trip guarantee: theta_to_value(value_to_theta(v)) ≈ v  (within fp32)
// Verified by test_bloch_roundtrip in test_entity.h.

#pragma once

#include <cstdint>
#include <cmath>
#include "../vn/ScaledInt.h"

// ── Float overloads ───────────────────────────────────────────

inline float bloch_value_to_theta(float v) {
    if (v <= 0.0f) return 3.14159265f;
    if (v >= 1.0f) return 0.0f;
    return std::acos(2.0f * v - 1.0f);
}

inline float bloch_theta_to_value(float theta) {
    return (std::cos(theta) + 1.0f) * 0.5f;
}

inline float bloch_rotate(float current, float delta_theta) {
    float theta = bloch_value_to_theta(current);
    float new_theta = theta + delta_theta;
    return bloch_theta_to_value(new_theta);
}

// ── fp20_t overloads ──────────────────────────────────────────

inline vn::fp20_t bloch_value_to_theta(vn::fp20_t val) {
    return vn::fp20_t(bloch_value_to_theta(val.to_float()));
}

inline vn::fp20_t bloch_theta_to_value(vn::fp20_t theta) {
    return vn::fp20_t(bloch_theta_to_value(theta.to_float()));
}

inline vn::fp20_t bloch_rotate(vn::fp20_t current, vn::fp20_t delta_theta) {
    float t = bloch_value_to_theta(current.to_float());
    float dt = delta_theta.to_float();
    return vn::fp20_t(bloch_theta_to_value(t + dt));
}

// ── Distortion torsion ────────────────────────────────────────

inline float bloch_apply_distortion_torsion(float awareness, float distortion) {
    float theta = bloch_value_to_theta(awareness);
    float phi_twist = distortion * 3.14159265f;
    return bloch_theta_to_value(theta + phi_twist);
}

inline vn::fp20_t bloch_apply_distortion_torsion(vn::fp20_t awareness, vn::fp20_t distortion) {
    return vn::fp20_t(bloch_apply_distortion_torsion(awareness.to_float(), distortion.to_float()));
}

// ── Deprecated aliases (for backward compat) ──────────────────
// New code should use bloch_value_to_theta / bloch_theta_to_value / bloch_rotate directly.
// These will be removed in a future cleanup pass.

[[deprecated("Use bloch_value_to_theta() from BlochMath.h")]]
inline vn::fp20_t pillar_value_to_theta(vn::fp20_t val) { return bloch_value_to_theta(val); }

[[deprecated("Use bloch_theta_to_value() from BlochMath.h")]]
inline vn::fp20_t theta_to_pillar_value(vn::fp20_t theta) { return bloch_theta_to_value(theta); }

[[deprecated("Use bloch_rotate() from BlochMath.h")]]
inline vn::fp20_t apply_bloch_rotation(vn::fp20_t current, vn::fp20_t delta_theta) {
    return bloch_rotate(current, delta_theta);
}

[[deprecated("Use bloch_apply_distortion_torsion() from BlochMath.h")]]
inline vn::fp20_t apply_distortion_torsion(vn::fp20_t awareness, vn::fp20_t distortion) {
    return bloch_apply_distortion_torsion(awareness, distortion);
}
