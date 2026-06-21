#include "../celestial/GravitationalSystem.h"
#include "../celestial/PlanetarySystem.h"
#include "../celestial/StellarSystem.h"
#include "../voxel/VoxelCell.h"
#include "../voxel/YieldMatrix.h"
#include "../voxel/BCCIndex.h"
#include "../include/Entity.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <cassert>
#include <string>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { std::cout << "  " << name << "... "; } while(0)
#define PASS() do { std::cout << "PASS\n"; tests_passed++; } while(0)
#define FAIL(msg) do { std::cout << "FAIL: " << msg << "\n"; tests_failed++; } while(0)

static VoxelCell make_cell(int i, int j, int k, float cohesion = 0.5f,
                            float integrity = 0.5f, float attraction = 0.5f,
                            float force = 0.5f, float warmth = 0.5f) {
    VoxelCell cell;
    cell.coord = {i, j, k, (i + j + k) & 1};
    cell.active = true;
    for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
        cell.pyramids[f].material_composition[Cohesion] = vn::fp20_t(cohesion);
        cell.pyramids[f].material_composition[Integrity] = vn::fp20_t(integrity);
        cell.pyramids[f].material_composition[Attraction] = vn::fp20_t(attraction);
        cell.pyramids[f].material_composition[Force] = vn::fp20_t(force);
        cell.pyramids[f].material_composition[Warmth] = vn::fp20_t(warmth);
        cell.pyramids[f].material_composition[Flux] = vn::fp20_t(0.5f);
        cell.pyramids[f].material_composition[Harm] = vn::fp20_t(0.0f);
        cell.pyramids[f].material_composition[Awareness] = vn::fp20_t(0.3f);
        cell.pyramids[f].material_composition[Resistance] = vn::fp20_t(0.5f);
        cell.pyramids[f].material_composition[Willpower] = vn::fp20_t(0.5f);
        cell.pyramids[f].volume = vn::fp20_t(0.5f);
    }
    return cell;
}

static void test_gravitational_system() {
    std::cout << "\n=== GravitationalSystem Tests ===\n";

    // 1. voxel_mass
    TEST("voxel_mass basic");
    {
        VoxelCell c = make_cell(0, 0, 0, 0.6f, 0.4f, 0.5f);
        float m = voxel_mass(c);
        float expected = 0.6f + 0.4f + 0.5f;
        if (fabsf(m - expected) < 0.01f) PASS();
        else FAIL("mass mismatch: got " + std::to_string(m) + " expected " + std::to_string(expected));
    }

    // 2. center of mass
    TEST("center_of_mass");
    {
        std::vector<VoxelCell> cells;
        cells.push_back(make_cell(0, 0, 0, 0.5f, 0.5f, 0.5f));
        cells.push_back(make_cell(10, 0, 0, 5.0f, 5.0f, 5.0f));
        cells.push_back(make_cell(0, 2, 0, 0.5f, 0.5f, 0.5f));
        BCCCoord com = center_of_mass(cells);
        if (com.i > 2) PASS();
        else FAIL("com.i should be > 2, got " + std::to_string(com.i));
    }

    // 3. gravitational force
    TEST("gravitational_force");
    {
        VoxelCell a = make_cell(0, 0, 0, 1.0f, 1.0f, 1.0f);
        VoxelCell b = make_cell(10, 0, 0, 1.0f, 1.0f, 1.0f);
        GravitationalForce f = gravitational_force(a, b);
        if (f.magnitude > 0.0f && f.fx > 0.0f) PASS();
        else FAIL("no gravitational force");
    }

    // 4. same position => zero force
    TEST("gravitational_force same pos");
    {
        VoxelCell a = make_cell(5, 5, 5, 1.0f, 1.0f, 1.0f);
        VoxelCell b = make_cell(5, 5, 5, 1.0f, 1.0f, 1.0f);
        GravitationalForce f = gravitational_force(a, b);
        if (f.magnitude < 0.001f) PASS();
        else FAIL("same pos should have ~0 force");
    }

    // 5. aggregate_clusters
    TEST("aggregate_clusters");
    {
        std::vector<VoxelCell> cells;
        cells.push_back(make_cell(0, 0, 0, 0.5f, 0.5f, 0.8f));
        cells.push_back(make_cell(1, 0, 0, 0.5f, 0.5f, 0.8f));
        cells.push_back(make_cell(100, 0, 0, 0.5f, 0.5f, 0.2f));
        auto clusters = aggregate_clusters(cells, 0.4f);
        if (clusters.size() > 0) {
            bool found = false;
            for (const auto& cl : clusters) {
                if (cl.size() >= 2) { found = true; break; }
            }
            if (found) PASS();
            else FAIL("no cluster >= 2");
        } else {
            FAIL("no clusters at all");
        }
    }

    // 6. orbital_velocity
    TEST("orbital_velocity");
    {
        float v = orbital_velocity(100.0f, 10.0f, 0.1f);
        if (v > 0.0f && v < 10.0f) PASS();
        else FAIL("orbital velocity out of range");
    }

    // 7. escape_velocity > orbital_velocity
    TEST("escape_velocity > orbital_velocity");
    {
        float v_orb = orbital_velocity(100.0f, 10.0f, 0.1f);
        float v_esc = escape_velocity(100.0f, 10.0f, 0.1f);
        if (v_esc > v_orb) PASS();
        else FAIL("escape should exceed orbital");
    }

    // 8. is_bound_orbit
    TEST("is_bound_orbit");
    {
        bool bound = is_bound_orbit(0.5f, 100.0f, 10.0f, 0.1f);
        bool unbound = is_bound_orbit(10.0f, 100.0f, 10.0f, 0.1f);
        if (bound && !unbound) PASS();
        else FAIL("orbit bounding logic");
    }
}

static void test_planetary_system() {
    std::cout << "\n=== PlanetarySystem Tests ===\n";

    // 1. voxel_density
    TEST("voxel_density");
    {
        VoxelCell c = make_cell(0, 0, 0, 0.8f, 0.6f, 0.3f);
        float d = voxel_density(c);
        if (d > 0.5f && d < 5.0f) PASS();
        else FAIL("density out of range: " + std::to_string(d));
    }

    // 2. differentiate (small body)
    TEST("differentiate");
    {
        std::vector<VoxelCell> cells;
        for (int i = -2; i <= 2; i++)
            for (int j = -2; j <= 2; j++)
                for (int k = -2; k <= 2; k++)
                    cells.push_back(make_cell(i, j, k, 0.8f, 0.6f, 0.5f));
        PlanetaryBody body;
        differentiate(cells, body);
        bool has_core = !body.core_cells.empty();
        bool has_mantle = !body.mantle_cells.empty();
        bool has_crust = !body.crust_cells.empty();
        if (has_core && has_mantle && has_crust) PASS();
        else FAIL("missing layers: core=" + std::to_string(has_core) +
                  " mantle=" + std::to_string(has_mantle) + " crust=" + std::to_string(has_crust));
    }

    // 3. density sorted: core > mantle > crust
    TEST("density ordering");
    {
        std::vector<VoxelCell> cells;
        int idx = 0;
        for (int i = -2; i <= 2; i++) {
            for (int j = -2; j <= 2; j++) {
                for (int k = -2; k <= 2; k++) {
                    float cohesion = 0.3f + 0.7f * (idx % 5) / 4.0f;
                    cells.push_back(make_cell(i, j, k, cohesion, 0.5f, 0.5f));
                    idx++;
                }
            }
        }
        PlanetaryBody body;
        differentiate(cells, body);
        compute_planetary_structure(body);
        if (body.layers.core_density >= body.layers.mantle_density &&
            body.layers.mantle_density >= body.layers.crust_density)
            PASS();
        else FAIL("density ordering violated: core=" +
                  std::to_string(body.layers.core_density) + " mantle=" +
                  std::to_string(body.layers.mantle_density) + " crust=" +
                  std::to_string(body.layers.crust_density));
    }

    // 4. layer_name
    TEST("layer_name");
    {
        LayerThresholds thresh;
        bool core_match = (strcmp(layer_name(3.0f, thresh), "Core") == 0);
        bool mantle_match = (strcmp(layer_name(2.0f, thresh), "Mantle") == 0);
        bool crust_match = (strcmp(layer_name(1.0f, thresh), "Crust") == 0);
        if (core_match && mantle_match && crust_match) PASS();
        else FAIL("layer name mismatch");
    }

    // 5. body radius scales with n
    TEST("body radius scaling");
    {
        std::vector<VoxelCell> cells;
        for (int i = -3; i <= 3; i++)
            for (int j = -3; j <= 3; j++)
                for (int k = -3; k <= 3; k++)
                    cells.push_back(make_cell(i, j, k, 0.5f, 0.5f, 0.5f));
        PlanetaryBody body;
        differentiate(cells, body);
        if (body.radius > 0) PASS();
        else FAIL("radius should be > 0");
    }
}

static void test_stellar_system() {
    std::cout << "\n=== StellarSystem Tests ===\n";

    // 1. compute_core_force
    TEST("compute_core_force");
    {
        std::vector<VoxelCell> cells;
        cells.push_back(make_cell(0, 0, 0, 0.5f, 0.5f, 0.5f, 0.9f, 0.5f));
        cells.push_back(make_cell(1, 0, 0, 0.5f, 0.5f, 0.5f, 0.8f, 0.5f));
        float f = compute_core_force(cells);
        if (f > 0.7f && f <= 1.0f) PASS();
        else FAIL("force out of range: " + std::to_string(f));
    }

    // 2. compute_core_warmth
    TEST("compute_core_warmth");
    {
        std::vector<VoxelCell> cells;
        cells.push_back(make_cell(0, 0, 0, 0.5f, 0.5f, 0.5f, 0.5f, 0.95f));
        float w = compute_core_warmth(cells);
        if (w > 0.9f && w <= 1.0f) PASS();
        else FAIL("warmth out of range: " + std::to_string(w));
    }

    // 3. compute_core_density
    TEST("compute_core_density");
    {
        std::vector<VoxelCell> cells;
        cells.push_back(make_cell(0, 0, 0, 1.0f, 1.0f, 0.0f)); // high density
        float d = compute_core_density(cells);
        if (d > 0.5f) PASS();
        else FAIL("density too low: " + std::to_string(d));
    }

    // 4. check_ignition - insufficient
    TEST("ignition insufficient");
    {
        StellarBody star;
        star.core_cells.push_back(make_cell(0, 0, 0, 0.5f, 0.5f, 0.5f, 0.3f, 0.3f));
        bool ignited = check_ignition(star);
        if (!ignited) PASS();
        else FAIL("should not ignite with low force/warmth");
    }

    // 5. check_ignition - sufficient
    TEST("ignition sufficient");
    {
        StellarBody star;
        star.core_cells.push_back(make_cell(0, 0, 0, 1.5f, 1.5f, 0.5f, 0.9f, 0.95f));
        star.core_cells.push_back(make_cell(1, 0, 0, 1.5f, 1.5f, 0.5f, 0.9f, 0.95f));
        bool ignited = check_ignition(star);
        if (ignited) PASS();
        else FAIL("should ignite with sufficient force/warmth/density");
    }

    // 6. fusion_energy_output - no fusion before ignition
    TEST("fusion output without ignition");
    {
        StellarBody star;
        star.state = StellarState::Protostar;
        star.core_cells.push_back(make_cell(0, 0, 0, 0.5f, 0.5f, 0.5f, 0.9f, 0.5f));
        float energy = fusion_energy_output(star);
        if (energy == 0.0f) PASS();
        else FAIL("protostar should produce 0 fusion energy");
    }

    // 7. stellar_evolution_tick - protostar to main sequence
    TEST("protostar evolution");
    {
        StellarBody star;
        star.core_cells.push_back(make_cell(0, 0, 0, 1.5f, 1.5f, 0.5f, 0.9f, 0.95f));
        star.core_cells.push_back(make_cell(1, 0, 0, 1.5f, 1.5f, 0.5f, 0.9f, 0.95f));
        star.core_cells.push_back(make_cell(0, 1, 0, 1.5f, 1.5f, 0.5f, 0.9f, 0.95f));
        stellar_evolution_tick(star, 1.0f);
        if (star.state == StellarState::MainSequence) PASS();
        else FAIL("should transition to MainSequence, got " +
                  std::string(stellar_state_name(star.state)));
    }

    // 8. ignite_star
    TEST("ignite_star");
    {
        StellarBody star;
        star.core_cells.push_back(make_cell(0, 0, 0, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f));
        star.state = StellarState::Protostar;
        ignite_star(star);
        if (star.state == StellarState::MainSequence) {
            bool cells_warm = true;
            for (const auto& c : star.core_cells) {
                for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                    float w = c.pyramids[f].material_composition[Warmth].to_float();
                    if (w < 0.9f) { cells_warm = false; break; }
                }
            }
            if (cells_warm) PASS();
            else FAIL("cells not heated to max warmth");
        } else {
            FAIL("should be MainSequence after ignite");
        }
    }

    // 9. stellar evolution timeline: main sequence → red giant → white dwarf
    TEST("stellar lifecycle");
    {
        StellarBody star;
        star.core_cells.push_back(make_cell(0, 0, 0, 0.4f, 0.4f, 0.5f, 0.9f, 0.95f));
        star.state = StellarState::MainSequence;
        star.remaining_fuel = 0.0f;
        stellar_evolution_tick(star, 1.0f);
        if (star.state == StellarState::RedGiant ||
            star.state == StellarState::WhiteDwarf) PASS();
        else FAIL("unexpected state: " + std::string(stellar_state_name(star.state))
                  + " mass=" + std::to_string(star.total_mass));
    }

    // 10. StellarState names
    TEST("stellar_state_name all states");
    {
        bool all_valid = true;
        all_valid &= (strcmp(stellar_state_name(StellarState::Protostar), "Protostar") == 0);
        all_valid &= (strcmp(stellar_state_name(StellarState::MainSequence), "MainSequence") == 0);
        all_valid &= (strcmp(stellar_state_name(StellarState::RedGiant), "RedGiant") == 0);
        all_valid &= (strcmp(stellar_state_name(StellarState::WhiteDwarf), "WhiteDwarf") == 0);
        all_valid &= (strcmp(stellar_state_name(StellarState::NeutronStar), "NeutronStar") == 0);
        all_valid &= (strcmp(stellar_state_name(StellarState::Supernova), "Supernova") == 0);
        all_valid &= (strcmp(stellar_state_name(StellarState::BlackHole), "BlackHole") == 0);
        all_valid &= (strcmp(stellar_state_name(static_cast<StellarState>(99)), "Unknown") == 0);
        if (all_valid) PASS();
        else FAIL("name mismatch");
    }
}

static void demo_integration() {
    std::cout << "\n=== Celestial Demo: Gravitational Aggregation ===\n";

    std::vector<VoxelCell> core;
    std::vector<VoxelCell> envelope;
    for (int i = -2; i <= 2; i++) {
        for (int j = -2; j <= 2; j++) {
            for (int k = -2; k <= 2; k++) {
                float dist = sqrtf(static_cast<float>(i*i + j*j + k*k));
                if (dist <= 1.5f) {
                    core.push_back(make_cell(i, j, k, 2.0f, 1.5f, 0.5f, 0.92f, 0.95f));
                } else {
                    envelope.push_back(make_cell(i, j, k, 0.8f, 0.6f, 0.5f, 0.6f, 0.4f));
                }
            }
        }
    }

    std::cout << "Core cells: " << core.size() << ", envelope: " << envelope.size() << "\n";
    float total_force = 0.0f;
    float total_warmth = 0.0f;
    for (const auto& c : core) {
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            total_force += c.pyramids[f].material_composition[Force].to_float();
            total_warmth += c.pyramids[f].material_composition[Warmth].to_float();
        }
    }
    if (!core.empty()) {
        uint32_t faces = VoxelCell::FACE_COUNT;
        std::cout << "  Core Avg Force: " << (total_force / (core.size() * faces)) << "\n";
        std::cout << "  Core Avg Warmth: " << (total_warmth / (core.size() * faces)) << "\n";
    }

    std::cout << "\n=== Stellar Evolution ===\n";
    StellarBody star;
    star.core_cells = core;
    star.envelope_cells = envelope;
    star.remaining_fuel = 1.0f;
    star.state = StellarState::Protostar;

    int tick = 0;
    while (star.state == StellarState::Protostar && tick < 5) {
        stellar_evolution_tick(star, 1.0f);
        tick++;
    }
    std::cout << "  State after " << tick << " ticks: "
              << stellar_state_name(star.state) << "\n";

    if (star.state == StellarState::MainSequence) {
        float energy = fusion_energy_output(star);
        std::cout << "  Fusion output: " << energy << " energy/tick\n";
        std::cout << "  Core temp: " << std::fixed << std::setprecision(0)
                  << star.core_temperature << " K\n";
        std::cout << "  Core pressure: " << star.core_pressure << "\n";

        for (int t = 0; t < 60; t++) {
            stellar_evolution_tick(star, 1.0f);
            if (t % 20 == 0) {
                std::cout << "  Tick " << (t+1) << ": "
                          << stellar_state_name(star.state)
                          << " fuel=" << std::fixed << std::setprecision(2)
                          << star.remaining_fuel << "\n";
            }
        }
        std::cout << "  Final state: " << stellar_state_name(star.state) << "\n";
    } else {
        std::cout << "  (star did not ignite)\n";
    }

    std::cout << "\n=== Orbital Mechanics ===\n";
    std::vector<VoxelCell> all_cells = core;
    all_cells.insert(all_cells.end(), envelope.begin(), envelope.end());
    float central_mass = body_mass(all_cells);
    std::cout << "  Central mass: " << central_mass << "\n";
    std::cout << "  Orbital velocity at r=10: "
              << orbital_velocity(central_mass, 10.0f) << "\n";
    std::cout << "  Escape velocity at r=10: "
              << escape_velocity(central_mass, 10.0f) << "\n";
}

int main() {
    std::cout << "Van Nueman Engine — Phase 11: Celestial Formation\n";
    std::cout << "==================================================\n";

    test_gravitational_system();
    test_planetary_system();
    test_stellar_system();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Tests passed: " << tests_passed
              << ", failed: " << tests_failed << "\n";

    demo_integration();

    return tests_failed > 0 ? 1 : 0;
}
