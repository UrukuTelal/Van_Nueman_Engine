#include <cstdio>
#include <cmath>
#include <cstring>
#include <cassert>

#include "fugure/fugure.h"

static int fugure_tests_run = 0;
static int fugure_tests_passed = 0;

#define FUGURE_TEST(name) do { \
    fugure_tests_run++; \
    printf("  %-45s ", name); \
} while(0)

#define FUGURE_PASS() do { \
    fugure_tests_passed++; \
    printf("PASS\n"); \
} while(0)

static void test_sandbox_ingest() {
    FUGURE_TEST("FUGURE sandbox ingest isolates PSV");
    FugureSandbox sandbox;
    PillarStateVector input;
    input.fill(0.5f);
    input[Distortion] = 0.9f;
    input[Harm] = 0.8f;
    input[Resistance] = 0.1f;
    input[Willpower] = 0.3f;
    input[Flux] = 0.7f;

    assert(sandbox.ingest(input));
    assert(sandbox.is_isolated());
    assert(sandbox.get_contained_psv()[Resistance] >= 0.99f);
    assert(sandbox.get_contained_psv()[Willpower] == 0.3f);
    FUGURE_PASS();
}

static void test_accelerator_saturation() {
    FUGURE_TEST("FUGURE accelerator drives lie to saturation");
    FugureSandbox sandbox;
    PillarStateVector input;
    input.fill(0.5f);
    input[Distortion] = 0.95f;
    input[Harm] = 0.85f;
    input[Resistance] = 0.05f;
    input[Flux] = 0.9f;
    input[Depth] = 0.1f;
    input[Relation] = 0.8f;
    sandbox.ingest(input);

    FugureAccelerator accelerator;
    FugureDistortionMetrics metrics = accelerator.extrapolate(sandbox, 100.0f, 256);

    assert(metrics.saturated);
    assert(metrics.steps_to_saturation > 0);
    assert(metrics.steps_to_saturation <= 256);
    FUGURE_PASS();
}

static void test_tracer_maps_topology() {
    FUGURE_TEST("FUGURE tracer maps lie topology");
    FugureSandbox sandbox;
    PillarStateVector input;
    input.fill(0.5f);
    input[Distortion] = 0.9f;
    input[Harm] = 0.8f;
    input[Flux] = 0.7f;
    input[Resistance] = 0.1f;
    input[Relation] = 0.7f;
    sandbox.ingest(input);

    FugureAccelerator accelerator;
    FugureDistortionMetrics metrics = accelerator.extrapolate(sandbox, 100.0f, 128);

    FugureTracer tracer;
    FugureLieTopology topology = tracer.trace(sandbox, metrics);

    assert(!topology.torsion_trace.empty());
    assert(!topology.pillar_snapshots.empty());
    assert(!topology.flux_trajectory.empty());
    assert(!topology.relation_boundary.empty());
    assert(topology.negative_space_volume >= 0.0f);
    FUGURE_PASS();
}

static void test_extractor_recovers_truth() {
    FUGURE_TEST("FUGURE extractor recovers true payload");
    FugureSandbox sandbox;
    PillarStateVector input;
    input.fill(0.5f);
    input[Distortion] = 0.9f;
    input[Harm] = 0.8f;
    input[Flux] = 0.7f;
    input[Resistance] = 0.1f;
    input[Relation] = 0.7f;
    sandbox.ingest(input);

    FugureAccelerator accelerator;
    FugureDistortionMetrics metrics = accelerator.extrapolate(sandbox, 100.0f, 128);

    FugureTracer tracer;
    FugureLieTopology topology = tracer.trace(sandbox, metrics);

    FugureExtractor extractor;
    FugureExtractionResult result = extractor.extract(sandbox, topology);

    assert(result.extraction_confidence >= 0.0f);
    assert(result.extraction_confidence <= 1.0f);
    FUGURE_PASS();
}

static void test_full_pipeline() {
    FUGURE_TEST("FUGURE full pipeline end-to-end");
    FugureRuntime runtime;
    PillarStateVector input;
    input.fill(0.5f);
    input[Distortion] = 0.95f;
    input[Harm] = 0.9f;
    input[Flux] = 0.85f;
    input[Resistance] = 0.05f;
    input[Relation] = 0.75f;

    std::array<float, FUGURE_WHT_DIM> wht{};
    wht[0] = 0.5f;
    wht[1] = 0.8f;

    std::array<float, FUGURE_HOPF_FIBER_DIM> hopf{};
    hopf[0] = 0.3f;
    hopf[16] = 0.7f;

    FugureExtractionResult result = runtime.process(input, wht, hopf);

    assert(result.extraction_confidence > 0.0f);
    assert(result.immunized);
    for (int p = 0; p < NumPillars; ++p) {
        assert(result.true_payload[p] >= 0.0f);
        assert(result.true_payload[p] <= 1.0f);
    }
    FUGURE_PASS();
}

static void test_clean_input_no_saturation() {
    FUGURE_TEST("FUGURE clean input no saturation");
    FugureSandbox sandbox;
    PillarStateVector input;
    input.fill(0.5f);
    input[Resistance] = 0.9f;
    input[Integrity] = 0.9f;
    input[Relation] = 0.9f;
    sandbox.ingest(input);

    FugureAccelerator accelerator;
    FugureDistortionMetrics metrics = accelerator.extrapolate(sandbox, 1.0f, 64);

    assert(!metrics.saturated);
    FUGURE_PASS();
}

inline void run_fugure_tests() {
    printf("\n=== FUGURE Runtime Tests ===\n");
    test_sandbox_ingest();
    test_accelerator_saturation();
    test_tracer_maps_topology();
    test_extractor_recovers_truth();
    test_full_pipeline();
    test_clean_input_no_saturation();
    printf("--- FUGURE: %d passed, %d failed ---\n", fugure_tests_passed, fugure_tests_run - fugure_tests_passed);
}
