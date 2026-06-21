#include "../celestial/LargeScaleStructure.h"
#include "../celestial/UniversalExpansion.h"
#include "../celestial/CrossScaleWHTBridge.h"
#include "../voxel/VoxelCell.h"
#include "../voxel/YieldMatrix.h"
#include "../voxel/BCCIndex.h"
#include "../include/Entity.h"
#include "../tests/test_wht_fp.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { std::cout << "  " << name << "... "; } while(0)
#define PASS() do { std::cout << "PASS\n"; tests_passed++; } while(0)
#define FAIL(msg) do { std::cout << "FAIL: " << msg << "\n"; tests_failed++; } while(0)

static GalaxyCluster make_cluster(int i, int j, int k, float mass = 100.0f,
                                   float radius = 5.0f, float attraction = 0.5f) {
    GalaxyCluster c;
    c.position = {i, j, k, (i + j + k) & 1};
    c.mass = mass;
    c.radius = radius;
    c.pillar_attraction = attraction;
    c.pillar_warmth = 0.3f;
    c.num_galaxies = static_cast<uint32_t>(mass * 10);
    c.velocity_x = 0; c.velocity_y = 0; c.velocity_z = 0;
    return c;
}

static void test_cosmological_system() {
    std::cout << "\n=== CosmologicalSystem Tests ===\n";

    // 1. Hubble velocity
    TEST("hubble_velocity");
    {
        CosmologicalConfig cfg;
        float v = hubble_velocity(100.0f, cfg);
        float expected = 0.07f * 0.001f * 100.0f;
        if (fabsf(v - expected) < 0.001f) PASS();
        else FAIL("v mismatch: " + std::to_string(v));
    }

    // 2. scale factor expansion
    TEST("scale_factor expanding");
    {
        CosmologicalConfig cfg;
        cfg.expanding = true;
        float sf = scale_factor_at_time(100.0f, cfg);
        if (sf > cfg.scale_factor) PASS();
        else FAIL("should increase");
    }

    // 3. scale factor contraction
    TEST("scale_factor contracting");
    {
        CosmologicalConfig cfg;
        cfg.expanding = false;
        float sf = scale_factor_at_time(100.0f, cfg);
        if (sf < cfg.scale_factor) PASS();
        else FAIL("should decrease");
    }

    // 4. cosmological expand
    TEST("cosmological_expand");
    {
        CosmologicalConfig cfg;
        cfg.hubble_constant = 1.0f;
        cfg.expansion_rate = 0.1f;
        GalaxyCluster c = make_cluster(100, 100, 100, 100.0f, 5.0f);
        int32_t orig_i = c.position.i;
        float orig_r = c.radius;
        cosmological_expand(c, cfg, 100.0f);
        bool pos_grew = c.position.i > orig_i;
        bool rad_grew = c.radius > orig_r;
        if (pos_grew && rad_grew) PASS();
        else FAIL("pos=" + std::to_string(c.position.i) + "/" + std::to_string(orig_i)
                  + " rad=" + std::to_string(c.radius) + "/" + std::to_string(orig_r));
    }

    // 5. classify region
    TEST("classify_region");
    {
        bool void_ok = classify_region_cosmic(0.1f, 1.0f) == CosmicStructure::Void;
        bool cluster_ok = classify_region_cosmic(2.0f, 1.0f) == CosmicStructure::Cluster;
        bool sheet_ok = classify_region_cosmic(1.3f, 1.0f) == CosmicStructure::Sheet;
        bool filament_ok = classify_region_cosmic(1.0f, 1.0f) == CosmicStructure::Filament;
        if (void_ok && cluster_ok && sheet_ok && filament_ok) PASS();
        else FAIL("classification mismatch");
    }

    // 6. cosmic structure names
    TEST("cosmic_structure_name");
    {
        bool all_ok = true;
        all_ok &= strcmp(cosmic_structure_name(CosmicStructure::Void), "Void") == 0;
        all_ok &= strcmp(cosmic_structure_name(CosmicStructure::Filament), "Filament") == 0;
        all_ok &= strcmp(cosmic_structure_name(CosmicStructure::Sheet), "Sheet") == 0;
        all_ok &= strcmp(cosmic_structure_name(CosmicStructure::Cluster), "Cluster") == 0;
        all_ok &= strcmp(cosmic_structure_name(CosmicStructure::Supercluster), "Supercluster") == 0;
        all_ok &= strcmp(cosmic_structure_name(static_cast<CosmicStructure>(99)), "Unknown") == 0;
        if (all_ok) PASS();
        else FAIL("name mismatch");
    }

    // 7. galaxy cluster default values
    TEST("galaxy cluster defaults");
    {
        GalaxyCluster c;
        if (c.mass == 0.0f && c.radius == 0.0f && c.num_galaxies == 0) PASS();
        else FAIL("defaults not zero");
    }

    // 8. scale layer init
    TEST("scale layer init");
    {
        Universe uni;
        uni.init_scale_layers();
        if (uni.num_scale_layers == 8) PASS();
        else FAIL("expected 8 layers, got " + std::to_string(uni.num_scale_layers));
    }

    // 9. WHT round-trip on scale layers
    TEST("WHT bridge round-trip");
    {
        Universe uni;
        uni.init_scale_layers();
        for (int i = 0; i < uni.num_scale_layers; i++) {
            for (int p = 0; p < NumPillars; p++) {
                uni.scale_layers[i].pillar_state[p] = vn::fp20_t(0.1f * (i + 1));
            }
        }

        vn::fp20_t original[8][NumPillars];
        for (int i = 0; i < uni.num_scale_layers; i++)
            for (int p = 0; p < NumPillars; p++)
                original[i][p] = uni.scale_layers[i].pillar_state[p];

        cross_scale_wht_bridge(uni.scale_layers, uni.num_scale_layers);
        cross_scale_inverse_wht_bridge(uni.scale_layers, uni.num_scale_layers);

        bool match = true;
        for (int i = 0; i < uni.num_scale_layers && match; i++) {
            for (int p = 0; p < NumPillars && match; p++) {
                vn::fp20_t diff = uni.scale_layers[i].pillar_state[p] - original[i][p];
                if (diff < vn::fp20_t(0)) diff = vn::fp20_t(0) - diff;
                if (vn::fp20_t(0.01f) < diff) match = false;
            }
        }
        if (match) PASS();
        else FAIL("round-trip deviation too large");
    }

    // 10. large-scale structure detection
    TEST("large scale structure detection");
    {
        CosmicField field;
        field.clusters.push_back(make_cluster(0, 0, 0, 1000.0f, 3.0f, 0.8f));
        field.clusters.push_back(make_cluster(100, 0, 0, 10.0f, 3.0f, 0.3f));
        field.clusters.push_back(make_cluster(200, 0, 0, 10.0f, 3.0f, 0.3f));
        detect_large_scale_structure(field);
        if (field.structure_map.size() == 3) PASS();
        else FAIL("expected 3 structure entries");
    }

    // 11. WHT test from test_wht_fp
    TEST("WHT fp20 round-trip base test");
    {
        run_wht_fp_tests();
        PASS();
    }

    // 12. universe tick
    TEST("universe tick");
    {
        Universe uni;
        uni.config.expanding = true;
        uni.field.clusters.push_back(make_cluster(0, 0, 0, 500.0f, 5.0f, 0.7f));
        uni.field.clusters.push_back(make_cluster(50, 0, 0, 300.0f, 4.0f, 0.6f));
        uni.init_scale_layers();
        float initial_scale = uni.config.scale_factor;
        uni.tick(10.0f);
        if (uni.config.scale_factor > initial_scale) PASS();
        else FAIL("scale should increase");
    }
}

static void demo_cosmology() {
    std::cout << "\n=== Cosmological Demo: Large-Scale Structure ===\n";

    Universe universe;
    universe.config.hubble_constant = 0.07f;
    universe.config.expansion_rate = 0.002f;
    universe.config.expanding = true;

    std::srand(static_cast<unsigned>(std::time(nullptr)));

    for (int i = 0; i < 20; i++) {
        int x = (std::rand() % 200) - 100;
        int y = (std::rand() % 200) - 100;
        int z = (std::rand() % 200) - 100;
        float mass = 50.0f + static_cast<float>(std::rand() % 500);
        float attraction = 0.3f + static_cast<float>(std::rand() % 70) / 100.0f;
        universe.field.clusters.push_back(
            make_cluster(x, y, z, mass, 3.0f, attraction));
    }

    std::cout << "Galaxy clusters: " << universe.field.clusters.size() << "\n";

    universe.init_scale_layers();

    std::cout << "\n--- Cosmic Evolution Timeline ---\n";
    int timeline[] = {0, 10, 50, 100, 200, 500};
    for (int t : timeline) {
        while (universe.cosmic_time < t) {
            universe.tick(1.0f);
        }
        detect_large_scale_structure(universe.field);
        if (t == 0 || t == 500) {
            std::cout << "  t=" << std::setw(4) << t
                      << " scale=" << std::fixed << std::setprecision(4)
                      << universe.config.scale_factor;
            int voids = 0, filaments = 0, sheets = 0, clusters = 0, superclusters = 0;
            for (auto s : universe.field.structure_map) {
                switch (s) {
                    case CosmicStructure::Void: voids++; break;
                    case CosmicStructure::Filament: filaments++; break;
                    case CosmicStructure::Sheet: sheets++; break;
                    case CosmicStructure::Cluster: clusters++; break;
                    case CosmicStructure::Supercluster: superclusters++; break;
                    default: break;
                }
            }
            std::cout << " V:" << voids << " F:" << filaments
                      << " S:" << sheets << " C:" << clusters
                      << " SC:" << superclusters << "\n";
        }
    }

    std::cout << "\n--- Cross-Scale WHT Bridge ---\n";
    for (int i = 0; i < universe.num_scale_layers; i++) {
        auto& layer = universe.scale_layers[i];
        float force = layer.pillar_state[Force].to_float();
        float warmth = layer.pillar_state[Warmth].to_float();
        std::cout << "  Scale " << layer.scale_level
                  << " (10^" << (i - 4) << "): Force="
                  << std::fixed << std::setprecision(3) << force
                  << " Warmth=" << warmth << "\n";
    }

    std::cout << "\n--- Contraction Phase ---\n";
    universe.config.expanding = false;
    float start_scale = universe.config.scale_factor;
    for (int t = 0; t < 100; t++) {
        universe.tick(1.0f);
    }
    std::cout << "  Scale went from " << start_scale << " to "
              << universe.config.scale_factor
              << " (contracted by " << (1.0f - universe.config.scale_factor / start_scale) * 100.0f
              << "%)\n";
}

int main() {
    std::cout << "Van Nueman Engine — Phase 12: Cosmological Formation\n";
    std::cout << "=====================================================\n";

    test_cosmological_system();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Tests passed: " << tests_passed
              << ", failed: " << tests_failed << "\n";

    demo_cosmology();

    return tests_failed > 0 ? 1 : 0;
}
