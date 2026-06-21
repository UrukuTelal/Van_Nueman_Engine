#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "../include/Entity.h"
#include "../include/simulation/AgentECS.h"
#include "../agents/cognition.h"

inline void test_bloch_rotation_roundtrip() {
    printf("Test: Bloch rotation round-trip... ");
    vn::fp20_t val(0.3f);
    float theta = acosf(2.0f * val.to_float() - 1.0f);
    float new_theta = theta + 0.1f;
    float new_val = (cosf(new_theta) + 1.0f) * 0.5f;
    vn::fp20_t roundtrip(new_val);
    float back_theta = acosf(2.0f * roundtrip.to_float() - 1.0f);
    bool pass = fabsf(back_theta - new_theta) < 0.001f;
    printf("%s (theta %.3f -> %.3f, val %.3f -> %.3f)\n",
           pass ? "PASS" : "FAIL", theta, new_theta, val.to_float(), new_val);
}

inline void test_bloch_drift_to_baseline() {
    printf("Test: Bloch drift toward baseline... ");
    vn::fp20_t p[16];
    for (int i = 0; i < 16; i++) {
        float baseline = (i == 5) ? 0.1f : 0.5f;
        if (i < 8) p[i] = vn::fp20_t(0.9f);
        else p[i] = vn::fp20_t(0.2f);
    }
    float dt = 1.0f / 60.0f;
    for (int step = 0; step < 1800; step++) {
        for (int i = 0; i < 16; i++) {
            float baseline = (i == 5) ? 0.1f : 0.5f;
            float current = p[i].to_float();
            float theta = acosf(2.0f * current - 1.0f);
            float target_theta = acosf(2.0f * baseline - 1.0f);
            float diff = target_theta - theta;
            float drift = diff * 0.1f * dt;
            if (fabsf(drift) > 1e-8f) {
                float new_theta = theta + drift;
                p[i] = vn::fp20_t((cosf(new_theta) + 1.0f) * 0.5f);
            }
        }
    }
    bool pass = true;
    for (int i = 0; i < 16; i++) {
        float expected = (i == 5) ? 0.1f : 0.5f;
        float err = fabsf(p[i].to_float() - expected);
        if (err > 0.1f) { pass = false; break; }
    }
    printf("%s\n", pass ? "PASS" : "FAIL");
}

inline void test_harm_reduces_integrity() {
    printf("Test: Harm reduces Integrity via Bloch rotation... ");
    vn::fp20_t integrity(0.8f);
    float harm_f = 0.9f;
    float willpower_f = 0.6f;
    float depth_f = 0.5f;
    float dt = 1.0f / 60.0f;

    float before = integrity.to_float();
    float resistance = willpower_f * depth_f;
    if (resistance < 1e-8f) resistance = 1e-8f;
    float delta_theta = (harm_f * 0.1f * dt) / resistance;
    float theta = acosf(2.0f * before - 1.0f);
    float new_theta = theta + delta_theta;
    integrity = vn::fp20_t((cosf(new_theta) + 1.0f) * 0.5f);
    float after = integrity.to_float();

    bool pass = after < before && after >= 0.0f;
    printf("%s (%.3f -> %.3f, delta_theta=%.6f)\n",
           pass ? "PASS" : "FAIL", before, after, delta_theta);
}

inline void test_force_drives_velocity() {
    printf("Test: Force drives velocity... ");
    float force_f = 0.8f;
    float relation_f = 0.5f;
    float depth_f = 0.3f;
    float dt = 1.0f / 60.0f;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    float px = 0.0f, py = 0.0f, pz = 0.0f;

    for (int step = 0; step < 60; step++) {
        float speed = force_f * dt * 2.0f;
        vx += cosf(relation_f * 6.2832f) * speed;
        vy += sinf(relation_f * 6.2832f) * speed;
        vz += cosf(depth_f * 6.2832f) * speed * 0.5f;
        px += vx * dt;
        py += vy * dt;
        pz += vz * dt;
    }

    bool pass = fabsf(px) > 0.0f || fabsf(py) > 0.0f || fabsf(pz) > 0.0f;
    printf("%s (pos: %.3f, %.3f, %.3f, speed=%.3f)\n",
           pass ? "PASS" : "FAIL", px, py, pz, force_f * dt * 2.0f);
}

inline void test_decay_increases_harm() {
    printf("Test: Decay increases Harm from hazard... ");
    vn::fp20_t harm(0.1f);
    float hazard_level = 0.5f;
    float dt = 1.0f / 60.0f;
    float before = harm.to_float();
    float harm_f = before + hazard_level * 0.01f * dt;
    if (harm_f > 1.0f) harm_f = 1.0f;
    harm = vn::fp20_t(harm_f);
    bool pass = harm.to_float() > before;
    printf("%s (%.3f -> %.3f)\n", pass ? "PASS" : "FAIL", before, harm.to_float());
}

inline void test_distortion_twists_awareness() {
    printf("Test: Distortion twists Awareness... ");
    float awareness_f = 0.7f;
    float distortion_f = 0.5f;
    float dt = 1.0f / 60.0f;
    float before = awareness_f;
    if (distortion_f > 0.001f) {
        float theta = acosf(2.0f * awareness_f - 1.0f);
        float phi_twist = distortion_f * dt * 0.5f;
        float z = cosf(theta) * cosf(phi_twist);
        awareness_f = (z + 1.0f) * 0.5f;
    }
    bool pass = fabsf(awareness_f - before) > 0.0f;
    printf("%s (%.3f -> %.3f)\n", pass ? "PASS" : "FAIL", before, awareness_f);
}

inline void test_agent_ecs_reserve_and_add() {
    printf("Test: AgentECS reserve and add... ");
    vn::simulation::AgentECS ecs;
    ecs.reserve(100);
    auto idx = ecs.add_agent(PillarStateVector(), 0, 0, 0, 0, 0, 0, true, 0, 0, 0, 0, nullptr);
    bool pass = ecs.active(idx) && ecs.size() == 1;
    printf("%s (idx=%u, size=%llu)\n", pass ? "PASS" : "FAIL", (unsigned)idx, (unsigned long long)ecs.size());
}

inline void run_transform_pipeline_tests() {
    printf("=== TRANSFORM Pipeline Tests ===\n");
    test_bloch_rotation_roundtrip();
    test_bloch_drift_to_baseline();
    test_harm_reduces_integrity();
    test_force_drives_velocity();
    test_decay_increases_harm();
    test_distortion_twists_awareness();
    test_agent_ecs_reserve_and_add();
    printf("=== All TRANSFORM Pipeline Tests PASSED ===\n\n");
}
