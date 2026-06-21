#pragma once

#include <cstdio>
#include <cmath>
#include <cstring>
#include "../fll/include/ThoughtGlyphBridge.h"

using namespace vn::fll;

static int tg_pass = 0, tg_fail = 0;

#define TG_TEST(name, expr) do { \
    if (!(expr)) { \
        printf("  FAIL: %s\n", name); \
        tg_fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        tg_pass++; \
    } \
} while(0)

namespace vn {
namespace tests {

static int test_thought_to_embedding() {
    printf("test_thought_to_embedding\n");

    // Create a 512D thought vector with known pattern
    float thought[HOPF_FIBER_DIM];
    for (int i = 0; i < HOPF_FIBER_DIM; i++) {
        thought[i] = (float)i / (float)HOPF_FIBER_DIM;
    }

    float embedding[64];
    ThoughtGlyphBridge::thought_to_embedding(thought, embedding);

    // First 32 should match membrane directly
    for (int i = 0; i < 32 && i < HOPF_BASE_DIM; i++) {
        float diff = std::fabs(embedding[i] - thought[i]);
        TG_TEST("membrane element matches", diff < 0.001f);
    }

    // Next 32 should be stride-15 samples from fiber
    for (int i = 0; i < 32; i++) {
        int fiber_idx = HOPF_BASE_DIM + i * 15;
        if (fiber_idx < HOPF_FIBER_DIM) {
            float diff = std::fabs(embedding[32 + i] - thought[fiber_idx]);
            TG_TEST("fiber element matches stride-15", diff < 0.001f);
        }
    }

    return tg_fail;
}

static int test_psv_to_embedding() {
    printf("test_psv_to_embedding\n");

    PillarStateVector psv;
    for (int p = 0; p < NumPillars; p++) {
        psv.pillars[p] = (float)p / (float)(NumPillars - 1);
    }

    float embedding[64];
    ThoughtGlyphBridge::psv_to_embedding(psv, embedding);

    // First 16 should match PSV directly
    for (int p = 0; p < NumPillars; p++) {
        float diff = std::fabs(embedding[p] - psv.pillars[p]);
        TG_TEST("PSV element copied to embedding", diff < 0.001f);
    }

    // Remaining should be zero
    for (int p = NumPillars; p < 64; p++) {
        TG_TEST("PSV embedding padding is zero", embedding[p] == 0.0f);
    }

    return tg_fail;
}

static int test_build_from_embedding() {
    printf("test_build_from_embedding\n");

    ArenaAllocator arena;
    if (!arena.init(1024, 64, 65536)) {
        printf("  FAIL: arena init\n");
        return ++tg_fail;
    }

    float embedding[64] = {0};
    for (int i = 0; i < 64; i++) {
        embedding[i] = (float)i / 63.0f;
    }

    ThoughtGlyphBridge bridge;
    FractalNode* root = bridge.build_from_embedding(embedding, arena, 4);
    TG_TEST("root node created", root != nullptr);

    if (root) {
        TG_TEST("root depth is 0", root->depth == 0);
        TG_TEST("root has children", root->has_children());
        TG_TEST("root has non-zero geometry scale",
                root->geometry.scale > 0.0f);
        TG_TEST("root has valid symmetry_n",
                root->geometry.symmetry_n >= 1 &&
                root->geometry.symmetry_n <= 12);
        TG_TEST("root outer_radius > inner_radius",
                root->geometry.outer_radius > root->geometry.inner_radius);
    }

    arena.shutdown();
    return tg_fail;
}

static int test_build_from_psv() {
    printf("test_build_from_psv\n");

    ArenaAllocator arena;
    if (!arena.init(1024, 64, 65536)) {
        printf("  FAIL: arena init\n");
        return ++tg_fail;
    }

    PillarStateVector psv;
    for (int p = 0; p < NumPillars; p++) {
        psv.pillars[p] = (float)p / (float)(NumPillars - 1);
    }

    ThoughtGlyphBridge bridge;
    FractalNode* root = bridge.build_from_psv(psv, arena, 4);
    TG_TEST("root built from PSV", root != nullptr);

    if (root) {
        TG_TEST("root depth 0 from PSV", root->depth == 0);
        TG_TEST("root geometry valid from PSV",
                root->geometry.symmetry_n >= 1);
    }

    arena.shutdown();
    return tg_fail;
}

static int test_rebuild_after_reset() {
    printf("test_rebuild_after_reset\n");

    ArenaAllocator arena;
    if (!arena.init(1024, 64, 65536)) {
        printf("  FAIL: arena init\n");
        return ++tg_fail;
    }

    PillarStateVector psv;
    psv.fill(0.3f);

    ThoughtGlyphBridge bridge;

    // First build
    FractalNode* root1 = bridge.build_from_psv(psv, arena, 4);
    TG_TEST("first build succeeds", root1 != nullptr);
    uint64_t count1 = arena.node_count();

    // Reset and rebuild
    arena.reset();
    TG_TEST("arena reset node_count to 0", arena.node_count() == 0);

    FractalNode* root2 = bridge.build_from_psv(psv, arena, 4);
    TG_TEST("rebuild after reset succeeds", root2 != nullptr);
    uint64_t count2 = arena.node_count();

    TG_TEST("rebuild produces same node count",
            count1 == count2);

    arena.shutdown();
    return tg_fail;
}

static int test_update_root_from_thought() {
    printf("test_update_root_from_thought\n");

    ArenaAllocator arena;
    if (!arena.init(1024, 64, 65536)) {
        printf("  FAIL: arena init\n");
        return ++tg_fail;
    }

    ThoughtGlyphBridge bridge;

    // Build initial tree
    {
        PillarStateVector psv;
        psv.fill(0.5f);
        FractalNode* root = bridge.build_from_psv(psv, arena, 4);
        TG_TEST("initial tree built", root != nullptr);

        if (root) {
            // Create a new thought vector
            float thought[HOPF_FIBER_DIM];
            for (int i = 0; i < HOPF_FIBER_DIM; i++) {
                thought[i] = (float)i / (float)HOPF_FIBER_DIM;
            }

            // Save old geometry and update
            vn::fll::GeometricCoefficients oldGeo = root->geometry;
            bridge.update_root_from_thought(root, thought);

            // Verify geometry changed (different scales at minimum)
            TG_TEST("root scale changed after update",
                    std::fabs(root->geometry.scale - oldGeo.scale) > 0.0001f ||
                    std::fabs(root->geometry.outer_radius - oldGeo.outer_radius) > 0.0001f);

            TG_TEST("root children preserved after update",
                    root->has_children());

            TG_TEST("updated symmetry valid",
                    root->geometry.symmetry_n >= 1 &&
                    root->geometry.symmetry_n <= 12);
        }
    }

    arena.shutdown();
    return tg_fail;
}

static int test_thought_glyph_bridge_tests() {
    printf("\n=== Thought Glyph Bridge Tests ===\n");

    tg_pass = 0;
    tg_fail = 0;

    test_thought_to_embedding();
    test_psv_to_embedding();
    test_build_from_embedding();
    test_build_from_psv();
    test_rebuild_after_reset();
    test_update_root_from_thought();

    printf("--- Thought Glyph Bridge: %d passed, %d failed ---\n",
           tg_pass, tg_fail);
    return tg_fail;
}

} // namespace tests
} // namespace vn
