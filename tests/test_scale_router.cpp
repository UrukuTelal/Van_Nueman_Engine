#include <cstdio>
#include <cmath>
#include <cassert>
#include "scale_router.h"
#include "../agents/cognition.h"

static int sr_tests_run = 0;
static int sr_tests_passed = 0;

#define SR_TEST(name) do { \
    sr_tests_run++; \
    printf("  %-45s ", name); \
} while(0)

#define SR_PASS() do { \
    sr_tests_passed++; \
    printf("PASS\n"); \
} while(0)

static void test_scale_coupling() {
    SR_TEST("ScaleRouter coupling decreases with delta");
    float c0 = ScaleRouter::scale_coupling(0);
    float c10 = ScaleRouter::scale_coupling(10);
    float c30 = ScaleRouter::scale_coupling(30);
    float c_neg = ScaleRouter::scale_coupling(-10);

    assert(c0 > 0.0f);
    assert(c10 < c0);
    assert(c30 < c10);
    assert(std::abs(c10 - c_neg) < 0.001f);
    SR_PASS();
}

static void test_interaction_radius() {
    SR_TEST("ScaleRouter interaction radius grows with scale");
    float r0 = ScaleRouter::scale_interaction_radius(SCALE_ATOM);
    float r30 = ScaleRouter::scale_interaction_radius(SCALE_ORGANISM);
    float r58 = ScaleRouter::scale_interaction_radius(SCALE_GALAXY);

    assert(r0 > 0.0f);
    assert(r30 > r0);
    assert(r58 > r30);
    SR_PASS();
}

static void test_route_computation() {
    SR_TEST("ScaleRouter route computation");
    ScaleRouter router;
    ScaleRoute r = router.route(SCALE_ORGANISM, SCALE_SOCIETY);

    assert(r.source_scale == SCALE_ORGANISM);
    assert(r.target_scale == SCALE_SOCIETY);
    assert(r.delta == -10);
    assert(r.coupling_coefficient > 0.0f);
    assert(r.attention_attenuation > 0.0f);
    assert(r.attention_attenuation < 1.0f);
    SR_PASS();
}

static void test_route_pillars() {
    SR_TEST("ScaleRouter route_pillars updates pillars");
    vn::simulation::AgentECS ecs;
    PillarStateVector psv;
    psv.fill(0.5f);
    auto idx = ecs.add_agent(psv, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0, 0.0f, 0.0f, 0.0f,
                  std::make_unique<AgentCognition>(0));

    ScaleRouter router;
    router.set_entity_scale(idx, SCALE_ORGANISM);
    router.build_routing_table();
    router.route_pillars(ecs, SCALE_ORGANISM, 0.016f);

    float c = static_cast<float>(ecs.pillar(idx, Cohesion));
    assert(c > 0.0f);
    assert(c <= 1.0f);
    SR_PASS();
}

static void test_cross_route() {
    SR_TEST("ScaleRouter cross_route between scales");
    vn::simulation::AgentECS ecs;

    PillarStateVector psv_organism;
    psv_organism.fill(0.5f);
    psv_organism[Force] = 0.8f;
    psv_organism[Influence] = 0.7f;
    auto org_idx = ecs.add_agent(psv_organism, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0, 0.0f, 0.0f, 0.0f,
                  std::make_unique<AgentCognition>(0));

    PillarStateVector psv_society;
    psv_society.fill(0.5f);
    psv_society[Resistance] = 0.9f;
    auto soc_idx = ecs.add_agent(psv_society, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0, 0.0f, 0.0f, 0.0f,
                  std::make_unique<AgentCognition>(1));

    ScaleRouter router;
    router.set_entity_scale(org_idx, SCALE_ORGANISM);
    router.set_entity_scale(soc_idx, SCALE_SOCIETY);
    router.build_routing_table();
    router.cross_route_entities(ecs, SCALE_ORGANISM, SCALE_SOCIETY, 0.016f);

    float val = static_cast<float>(ecs.pillar(soc_idx, Force));
    assert(val >= 0.0f);
    assert(val <= 1.0f);
    SR_PASS();
}

static void test_route_all() {
    SR_TEST("ScaleRouter route_all full pipeline");
    vn::simulation::AgentECS ecs;
    ScaleRouter router;

    for (int s = 0; s <= 60; s += 10) {
        PillarStateVector psv;
        psv.fill(0.5f);
        auto idx = ecs.add_agent(psv, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0, 0.0f, 0.0f, 0.0f,
                                 std::make_unique<AgentCognition>(s));
        router.set_entity_scale(idx, static_cast<ScaleExponent>(s));
    }

    router.build_routing_table();
    router.route_all(ecs, 0.016f);

    for (vn::simulation::AgentECS::Index i = 0; i < ecs.size(); ++i) {
        if (!ecs.active(i)) continue;
        for (int p = 0; p < NumPillars; ++p) {
            float val = static_cast<float>(ecs.pillar(i, static_cast<PillarIndex>(p)));
            assert(val >= 0.0f);
            assert(val <= 1.0f);
        }
    }

    assert(router.get_routing_table().active_scale_count > 0);
    SR_PASS();
}

static void test_scale_groups() {
    SR_TEST("ScaleRouter groups entities by scale");
    vn::simulation::AgentECS ecs;
    ScaleRouter router;

    for (int i = 0; i < 3; i++) {
        PillarStateVector psv;
        psv.fill(0.5f);
        auto idx = ecs.add_agent(psv, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0, 0.0f, 0.0f, 0.0f,
                                 std::make_unique<AgentCognition>(i));
        router.set_entity_scale(idx, SCALE_ORGANISM);
    }
    for (int i = 0; i < 2; i++) {
        PillarStateVector psv;
        psv.fill(0.5f);
        auto idx = ecs.add_agent(psv, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, 0, 0.0f, 0.0f, 0.0f,
                                 std::make_unique<AgentCognition>(3 + i));
        router.set_entity_scale(idx, SCALE_SOCIETY);
    }

    router.build_routing_table();

    const auto& table = router.get_routing_table();
    assert(table.active_scale_count == 2);
    SR_PASS();
}

static void test_entity_scale_api() {
    SR_TEST("ScaleRouter entity scale get/set");
    ScaleRouter router;
    router.set_entity_scale(0, SCALE_ATOM);
    router.set_entity_scale(1, SCALE_GALAXY);

    assert(router.get_entity_scale(0) == SCALE_ATOM);
    assert(router.get_entity_scale(1) == SCALE_GALAXY);
    assert(router.get_entity_scale(999) == SCALE_ORGANISM);
    SR_PASS();
}

inline void run_scale_router_tests() {
    printf("\n=== Scale Router Tests (Scale-Invariant) ===\n");
    test_scale_coupling();
    test_interaction_radius();
    test_route_computation();
    test_route_pillars();
    test_cross_route();
    test_route_all();
    test_scale_groups();
    test_entity_scale_api();
    printf("--- Scale Router: %d passed, %d failed ---\n", sr_tests_passed, sr_tests_run - sr_tests_passed);
}
