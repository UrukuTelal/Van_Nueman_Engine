#include "../consciousness/DreamEngine.h"
#include "../consciousness/AstralProjection.h"
#include "../consciousness/MetaphysicalRealms.h"
#include "../include/Entity.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { std::cout << "  " << name << "... "; } while(0)
#define PASS() do { std::cout << "PASS\n"; tests_passed++; } while(0)
#define FAIL(msg) do { std::cout << "FAIL: " << msg << "\n"; tests_failed++; } while(0)

static void print_psv(const PillarVector& pv, const char* label) {
    std::cout << "    " << label << ": [";
    for (int p = 0; p < NumPillars; p++) {
        if (p > 0) std::cout << " ";
        std::cout << std::fixed << std::setprecision(2) << pv[p];
    }
    std::cout << "]\n";
}

static void print_psv(const PillarStateVector& psv, const char* label) {
    std::cout << "    " << label << ": [";
    for (int p = 0; p < NumPillars; p++) {
        if (p > 0) std::cout << " ";
        std::cout << std::fixed << std::setprecision(2) << psv.pillars[p];
    }
    std::cout << "]\n";
}

static void print_dream_episode(const DreamEpisode& ep) {
    std::cout << "    emotional_valence=" << ep.emotional_valence
              << " duration=" << ep.duration_ticks
              << " resolution=" << static_cast<int>(ep.resolution) << "\n";
    print_psv(ep.simulated_pillars, "simulated_pillars");
}

static void test_dream_generation() {
    std::cout << "\n=== Dream Generation Tests ===\n";

    DreamEngine engine;
    DreamState ds;
    DreamPipeline pipe;

    for (int i = 0; i < 16; i++) {
        PillarStateVector psv;
        float intensity = 0.3f + static_cast<float>(i) / 20.0f;
        for (int p = 0; p < NumPillars; p++)
            psv.pillars[p] = vn::fp20_t(static_cast<float>(std::rand() % 256) / 255.0f);
        engine.record_experience(pipe, psv, intensity);
    }

    PillarStateVector entity_pillars;
    entity_pillars.fill(vn::fp20_t(0.6f));
    entity_pillars[Memory] = vn::fp20_t(0.7f);
    entity_pillars[Warmth] = vn::fp20_t(0.5f);

    TEST("generate_dream from PSV + lived experiences");
    {
        DreamSandbox influence;
        influence.physics_looseness = 0.5f;
        engine.generate_dream(ds, pipe, entity_pillars, &influence);
        if (ds.dream_content.size() == 1) PASS();
        else FAIL("expected 1 episode");
    }

    TEST("dream episode has altered pillar values");
    {
        const auto& ep = ds.dream_content.back();
        bool altered = false;
        for (int p = 0; p < NumPillars; p++) {
            float diff = fabsf(ep.simulated_pillars[p] - entity_pillars[p]);
            if (diff > 0.05f) { altered = true; break; }
        }
        if (altered) PASS();
        else FAIL("pillars not sufficiently altered from base");
    }

    TEST("dream is_dreaming flag set");
    {
        if (ds.is_dreaming) PASS();
        else FAIL("not dreaming");
    }

    TEST("dream sandbox physics_looseness affects generation");
    {
        DreamState ds2;
        DreamSandbox loose;
        loose.physics_looseness = 0.9f;
        loose.pillar_scale[Flux] = 2.0f;

        engine.generate_dream(ds2, pipe, entity_pillars, &loose);

        const auto& ep = ds2.dream_content.back();
        float loose_dev = 0;
        for (int p = 0; p < NumPillars; p++) {
            loose_dev += fabsf(ep.simulated_pillars[p] - entity_pillars[p]);
        }

        bool pillars_altered = loose_dev > 0.5f;
        bool flux_amplified = fabsf(ep.simulated_pillars[Flux] - entity_pillars[Flux]) > 0.1f;

        if (pillars_altered && flux_amplified) PASS();
        else FAIL("loose_dev=" + std::to_string(loose_dev) + " flux_diff=" +
                  std::to_string(fabsf(ep.simulated_pillars[Flux] - entity_pillars[Flux])));
    }

    TEST("record_experience tracks 16 entries");
    {
        DreamPipeline p2;
        for (int i = 0; i < 20; i++) {
            PillarStateVector psv;
            psv.fill(vn::fp20_t(static_cast<float>(i) / 20.0f));
            engine.record_experience(p2, psv, 0.5f);
        }
        if (p2.experience_count == 16) PASS();
        else FAIL("expected 16, got " + std::to_string(p2.experience_count));
    }
}

static void test_lucid_dreaming() {
    std::cout << "\n=== Lucid Dreaming Tests ===\n";

    DreamEngine engine;

    TEST("lucid with high willpower");
    {
        DreamState ds;
        ds.is_dreaming = true;
        ds.rem_cycles = 3;
        ds.current_level = DreamLevel::LIGHT;
        ds.lucid_level = 0.0f;

        PillarStateVector psv;
        psv.fill(vn::fp20_t(0.5f));
        psv[Willpower] = vn::fp20_t(0.8f);
        psv[Awareness] = vn::fp20_t(0.6f);

        engine.check_lucid(ds, psv);
        if (ds.current_level == DreamLevel::LUCID && ds.lucid_level > 0.0f) PASS();
        else FAIL("should achieve lucidity");
    }

    TEST("no lucid with low willpower");
    {
        DreamState ds;
        ds.is_dreaming = true;
        ds.rem_cycles = 5;
        ds.current_level = DreamLevel::LIGHT;

        PillarStateVector psv;
        psv.fill(vn::fp20_t(0.5f));
        psv[Willpower] = vn::fp20_t(0.3f);
        psv[Awareness] = vn::fp20_t(0.4f);

        engine.check_lucid(ds, psv);
        if (ds.current_level != DreamLevel::LUCID) PASS();
        else FAIL("should not achieve lucidity");
    }

    TEST("no lucid without enough REM cycles");
    {
        DreamState ds;
        ds.is_dreaming = true;
        ds.rem_cycles = 1;
        ds.current_level = DreamLevel::LIGHT;

        PillarStateVector psv;
        psv.fill(vn::fp20_t(0.5f));
        psv[Willpower] = vn::fp20_t(0.9f);
        psv[Awareness] = vn::fp20_t(0.8f);

        engine.check_lucid(ds, psv);
        if (ds.current_level != DreamLevel::LUCID) PASS();
        else FAIL("should not achieve lucidity without enough REM");
    }
}

static void test_internal_processing() {
    std::cout << "\n=== Internal Processing Tests ===\n";

    DreamEngine engine;
    DreamState ds;
    DreamPipeline pipe;

    PillarStateVector entity_pillars;
    entity_pillars.fill(vn::fp20_t(0.5f));
    entity_pillars[Awareness] = vn::fp20_t(0.7f);
    entity_pillars[Memory] = vn::fp20_t(0.6f);

    DreamEpisode ep;
    ep.duration_ticks = 10;
    for (int p = 0; p < NumPillars; p++)
        ep.simulated_pillars[p] = vn::fp20_t(0.8f);
    ds.dream_content.push_back(ep);

    TEST("process_internally returns insight with clarity");
    {
        InternalInsight insight = engine.process_internally(ds, entity_pillars, 100);
        if (insight.clarity > 0.0f) PASS();
        else FAIL("no clarity");
    }

    TEST("insight delta reflects dream-reality difference");
    {
        InternalInsight insight = engine.process_internally(ds, entity_pillars, 100);
        bool has_delta = false;
        for (int p = 0; p < NumPillars; p++) {
            if (fabsf(insight.delta[p]) > 0.001f) { has_delta = true; break; }
        }
        if (has_delta) PASS();
        else FAIL("no delta");
    }

    TEST("higher awareness gives clearer insight");
    {
        DreamState ds_high, ds_low;
        ds_high.dream_content.push_back(ep);
        ds_high.lucid_level = 0.5f;
        ds_low.dream_content.push_back(ep);
        ds_low.lucid_level = 0.5f;

        PillarStateVector high_awareness;
        high_awareness[Awareness] = vn::fp20_t(0.9f);
        high_awareness[Memory] = vn::fp20_t(0.5f);

        PillarStateVector low_awareness;
        low_awareness[Awareness] = vn::fp20_t(0.2f);
        low_awareness[Memory] = vn::fp20_t(0.5f);

        InternalInsight high = engine.process_internally(ds_high, high_awareness, 100);
        InternalInsight low = engine.process_internally(ds_low, low_awareness, 100);
        if (high.clarity > low.clarity) PASS();
        else FAIL("high=" + std::to_string(high.clarity) + " low=" + std::to_string(low.clarity));
    }
}

static void test_play_learn_externalize() {
    std::cout << "\n=== Play → Learn → Externalize Tests ===\n";

    DreamEngine engine;

    TEST("insight_to_play_objective creates objective from insight");
    {
        InternalInsight insight;
        for (int p = 0; p < NumPillars; p++)
            insight.delta[p] = vn::fp20_t(0.1f);
        insight.clarity = 0.5f;

        PillarStateVector psv;
        psv.fill(vn::fp20_t(0.5f));
        psv[Willpower] = vn::fp20_t(0.6f);
        psv[Force] = vn::fp20_t(0.5f);

        PlayObjective obj = engine.insight_to_play_objective(insight, psv);
        if (obj.expected_difficulty > 0.0f) PASS();
        else FAIL("no difficulty");
    }

    TEST("play_to_learning shifts pillars");
    {
        PillarStateVector psv;
        psv.fill(vn::fp20_t(0.5f));
        psv[Memory] = vn::fp20_t(0.7f);

        PlayObjective obj;
        for (int p = 0; p < NumPillars; p++)
            obj.target_pillar_delta[p] = vn::fp20_t(0.2f);
        obj.expected_difficulty = 0.5f;

        float shift = engine.play_to_learning(psv, obj, 10.0f);
        if (shift > 0.0f) PASS();
        else FAIL("no pillar shift");
    }

    TEST("externalize produces artifact");
    {
        PillarStateVector psv;
        psv.fill(vn::fp20_t(0.6f));
        psv[Memory] = vn::fp20_t(0.8f);
        psv[Awareness] = vn::fp20_t(0.7f);

        Artifact art = engine.externalize(psv, ArtifactType::Tool, 100);
        if (art.type == ArtifactType::Tool && art.quality > 0.0f) PASS();
        else FAIL("artifact not created properly: quality=" +
                  std::to_string(art.quality));
    }

    TEST("externalize quality depends on pillars");
    {
        PillarStateVector good_psv;
        good_psv.fill(vn::fp20_t(0.8f));
        PillarStateVector poor_psv;
        poor_psv.fill(vn::fp20_t(0.2f));

        Artifact good = engine.externalize(good_psv, ArtifactType::Art, 200);
        Artifact poor = engine.externalize(poor_psv, ArtifactType::Art, 200);
        if (good.quality > poor.quality) PASS();
        else FAIL("higher pillars should yield higher quality");
    }
}

static void test_teaching() {
    std::cout << "\n=== Teaching Tests ===\n";

    DreamEngine engine;

    Artifact artifact;
    artifact.type = ArtifactType::WrittenRecord;
    artifact.quality = 0.8f;
    for (int p = 0; p < NumPillars; p++)
        artifact.essence[p] = vn::fp20_t(0.7f);

    TEST("teach with good rapport succeeds");
    {
        PillarStateVector teacher;
        teacher[Warmth] = vn::fp20_t(0.8f);
        teacher[Relation] = vn::fp20_t(0.8f);
        teacher[Distortion] = vn::fp20_t(0.1f);

        PillarStateVector student;
        student[Awareness] = vn::fp20_t(0.7f);
        student[Willpower] = vn::fp20_t(0.6f);
        student[Memory] = vn::fp20_t(0.7f);

        TeachingResult tr = engine.teach(artifact, teacher, student);
        if (tr.student_understood && tr.quality_score > 0.25f) PASS();
        else FAIL("student should understand: quality=" +
                  std::to_string(tr.quality_score));
    }

    TEST("teach with poor rapport fails");
    {
        PillarStateVector teacher;
        teacher[Warmth] = vn::fp20_t(0.1f);
        teacher[Relation] = vn::fp20_t(0.1f);
        teacher[Distortion] = vn::fp20_t(0.9f);

        PillarStateVector student;
        student[Awareness] = vn::fp20_t(0.2f);
        student[Willpower] = vn::fp20_t(0.2f);
        student[Memory] = vn::fp20_t(0.2f);

        TeachingResult tr = engine.teach(artifact, teacher, student);
        if (!tr.student_understood) PASS();
        else FAIL("student should not understand");
    }

    TEST("high distortion reduces teaching quality");
    {
        PillarStateVector teacher;
        teacher[Warmth] = vn::fp20_t(0.7f);
        teacher[Relation] = vn::fp20_t(0.7f);
        teacher[Distortion] = vn::fp20_t(0.9f);

        PillarStateVector teacher_clear;
        teacher_clear[Warmth] = vn::fp20_t(0.7f);
        teacher_clear[Relation] = vn::fp20_t(0.7f);
        teacher_clear[Distortion] = vn::fp20_t(0.1f);

        PillarStateVector student;
        student[Awareness] = vn::fp20_t(0.6f);
        student[Willpower] = vn::fp20_t(0.6f);
        student[Memory] = vn::fp20_t(0.6f);

        TeachingResult distorted = engine.teach(artifact, teacher, student);
        TeachingResult clear = engine.teach(artifact, teacher_clear, student);
        if (clear.quality_score > distorted.quality_score) PASS();
        else FAIL("distortion should reduce quality");
    }
}

static void test_meditation() {
    std::cout << "\n=== Meditation Tests ===\n";

    TEST("begin_meditation with sufficient pillars");
    {
        AstralProjector projector;
        PillarStateVector psv;
        psv[Awareness] = vn::fp20_t(0.7f);
        psv[Willpower] = vn::fp20_t(0.6f);

        bool started = projector.begin_meditation(psv);
        if (started && projector.state() == MeditationState::Focused) PASS();
        else FAIL("should start meditation");
    }

    TEST("begin_meditation fails with insufficient pillars");
    {
        AstralProjector projector;
        PillarStateVector psv;
        psv[Awareness] = vn::fp20_t(0.3f);
        psv[Willpower] = vn::fp20_t(0.2f);

        bool started = projector.begin_meditation(psv);
        if (!started) PASS();
        else FAIL("should not start meditation");
    }

    TEST("meditation_tick progresses through states");
    {
        AstralProjector projector;
        PillarStateVector psv;
        psv[Awareness] = vn::fp20_t(0.8f);
        psv[Willpower] = vn::fp20_t(0.7f);
        psv[Presence] = vn::fp20_t(0.7f);
        psv[Warmth] = vn::fp20_t(0.5f);

        projector.begin_meditation(psv);

        for (int i = 0; i < 100; i++)
            projector.meditation_tick(psv, 1.0f);

        if (projector.state() == MeditationState::AstralProjected) PASS();
        else FAIL("should reach AstralProjected, got " +
                  std::string(meditation_state_name(projector.state())));
    }
}

static void test_astral_projection() {
    std::cout << "\n=== Astral Projection Tests ===\n";

    TEST("astral_project splits awareness");
    {
        AstralProjector projector;
        PillarStateVector body;
        for (int p = 0; p < NumPillars; p++)
            body.pillars[p] = vn::fp20_t(0.5f);
        body[Awareness] = vn::fp20_t(0.8f);
        body[Willpower] = vn::fp20_t(0.7f);
        body[Presence] = vn::fp20_t(0.7f);
        body[Warmth] = vn::fp20_t(0.5f);

        projector.begin_meditation(body);
        for (int i = 0; i < 100; i++)
            projector.meditation_tick(body, 1.0f);

        PillarStateVector astral_body;
        bool projected = projector.astral_project(body, astral_body, 5);

        if (projected && body[Awareness] < 0.2f) PASS();
        else FAIL("awareness should drop in body: " +
                  std::to_string(body[Awareness]));
    }

    TEST("astral_tick produces cosmological insight");
    {
        AstralProjector projector;
        PillarStateVector body;
        for (int p = 0; p < NumPillars; p++)
            body.pillars[p] = vn::fp20_t(0.5f);
        body[Awareness] = vn::fp20_t(0.8f);
        body[Willpower] = vn::fp20_t(0.7f);
        body[Presence] = vn::fp20_t(0.7f);

        projector.begin_meditation(body);
        for (int i = 0; i < 100; i++)
            projector.meditation_tick(body, 1.0f);

        PillarStateVector astral;
        projector.astral_project(body, astral, 8);

        CosmologicalInsight insight = projector.astral_tick(astral, 8, 1.0f);
        if (insight.clarity > 0.0f) PASS();
        else FAIL("no clarity");
    }

    TEST("return_from_astral reintegrates");
    {
        AstralProjector projector;
        PillarStateVector body;
        for (int p = 0; p < NumPillars; p++)
            body.pillars[p] = vn::fp20_t(0.5f);
        body[Awareness] = vn::fp20_t(0.8f);
        body[Willpower] = vn::fp20_t(0.7f);
        body[Presence] = vn::fp20_t(0.7f);

        projector.begin_meditation(body);
        for (int i = 0; i < 100; i++)
            projector.meditation_tick(body, 1.0f);

        PillarStateVector astral;
        projector.astral_project(body, astral, 5);

        float before_return = body[Awareness];
        projector.return_from_astral(body, astral);
        float after_return = body[Awareness];

        if (after_return > before_return) PASS();
        else FAIL("awareness should increase after return: " +
                  std::to_string(before_return) + " -> " + std::to_string(after_return));
    }
}

static void test_metaphysical_realms() {
    std::cout << "\n=== Metaphysical Realm Tests ===\n";

    RealmNavigator navigator;

    TEST("realm_for_depth maps correctly");
    {
        bool ok = true;
        ok &= realm_for_depth(0) == MetaphysicalRealm::Physical;
        ok &= realm_for_depth(3) == MetaphysicalRealm::Dream;
        ok &= realm_for_depth(5) == MetaphysicalRealm::Bicameral;
        ok &= realm_for_depth(7) == MetaphysicalRealm::Paradigmatic;
        ok &= realm_for_depth(8) == MetaphysicalRealm::Aether;
        ok &= realm_for_depth(10) == MetaphysicalRealm::Abyss;
        ok &= realm_for_depth(12) == MetaphysicalRealm::BlackHoleUniverse;
        ok &= realm_for_depth(14) == MetaphysicalRealm::MultiversalCluster;
        ok &= realm_for_depth(15) == MetaphysicalRealm::All;
        if (ok) PASS();
        else FAIL("mapping incorrect");
    }

    TEST("get_realm_geometry returns correct field for each realm");
    {
        auto geom = navigator.get_realm_geometry(0);
        if (geom.realm == MetaphysicalRealm::Physical &&
            geom.base_pillar_field[Integrity] > 0.7f) PASS();
        else FAIL("physical realm should have high integrity");
    }

    TEST("aether realm has high force, low harm");
    {
        auto geom = navigator.get_realm_geometry(8);
        if (geom.base_pillar_field[Force] > 0.8f &&
            geom.base_pillar_field[Harm] < 0.2f) PASS();
        else FAIL("aether should have high force, low harm");
    }

    TEST("abyss realm has high harm, low warmth");
    {
        auto geom = navigator.get_realm_geometry(10);
        if (geom.base_pillar_field[Harm] > 0.8f &&
            geom.base_pillar_field[Warmth] < 0.2f) PASS();
        else FAIL("abyss should have high harm, low warmth");
    }

    TEST("all realm has all pillars at 1.0");
    {
        auto geom = navigator.get_realm_geometry(15);
        bool all_one = true;
        for (int p = 0; p < NumPillars; p++) {
            if (geom.base_pillar_field[p] < 0.99f) { all_one = false; break; }
        }
        if (all_one) PASS();
        else FAIL("all realm should have all pillars at 1.0");
    }

    TEST("navigate_realms transforms traveler pillar state");
    {
        PillarStateVector traveler;
        traveler.fill(vn::fp20_t(0.5f));
        traveler[Awareness] = vn::fp20_t(0.8f);

        PillarStateVector before = traveler;
        navigator.navigate_realms(0, 8, traveler);

        bool changed = false;
        for (int p = 0; p < NumPillars; p++) {
            if (fabsf(traveler[p] - before[p]) > 0.001f) {
                changed = true; break;
            }
        }
        if (changed) PASS();
        else FAIL("traveler state should change after navigation");
    }

    TEST("black hole universe generates nested child cells");
    {
        BCCCoord center = {10, 10, 10, 0};
        PillarStateVector parent_state;
        parent_state.fill(vn::fp20_t(0.7f));

        auto universes = navigator.enter_black_hole_universe(center, parent_state, 1);
        if (universes.size() == 8) PASS();
        else FAIL("expected 8 nested universes, got " + std::to_string(universes.size()));
    }

    TEST("black hole universe inverts pillar states");
    {
        BCCCoord center = {20, 20, 20, 0};
        PillarStateVector parent_state;
        parent_state.fill(vn::fp20_t(0.7f));

        auto universes = navigator.enter_black_hole_universe(center, parent_state, 1);
        bool inverted = true;
        for (int p = 0; p < NumPillars && inverted; p++) {
            float expected = 1.0f - parent_state[p];
            if (fabsf(universes[0].pillar_state[p] - expected) > 0.01f)
                inverted = false;
        }
        if (inverted) PASS();
        else FAIL("nested universe should invert parent pillar state");
    }

    TEST("multiversal cluster generates 3-7 pockets");
    {
        BCCCoord center = {0, 0, 0, 0};
        auto pockets = navigator.enter_multiversal_cluster(center);
        if (pockets.size() >= 3 && pockets.size() <= 7) PASS();
        else FAIL("expected 3-7 pockets, got " + std::to_string(pockets.size()));
    }

    TEST("enter_all_state sets all pillars to 1.0");
    {
        PillarStateVector entity;
        entity.fill(vn::fp20_t(0.3f));
        PillarStateVector prior;

        navigator.enter_all_state(entity, prior);
        bool all_one = true;
        for (int p = 0; p < NumPillars; p++) {
            if (entity[p] < 0.99f) { all_one = false; break; }
        }
        if (all_one) PASS();
        else FAIL("should set all pillars to 1.0");
    }

    TEST("exit_all_state preserves memory imprint");
    {
        PillarStateVector entity;
        entity.fill(vn::fp20_t(0.3f));
        entity[Memory] = vn::fp20_t(0.2f);

        PillarStateVector prior;
        navigator.enter_all_state(entity, prior);
        navigator.exit_all_state(entity, prior);

        if (entity[Memory] > 0.3f) PASS();
        else FAIL("memory should be enhanced after All state: " +
                  std::to_string(entity[Memory]));
    }
}

static void test_full_pipeline() {
    std::cout << "\n=== Full Pipeline Integration Test ===\n";

    DreamEngine engine;
    AstralProjector projector;
    DreamState ds;
    DreamPipeline pipe;

    PillarStateVector entity_pillars;
    entity_pillars.fill(vn::fp20_t(0.5f));
    entity_pillars[Awareness] = vn::fp20_t(0.6f);
    entity_pillars[Memory] = vn::fp20_t(0.5f);
    entity_pillars[Willpower] = vn::fp20_t(0.7f);
    entity_pillars[Warmth] = vn::fp20_t(0.4f);

    for (int i = 0; i < 10; i++) {
        PillarStateVector exp;
        for (int p = 0; p < NumPillars; p++)
            exp.pillars[p] = vn::fp20_t(static_cast<float>(std::rand() % 256) / 255.0f);
        engine.record_experience(pipe, exp, 0.5f);
    }

    pipe.stage = DreamPipelineStage::Dreaming;
    pipe.stage_ticks = 0;

    DreamSandbox sandbox;
    sandbox.physics_looseness = 0.4f;
    engine.generate_dream(ds, pipe, entity_pillars, &sandbox);

    TEST("dream generated with episode");
    {
        if (!ds.dream_content.empty()) {
            std::cout << "\n";
            print_dream_episode(ds.dream_content.back());
            PASS();
        } else FAIL("no dream content");
    }

    InternalInsight insight = engine.process_internally(ds, entity_pillars, 42);
    TEST("internal processing produced insight");
    {
        if (insight.clarity > 0.0f) {
            print_psv(insight.delta, "insight delta");
            PASS();
        } else FAIL("no insight");
    }

    PlayObjective obj = engine.insight_to_play_objective(insight, entity_pillars);
    TEST("play objective formed");
    {
        if (obj.expected_difficulty > 0.0f) {
            print_psv(obj.target_pillar_delta, "play objective");
            PASS();
        } else FAIL("trivial play objective");
    }

    float shift = engine.play_to_learning(entity_pillars, obj, 10.0f);
    TEST("play produced learning");
    {
        if (shift > 0.0f) {
            print_psv(entity_pillars, "entity after learning");
            PASS();
        } else FAIL("no learning");
    }

    Artifact artifact = engine.externalize(entity_pillars, ArtifactType::Tool, 100);
    TEST("knowledge externalized as artifact");
    {
        if (artifact.quality > 0.0f) {
            std::cout << "    artifact type=" << artifact_type_name(artifact.type)
                      << " quality=" << std::fixed << std::setprecision(3)
                      << artifact.quality << "\n";
            PASS();
        } else FAIL("poor artifact quality");
    }

    PillarStateVector teacher = entity_pillars;
    teacher[Relation] = vn::fp20_t(0.7f);
    teacher[Distortion] = vn::fp20_t(0.1f);

    PillarStateVector student;
    student[Awareness] = vn::fp20_t(0.6f);
    student[Willpower] = vn::fp20_t(0.5f);
    student[Memory] = vn::fp20_t(0.5f);

    TeachingResult tr = engine.teach(artifact, teacher, student);
    TEST("teaching imperfectly transmitted knowledge");
    {
        if (tr.quality_score > 0.0f && tr.quality_score < 1.0f) {
            std::cout << "    teaching quality=" << std::fixed << std::setprecision(3)
                      << tr.quality_score
                      << " understood=" << (tr.student_understood ? "yes" : "no") << "\n";
            PASS();
        } else FAIL("unexpected quality: " + std::to_string(tr.quality_score));
    }
}

static void demo_integration() {
    std::cout << "\n=== Consciousness Demo: Dream → Learn → Teach ===\n";

    DreamEngine engine;
    AstralProjector projector;
    RealmNavigator navigator;
    DreamState dream_state;
    DreamPipeline pipeline;

    PillarStateVector entity;
    entity.fill(vn::fp20_t(0.5f));
    entity[Awareness] = vn::fp20_t(0.65f);
    entity[Memory] = vn::fp20_t(0.55f);
    entity[Willpower] = vn::fp20_t(0.72f);
    entity[Warmth] = vn::fp20_t(0.42f);

    for (int i = 0; i < 12; i++) {
        PillarStateVector exp;
        for (int p = 0; p < NumPillars; p++)
            exp.pillars[p] = vn::fp20_t(0.3f + static_cast<float>(i) / 20.0f);
        engine.record_experience(pipeline, exp, 0.3f + static_cast<float>(i) / 20.0f);
    }

    std::cout << "Entity initial state:\n";
    print_psv(entity, "pillars");

    int stage_ticks = 0;
    DreamPipelineStage stage = DreamPipelineStage::Dreaming;

    for (int tick = 0; tick < 200; tick++) {
        switch (stage) {
            case DreamPipelineStage::Dreaming: {
                if (stage_ticks == 0) {
                    DreamSandbox dream;
                    dream.physics_looseness = 0.5f;
                    engine.generate_dream(dream_state, pipeline, entity, &dream);

                    for (int p = 0; p < NumPillars; p++) {
                        entity.pillars[p] = vn::fp20_t(
                            dream_state.dream_content.back().simulated_pillars[p] * 0.3f +
                            entity.pillars[p] * 0.7f
                        );
                    }
                    std::cout << "\n--- Dreaming ---\n";
                    print_dream_episode(dream_state.dream_content.back());
                }
                stage_ticks++;
                if (stage_ticks >= 5) {
                    engine.check_lucid(dream_state, entity);
                    stage = DreamPipelineStage::ProcessingInternally;
                    stage_ticks = 0;
                }
                break;
            }
            case DreamPipelineStage::ProcessingInternally: {
                stage_ticks++;
                if (stage_ticks >= 3) {
                    InternalInsight insight = engine.process_internally(
                        dream_state, entity, tick);
                    std::cout << "--- Internal Processing (clarity="
                              << std::fixed << std::setprecision(3) << insight.clarity << ") ---\n";
                    pipeline.pending_insights.push_back(insight);
                    stage = DreamPipelineStage::FormingPlayObjective;
                    stage_ticks = 0;
                }
                break;
            }
            case DreamPipelineStage::FormingPlayObjective: {
                if (!pipeline.pending_insights.empty()) {
                    PlayObjective obj = engine.insight_to_play_objective(
                        pipeline.pending_insights.back(), entity);
                    pipeline.play_queue.push_back(obj);
                    pipeline.pending_insights.pop_back();
                    std::cout << "--- Play Objective (difficulty="
                              << obj.expected_difficulty << ") ---\n";
                    print_psv(obj.target_pillar_delta, "delta");
                }
                stage = DreamPipelineStage::Playing;
                stage_ticks = 0;
                break;
            }
            case DreamPipelineStage::Playing: {
                if (!pipeline.play_queue.empty()) {
                    float s = engine.play_to_learning(entity, pipeline.play_queue.front(), 1.0f);
                    pipeline.pipeline_progress += s;
                    if (stage_ticks >= 15 || pipeline.pipeline_progress >
                        pipeline.play_queue.front().expected_difficulty) {
                        pipeline.play_queue.front().attempted = true;
                        std::cout << "--- Playing (progress="
                                  << pipeline.pipeline_progress << ") ---\n";
                        pipeline.play_queue.erase(pipeline.play_queue.begin());
                        stage = DreamPipelineStage::Learning;
                        stage_ticks = 0;
                    }
                }
                stage_ticks++;
                break;
            }
            case DreamPipelineStage::Learning: {
                stage_ticks++;
                if (stage_ticks >= 5) {
                    std::cout << "--- Learning ---\n";
                    print_psv(entity, "new state");
                    stage = DreamPipelineStage::Externalizing;
                    stage_ticks = 0;
                }
                break;
            }
            case DreamPipelineStage::Externalizing: {
                stage_ticks++;
                if (stage_ticks >= 10) {
                    Artifact art = engine.externalize(entity, ArtifactType::Tool, tick);
                    pipeline.artifacts.push_back(art);
                    std::cout << "--- Externalized Artifact ---\n";
                    std::cout << "    type=" << artifact_type_name(art.type)
                              << " quality=" << art.quality << "\n";
                    print_psv(art.essence, "essence");

                    PillarStateVector teacher = entity;
                    teacher[Relation] = vn::fp20_t(0.7f);
                    teacher[Distortion] = vn::fp20_t(0.15f);

                    PillarStateVector student;
                    student[Awareness] = vn::fp20_t(0.55f);
                    student[Willpower] = vn::fp20_t(0.5f);
                    student[Memory] = vn::fp20_t(0.45f);

                    TeachingResult tr = engine.teach(art, teacher, student);
                    std::cout << "    Teaching quality=" << tr.quality_score
                              << " understood=" << (tr.student_understood ? "yes" : "no") << "\n";

                    stage = DreamPipelineStage::Idle;
                    stage_ticks = 0;
                }
                break;
            }
            default: break;
        }
    }

    std::cout << "\n=== Astral Projection into Black Hole Universe ===\n";

    projector.begin_meditation(entity);
    for (int i = 0; i < 50; i++)
        projector.meditation_tick(entity, 1.0f);

    std::cout << "Meditation state: " << meditation_state_name(projector.state())
              << " depth=" << projector.meditation_depth() << "\n";

    if (projector.state() == MeditationState::AstralProjected) {
        PillarStateVector astral_body;
        projector.astral_project(entity, astral_body, 12);

        std::cout << "Astral body at depth 12 (BlackHoleUniverse):\n";
        print_psv(astral_body, "astral pillars");

        CosmologicalInsight insight = projector.astral_tick(astral_body, 12, 5.0f);
        std::cout << "Cosmological insight (clarity=" << insight.clarity << "):\n";

        RealmGeometry bh_geom = navigator.get_realm_geometry(12);
        std::cout << "  Realm: " << metaphysical_realm_name(bh_geom.realm) << "\n";

        auto universes = navigator.enter_black_hole_universe(
            {0, 0, 0, 0}, astral_body, 1);
        std::cout << "  Nested universes within black hole: " << universes.size() << "\n";
        if (!universes.empty()) {
            print_psv(universes[0].pillar_state, "universe[0] inverted state");
        }

        projector.return_from_astral(entity, astral_body);
        std::cout << "Returned from astral projection.\n";
        print_psv(entity, "entity after return");
    }

    std::cout << "\n=== All State Experience ===\n";
    PillarStateVector prior;
    navigator.enter_all_state(entity, prior);
    std::cout << "All state active - all pillars at 1.0:\n";
    print_psv(entity, "all state");

    navigator.exit_all_state(entity, prior);
    std::cout << "Relaxed from All state:\n";
    print_psv(entity, "relaxed");
    std::cout << "Memory enhanced: " << entity[Memory] << "\n";
}

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    std::cout << "Van Nueman Engine — Phase 13: Consciousness, Dream & Metaphysical Realms\n";
    std::cout << "========================================================================\n";

    test_dream_generation();
    test_lucid_dreaming();
    test_internal_processing();
    test_play_learn_externalize();
    test_teaching();
    test_meditation();
    test_astral_projection();
    test_metaphysical_realms();
    test_full_pipeline();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Tests passed: " << tests_passed
              << ", failed: " << tests_failed << "\n";

    demo_integration();

    return tests_failed > 0 ? 1 : 0;
}
