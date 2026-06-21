#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include "../chemistry/EnvironmentalChemistry.h"
#include "../chemistry/DiffusionSystem.h"
#include "../chemistry/ReactionDiffusion.h"
#include "../chemistry/ChemicalBond.h"
#include "../chemistry/Molecule.h"
#include "../voxel/YieldMatrix.h"
#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"

static VoxelCell make_atom(BCCCoord coord, const YieldMatrix& mat, float size, uint32_t id) {
    VoxelCell cell;
    cell.init(coord, vn::fp20_t(size), id, mat);
    for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
        for (uint32_t p = 0; p < NumPillars; p++) {
            cell.pyramids[f].material_composition[p] = mat.pillar_to_vertex[p][0];
        }
    }
    return cell;
}

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    std::cout << "=== Voxel Environmental Chemistry Demo ===" << std::endl;
    std::cout << "Phase 9: Environmental Chemistry (Scale 10)" << std::endl;
    std::cout << std::endl;

    // ========== 1. pH and Redox for different environments ==========
    std::cout << "--- pH / Redox / Buffer ---" << std::endl;

    vn::fp20_t acidic_env[NumPillars];
    vn::fp20_t neutral_env[NumPillars];
    vn::fp20_t basic_env[NumPillars];
    vn::fp20_t oxidizing_env[NumPillars];
    vn::fp20_t reducing_env[NumPillars];

    for (uint32_t p = 0; p < NumPillars; p++) {
        acidic_env[p] = vn::fp20_t(0.5f);
        neutral_env[p] = vn::fp20_t(0.5f);
        basic_env[p] = vn::fp20_t(0.5f);
        oxidizing_env[p] = vn::fp20_t(0.5f);
        reducing_env[p] = vn::fp20_t(0.5f);
    }
    acidic_env[Flux] = vn::fp20_t(0.8f);    // high proton
    acidic_env[Integrity] = vn::fp20_t(0.2f);
    acidic_env[Harm] = vn::fp20_t(0.3f);

    neutral_env[Flux] = vn::fp20_t(0.5f);
    neutral_env[Integrity] = vn::fp20_t(0.5f);
    neutral_env[Harm] = vn::fp20_t(0.0f);

    basic_env[Flux] = vn::fp20_t(0.2f);
    basic_env[Integrity] = vn::fp20_t(0.8f);
    basic_env[Harm] = vn::fp20_t(0.0f);

    oxidizing_env[Flux] = vn::fp20_t(0.9f);
    oxidizing_env[Cohesion] = vn::fp20_t(0.1f);
    oxidizing_env[Warmth] = vn::fp20_t(0.6f);

    reducing_env[Flux] = vn::fp20_t(0.1f);
    reducing_env[Cohesion] = vn::fp20_t(0.9f);
    reducing_env[Warmth] = vn::fp20_t(0.2f);

    std::cout << "  Acidic environment:   pH=" << compute_ph(acidic_env)
              << " redox=" << redox_potential(acidic_env)
              << " buffer_beta=" << buffer_capacity(acidic_env, 0.05f, 0.05f) << std::endl;
    std::cout << "  Neutral environment:  pH=" << compute_ph(neutral_env)
              << " redox=" << redox_potential(neutral_env)
              << " buffer_beta=" << buffer_capacity(neutral_env, 0.05f, 0.05f) << std::endl;
    std::cout << "  Basic environment:    pH=" << compute_ph(basic_env)
              << " redox=" << redox_potential(basic_env)
              << " buffer_beta=" << buffer_capacity(basic_env, 0.05f, 0.05f) << std::endl;
    std::cout << "  Oxidizing env:        pH=" << compute_ph(oxidizing_env)
              << " redox=" << redox_potential(oxidizing_env) << std::endl;
    std::cout << "  Reducing env:         pH=" << compute_ph(reducing_env)
              << " redox=" << redox_potential(reducing_env) << std::endl;
    std::cout << std::endl;

    // ========== 2. pKa / protonation state ==========
    std::cout << "--- pKa / Protonation States ---" << std::endl;
    float test_pKa = 4.76f;
    std::cout << "  pKa = " << test_pKa << " (acetic acid)" << std::endl;
    for (float ph = 2.0f; ph <= 8.0f; ph += 1.0f) {
        float protonated = protonation_state(ph, test_pKa);
        float deprotonated = deprotonation_state(ph, test_pKa);
        std::cout << "  pH=" << std::fixed << std::setprecision(1) << ph
                  << " protonated=" << std::setprecision(4) << protonated
                  << " deprotonated=" << deprotonated << std::endl;
    }
    std::cout << std::endl;

    // ========== 3. pH after addition / buffer ==========
    std::cout << "--- Buffer Action ---" << std::endl;
    float initial_ph = 4.76f;
    std::cout << "  Initial pH=" << initial_ph << " with 0.05M acid + 0.05M base" << std::endl;
    std::cout << "  After +0.01M acid:  pH="
              << ph_after_addition(initial_ph, test_pKa, 0.01f, 0.0f, 0.05f, 0.05f) << std::endl;
    std::cout << "  After +0.01M base:  pH="
              << ph_after_addition(initial_ph, test_pKa, 0.0f, 0.01f, 0.05f, 0.05f) << std::endl;
    std::cout << "  After +0.05M acid:  pH="
              << ph_after_addition(initial_ph, test_pKa, 0.05f, 0.0f, 0.05f, 0.05f) << std::endl;
    std::cout << std::endl;

    // ========== 4. Sample environment ==========
    std::cout << "--- GradientField Sampling ---" << std::endl;
    GradientField gf = sample_environment(neutral_env, 0.05f, 0.05f);
    std::cout << "  Neutral env: pH=" << gf.ph << " redox=" << gf.redox
              << " buffer=" << gf.buffer_beta << " temp=" << gf.temperature << "K" << std::endl;
    gf = sample_environment(acidic_env, 0.05f, 0.05f);
    std::cout << "  Acidic env:  pH=" << gf.ph << " redox=" << gf.redox
              << " buffer=" << gf.buffer_beta << " temp=" << gf.temperature << "K" << std::endl;
    std::cout << std::endl;

    // ========== 5. Diffusion between two molecules ==========
    std::cout << "--- Diffusion ---" << std::endl;
    YieldMatrix h2_mat = YieldMatrix::default_organic();
    YieldMatrix o2_mat = YieldMatrix::default_organic();
    for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
        h2_mat.pillar_to_vertex[Flux][v] = vn::fp20_t(0.7f);
        h2_mat.pillar_to_vertex[Warmth][v] = vn::fp20_t(0.6f);

        o2_mat.pillar_to_vertex[Flux][v] = vn::fp20_t(0.3f);
        o2_mat.pillar_to_vertex[Integrity][v] = vn::fp20_t(0.8f);
    }

    Molecule mol_a;
    mol_a.molecule_id = 1;
    mol_a.add_atom(make_atom({0, 0, 0, 0}, h2_mat, 0.4f, 1));

    Molecule mol_b;
    mol_b.molecule_id = 2;
    mol_b.add_atom(make_atom({5, 0, 0, 0}, o2_mat, 0.4f, 2));

    std::cout << "  Before diffusion:" << std::endl;
    std::cout << "    Mol A (H2-like) at (0,0,0)" << std::endl;
    std::cout << "    Mol B (O2-like) at (5,0,0)" << std::endl;

    DiffusionConfig dconfig;
    dconfig.dt = 0.5f;
    std::vector<Molecule> diff_mols = { mol_a, mol_b };
    std::vector<ConcentrationField> diff_fields;
    build_concentration_fields(diff_mols, diff_fields);

    vn::fp20_t env[NumPillars];
    for (uint32_t p = 0; p < NumPillars; p++) env[p] = vn::fp20_t(0.5f);
    env[Warmth] = vn::fp20_t(0.8f);

    for (int t = 0; t < 5; t++) {
        diffusion_tick(diff_mols, diff_fields, env, dconfig);
        build_concentration_fields(diff_mols, diff_fields);
    }

    std::cout << "  After 5 diffusion ticks (warmth=0.8):" << std::endl;
    std::cout << "    Mol A now at (" << diff_mols[0].atoms[0].coord.i << ","
              << diff_mols[0].atoms[0].coord.j << ","
              << diff_mols[0].atoms[0].coord.k << ")" << std::endl;
    std::cout << "    Mol B now at (" << diff_mols[1].atoms[0].coord.i << ","
              << diff_mols[1].atoms[0].coord.j << ","
              << diff_mols[1].atoms[0].coord.k << ")" << std::endl;
    std::cout << std::endl;

    // ========== 6. Gray-Scott reaction-diffusion pattern ==========
    std::cout << "--- Gray-Scott Reaction-Diffusion ---" << std::endl;
    std::vector<EnvironmentCell> rd_grid;
    for (int x = -3; x <= 3; x++) {
        for (int y = -3; y <= 3; y++) {
            for (int z = -3; z <= 3; z++) {
                EnvironmentCell ec;
                ec.coord = {x, y, z, (x + y + z) & 1};
                ec.active = true;
                for (uint32_t p = 0; p < NumPillars; p++) {
                    ec.pillars[p] = vn::fp20_t(0.5f);
                }
                ec.concentration[Flux] = 1.0f;   // U
                ec.concentration[Cohesion] = 0.0f; // V
                float dist = sqrtf(static_cast<float>(x*x + y*y + z*z));
                if (dist < 1.5f) {
                    ec.concentration[Cohesion] = 0.5f;
                    ec.concentration[Flux] = 0.5f;
                }
                rd_grid.push_back(ec);
            }
        }
    }
    std::cout << "  Grid: " << rd_grid.size() << " cells (7x7x7 BCC)" << std::endl;

    std::unordered_map<uint64_t, std::vector<uint32_t>> adjacency;
    build_env_adjacency(rd_grid, adjacency);

    float du = 0.16f, dv = 0.08f, feed = 0.035f, kill = 0.065f;
    for (int t = 0; t < 20; t++) {
        gray_scott_rd_tick(rd_grid, adjacency, du, dv, feed, kill, 0.5f);
    }

    int high_u = 0, high_v = 0;
    for (const auto& ec : rd_grid) {
        if (ec.concentration[Flux] > 0.8f) high_u++;
        if (ec.concentration[Cohesion] > 0.5f) high_v++;
    }
    std::cout << "  After 20 ticks (feed=" << feed << " kill=" << kill << "):" << std::endl;
    std::cout << "    Cells with U>0.8: " << high_u
              << "  Cells with V>0.5: " << high_v << std::endl;

    for (int x = -3; x <= 3; x++) {
        for (int y = -3; y <= 3; y++) {
            for (int z = -3; z <= 3; z++) {
                uint64_t key = bcc_hash({x, y, z, (x + y + z) & 1});
                auto it = adjacency.find(key);
                if (it != adjacency.end()) {
                    float avg_u = 0, avg_v = 0;
                    int cnt = 0;
                    for (uint32_t ni : it->second) {
                        avg_u += rd_grid[ni].concentration[Flux];
                        avg_v += rd_grid[ni].concentration[Cohesion];
                        cnt++;
                    }
                    if (cnt > 0) {
                        avg_u /= cnt;
                        avg_v /= cnt;
                    }
                    for (const auto& ec : rd_grid) {
                        if (bcc_hash(ec.coord) == key) {
                            std::cout << "  Center cell (" << ec.coord.i << ","
                                      << ec.coord.j << "," << ec.coord.k
                                      << "): U=" << std::fixed << std::setprecision(4) << ec.concentration[Flux]
                                      << " V=" << ec.concentration[Cohesion]
                                      << " neighbors=" << cnt
                                      << " avg_U=" << avg_u << " avg_V=" << avg_v << std::endl;
                            goto next_cell;
                        }
                    }
                }
                next_cell:;
            }
        }
    }
    std::cout << std::endl;

    // ========== 7. Reaction-diffusion with molecules + environment ==========
    std::cout << "--- Combined Reaction-Diffusion ---" << std::endl;
    ReactionDiffusionConfig rdconfig;
    rdconfig.dt = 0.1f;
    rdconfig.substeps = 2;

    std::vector<Molecule> rd_mols;
    Molecule rd_mol1, rd_mol2;
    rd_mol1.molecule_id = 10;
    rd_mol2.molecule_id = 11;

    YieldMatrix acid_mat = YieldMatrix::default_organic();
    YieldMatrix base_mat = YieldMatrix::default_organic();
    for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
        acid_mat.pillar_to_vertex[Flux][v] = vn::fp20_t(0.8f);
        acid_mat.pillar_to_vertex[Attraction][v] = vn::fp20_t(0.7f);
        base_mat.pillar_to_vertex[Flux][v] = vn::fp20_t(0.2f);
        base_mat.pillar_to_vertex[Integrity][v] = vn::fp20_t(0.8f);
    }
    rd_mol1.add_atom(make_atom({0, 0, 0, 0}, acid_mat, 0.4f, 10));
    rd_mol2.add_atom(make_atom({2, 0, 0, 0}, base_mat, 0.4f, 11));
    rd_mols.push_back(rd_mol1);
    rd_mols.push_back(rd_mol2);

    reaction_diffusion_tick(rd_mols, rd_grid, env, rdconfig);

    std::cout << "  After RD tick:" << std::endl;
    std::cout << "    Mol 10 (acid-like) atoms active: "
              << (rd_mols[0].atoms[0].active ? "yes" : "no") << std::endl;
    std::cout << "    Mol 11 (base-like) atoms active: "
              << (rd_mols[1].atoms[0].active ? "yes" : "no") << std::endl;
    std::cout << "    Mol 10 bonds: " << rd_mols[0].bond_count() << std::endl;
    std::cout << "    Mol 11 bonds: " << rd_mols[1].bond_count() << std::endl;
    std::cout << std::endl;

    std::cout << "Demo complete." << std::endl;
    return 0;
}
