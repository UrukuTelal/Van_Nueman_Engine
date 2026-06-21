#pragma once

#include "../quantum/bcc_thought_env.h"
#include "../quantum/thought_engine.h"
#include <cstdio>
#include <cmath>
#include <cstring>

namespace vn {
namespace tests {

static int test_bcc_basic_store_load() {
    int failures = 0;
    vn::quantum::BCCThoughtEnvironment env;

    BCCCoord coord = {5, 7, 3, 1};
    PillarStateVector psv;
    psv.fill(0.75f);

    env.store(coord, psv);
    PillarStateVector loaded = env.load(coord);

    for (int i = 0; i < NumPillars; i++) {
        float diff = std::fabs(loaded.pillars[i] - 0.75f);
        if (diff > 0.001f) {
            printf("FAIL bcc_store_load[%d]: %.4f vs 0.75\n", i, loaded.pillars[i]);
            ++failures;
        }
    }

    if (failures == 0)
        printf("PASS: bcc_basic_store_load\n");
    return failures;
}

static int test_bcc_invalid_coord() {
    int failures = 0;
    vn::quantum::BCCThoughtEnvironment env;

    // Out-of-bounds should return 0.5 fallback
    BCCCoord bad = {-1, -1, -1, 0};
    PillarStateVector loaded = env.load(bad);

    for (int i = 0; i < NumPillars; i++) {
        float diff = std::fabs(loaded.pillars[i] - 0.5f);
        if (diff > 0.001f) {
            printf("FAIL bcc_invalid_coord[%d]: %.4f vs 0.5\n", i, loaded.pillars[i]);
            ++failures;
        }
    }

    if (failures == 0)
        printf("PASS: bcc_invalid_coord\n");
    return failures;
}

static int test_bcc_agent_movement() {
    int failures = 0;
    vn::quantum::BCCThoughtEnvironment env;

    // Default position should be center
    BCCCoord center = env.agent_pos;
    if (center.i != 16 || center.j != 16 || center.k != 16) {
        printf("FAIL bcc_agent_movement: initial pos (%d,%d,%d) != (16,16,16)\n",
               center.i, center.j, center.k);
        ++failures;
    }

    // Store at agent, move, load at new pos should get fallback
    PillarStateVector psv;
    psv.fill(0.9f);
    env.store_at_agent(psv);

    env.move_by(1, 0, 0);
    PillarStateVector new_pos = env.load_at_agent();

    // New pos should be default (0.5), not 0.9
    for (int i = 0; i < NumPillars; i++) {
        if (std::fabs(new_pos.pillars[i] - 0.5f) > 0.1f) {
            printf("FAIL bcc_agent_movement[%d]: %.4f vs 0.5\n", i, new_pos.pillars[i]);
            ++failures;
        }
    }

    // Move back should retrieve stored value
    env.move_by(-1, 0, 0);
    PillarStateVector back = env.load_at_agent();
    for (int i = 0; i < NumPillars; i++) {
        float diff = std::fabs(back.pillars[i] - 0.9f);
        if (diff > 0.001f) {
            printf("FAIL bcc_agent_movement back[%d]: %.4f vs 0.9\n", i, back.pillars[i]);
            ++failures;
        }
    }

    if (failures == 0)
        printf("PASS: bcc_agent_movement\n");
    return failures;
}

static int test_bcc_scatter_gather() {
    int failures = 0;
    vn::quantum::BCCThoughtEnvironment env;

    // Fill env with 0.5 baseline
    env.clear();

    // Store a distinctive PSV at agent position
    PillarStateVector psv;
    psv.fill(1.0f);

    env.store_at_agent(psv);
    env.scatter(psv, 2);

    // Gather from a position 2 steps away — should get some influence
    env.move_by(2, 0, 0);
    PillarStateVector gathered = env.gather(2);

    // At distance 2, weight = 1 - 2/3 = 0.333, so gathered should be > 0.5
    float avg = 0.0f;
    for (int i = 0; i < NumPillars; i++) avg += gathered.pillars[i];
    avg /= (float)NumPillars;

    if (avg < 0.51f) {
        printf("FAIL bcc_scatter_gather: avg=%.4f (expected > 0.5)\n", avg);
        ++failures;
    }

    // Gather from far away should get 0.5 (no influence)
    env.move_to({0, 0, 0, 0});
    PillarStateVector far_gathered = env.gather(1);
    float far_avg = 0.0f;
    for (int i = 0; i < NumPillars; i++) far_avg += far_gathered.pillars[i];
    far_avg /= (float)NumPillars;

    if (std::fabs(far_avg - 0.5f) > 0.1f) {
        printf("FAIL bcc_scatter_gather far: avg=%.4f (expected ~0.5)\n", far_avg);
        ++failures;
    }

    if (failures == 0)
        printf("PASS: bcc_scatter_gather (near=%.4f, far=%.4f)\n", avg, far_avg);
    return failures;
}

static int test_bcc_thought_encoding() {
    int failures = 0;
    vn::quantum::BCCThoughtEnvironment env;
    vn::quantum::ThoughtEngine engine;

    // Build a known thought vector
    float thought[HOPF_FIBER_DIM];
    for (int i = 0; i < HOPF_FIBER_DIM; i++) {
        thought[i] = 0.5f + 0.3f * std::sin((float)i * 0.1f);
    }

    env.encode_thought(thought);

    // Verify the grid has been populated (check a few cells)
    float sum = 0.0f;
    for (int p = 0; p < NumPillars; p++) {
        sum += env.at(p, 0, 0).pillars[p];
    }
    if (sum < 0.1f) {
        printf("FAIL bcc_thought_encoding: grid values too low (sum=%.4f)\n", sum);
        ++failures;
    }

    // Connect env to engine and verify spatial tick works
    engine.set_bcc_env(&env);
    engine.state.init();

    PillarStateVector input;
    input.fill(0.5f);
    engine.init_from_psv(input);

    // Run a tick with spatial context
    engine.tick(0.016f);

    // After tick, env should have been updated
    PillarStateVector spatial_result = env.load_at_agent();
    float spatial_avg = 0.0f;
    for (int i = 0; i < NumPillars; i++) spatial_avg += spatial_result.pillars[i];
    spatial_avg /= (float)NumPillars;

    if (spatial_avg < 0.1f || spatial_avg > 1.0f) {
        printf("FAIL bcc_thought_encoding: spatial result avg=%.4f out of range\n", spatial_avg);
        ++failures;
    }

    if (failures == 0)
        printf("PASS: bcc_thought_encoding\n");
    return failures;
}

static int test_bcc_spatial_think() {
    int failures = 0;
    vn::quantum::BCCThoughtEnvironment env;
    vn::quantum::ThoughtEngine engine;

    // Connected spatial think
    // Prime the BCC grid with a non-default pattern so spatial context is meaningful
    PillarStateVector seed;
    seed.fill(0.9f);
    env.store_at_agent(seed);
    env.scatter(seed, 2);

    PillarStateVector input;
    for (int i = 0; i < NumPillars; i++)
        input.pillars[i] = (float)i / (float)(NumPillars - 1);

    PillarStateVector output = engine.think(input, 3, 0.016f, false, &env);

    // Output must be valid
    for (int i = 0; i < NumPillars; i++) {
        if (output.pillars[i] < 0.0f || output.pillars[i] > 1.0f) {
            printf("FAIL bcc_spatial_think[%d]: %.6f out of [0,1]\n",
                   i, output.pillars[i]);
            ++failures;
        }
    }

    // Env should have been populated
    PillarStateVector env_state = env.load_at_agent();
    float env_avg = 0.0f;
    for (int i = 0; i < NumPillars; i++) env_avg += env_state.pillars[i];
    env_avg /= (float)NumPillars;

    if (env_avg < 0.1f || env_avg > 1.0f) {
        printf("FAIL bcc_spatial_think: env state avg=%.4f out of range\n", env_avg);
        ++failures;
    }

    // Verify spatial env was populated by the think
    PillarStateVector after_think = env.load_at_agent();
    float after_avg = 0.0f;
    for (int i = 0; i < NumPillars; i++) after_avg += after_think.pillars[i];
    after_avg /= (float)NumPillars;
    if (after_avg < 0.1f || after_avg > 1.0f) {
        printf("FAIL bcc_spatial_think: post-think env avg=%.4f out of range\n", after_avg);
        ++failures;
    } else {
        printf("INFO: spatial_think env avg=%.4f (was primed with 0.9)\n", after_avg);
    }

    if (failures == 0)
        printf("PASS: bcc_spatial_think\n");
    return failures;
}

static int test_bcc_neighbors() {
    int failures = 0;
    vn::quantum::BCCThoughtEnvironment env;

    // Fill env
    env.clear();

    // Store different values at each neighbor direction
    env.store_at_agent({0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f,
                        0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f});

    env.move_by(1, 0, 0);
    env.store_at_agent({0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f,
                        0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f});
    env.move_by(-1, 0, 0);

    // Get neighbors
    PillarStateVector neighbors[6];
    env.get_neighbors(neighbors);

    // Neighbor[0] is +x, should be 0.1
    float n0_avg = 0.0f;
    for (int i = 0; i < NumPillars; i++) n0_avg += neighbors[0].pillars[i];
    n0_avg /= (float)NumPillars;

    if (std::fabs(n0_avg - 0.1f) > 0.01f) {
        printf("FAIL bcc_neighbors: +x neighbor avg=%.4f (expected 0.1)\n", n0_avg);
        ++failures;
    }

    if (failures == 0)
        printf("PASS: bcc_neighbors\n");
    return failures;
}

static int test_bcc_thought_env() {
    int failures = 0;
    failures += test_bcc_basic_store_load();
    failures += test_bcc_invalid_coord();
    failures += test_bcc_agent_movement();
    failures += test_bcc_scatter_gather();
    failures += test_bcc_thought_encoding();
    failures += test_bcc_spatial_think();
    failures += test_bcc_neighbors();
    if (failures == 0)
        printf("ALL BCC thought environment tests PASS\n");
    else
        printf("BCC thought environment tests: %d FAILURES\n", failures);
    return failures;
}

} // namespace tests
} // namespace vn

inline void run_bcc_thought_env_tests() {
    vn::tests::test_bcc_thought_env();
}
