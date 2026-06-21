#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include "../biology/society_network.h"
#include "../biology/tool_use.h"
#include "../biology/material_science.h"
#include "../biology/voxel_organism.h"
#include "../chemistry/ChemicalBond.h"
#include "../chemistry/Molecule.h"
#include "../voxel/YieldMatrix.h"
#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"

static VoxelCell make_cell(BCCCoord coord, const YieldMatrix& mat, float size, uint32_t id) {
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

    std::cout << "=== Voxel Society & Technology Demo ===" << std::endl;
    std::cout << "Phase 10: Society & Technology (Scale 40)" << std::endl;
    std::cout << std::endl;

    // ========== 1. Society Network ==========
    std::cout << "--- Society Network ---" << std::endl;
    SocietyNetwork society;

    std::vector<VoxelOrganism> organisms(5);
    VoxelOrganismGenome genomes[5];
    for (int i = 0; i < 5; i++) {
        YieldMatrix mat = YieldMatrix::default_organic();
        for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
            mat.pillar_to_vertex[Warmth][v] = vn::fp20_t(0.5f + i * 0.1f);
            mat.pillar_to_vertex[Flux][v] = vn::fp20_t(0.4f + i * 0.05f);
        }
        PillarStateVector baseline;
        baseline.fill(vn::fp20_t(0.5f));
        baseline[Warmth] = vn::fp20_t(0.5f + i * 0.1f);
        baseline[Flux] = vn::fp20_t(0.4f + i * 0.05f);
        organisms[i].init_zygote({i * 3, 0, 0, 0}, mat, genomes[i], baseline);
        society.add_organism(&organisms[i], i + 1, 10.0f + i * 2.0f);
    }

    society.connect(1, 2);
    society.connect(2, 3);
    society.connect(3, 4);
    society.connect(4, 5);
    society.connect(1, 5);

    std::cout << "  Society created: " << society.node_count() << " organisms" << std::endl;
    std::cout << "  Active: " << society.active_count() << std::endl;

    for (int t = 0; t < 5; t++) {
        society.resource_sharing_tick();
    }

    for (uint32_t i = 0; i < society.node_count(); i++) {
        const auto& node = society.get_node(i);
        std::cout << "  Node " << node.node_id
                  << " dominant=" << (node.dominant ? "yes" : "no")
                  << " warmth=" << std::fixed << std::setprecision(3)
                  << node.local_resources.warmth
                  << " shared_warmth=" << node.shared_resources.warmth
                  << " info=" << node.local_resources.information << std::endl;
    }
    std::cout << std::endl;

    // ========== 2. Collective Decision ==========
    std::cout << "--- Collective Decision ---" << std::endl;
    society.collective_decision_tick();
    std::cout << "  Collective decision tick completed" << std::endl;
    std::cout << "  Active after decision: " << society.active_count() << std::endl;
    std::cout << std::endl;

    // ========== 3. Tool Use ==========
    std::cout << "--- Tool Use ---" << std::endl;

    YieldMatrix high_force_mat = YieldMatrix::default_organic();
    for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
        high_force_mat.pillar_to_vertex[Force][v] = vn::fp20_t(0.9f);
        high_force_mat.pillar_to_vertex[Willpower][v] = vn::fp20_t(0.8f);
    }
    VoxelOrganismGenome strong_genome;
    VoxelOrganism strong_org;
    PillarStateVector strong_baseline;
    strong_baseline.fill(vn::fp20_t(0.5f));
    strong_baseline[Force] = vn::fp20_t(0.9f);
    strong_baseline[Willpower] = vn::fp20_t(0.8f);
    strong_org.init_zygote({30, 0, 0, 0}, high_force_mat, strong_genome, strong_baseline);
    { EnvironmentalSample env = { vn::fp20_t(0.5f), vn::fp20_t(0.5f), vn::fp20_t(0.5f), vn::fp20_t(0.0f) };
      strong_org.tick(vn::fp20_t(0.1f), &env); }

    YieldMatrix rock_mat = YieldMatrix::default_rock();
    YieldMatrix organic_mat = YieldMatrix::default_organic();

    VoxelCell rock = make_cell({10, 0, 0, 0}, rock_mat, 0.5f, 100);
    VoxelCell target = make_cell({12, 0, 0, 0}, organic_mat, 0.5f, 101);
    VoxelCell target2 = make_cell({13, 0, 0, 0}, organic_mat, 0.5f, 102);

    ToolAction push_action;
    push_action.type = ToolAction::PUSH;
    push_action.force_applied = 1.2f;
    push_action.target_coord = rock.coord;

    ToolResult push_result = push_voxel(rock, {1, 0, 0, 0}, push_action.force_applied, strong_org);
    std::cout << "  PUSH rock: success=" << (push_result.success ? "yes" : "no")
              << " energy=" << push_result.energy_cost
              << " new_pos=(" << rock.coord.i << "," << rock.coord.j << "," << rock.coord.k << ")"
              << std::endl;

    ToolResult break_result = break_bond(target, target2, 0.6f, strong_org);
    std::cout << "  BREAK bond: success=" << (break_result.success ? "yes" : "no")
              << " energy=" << break_result.energy_cost
              << " target_active=" << (target.active ? "yes" : "no") << std::endl;

    YieldMatrix soft_mat = YieldMatrix::default_organic();
    for (uint32_t v = 0; v < YIELD_VERTICES; v++)
        soft_mat.pillar_to_vertex[Integrity][v] = vn::fp20_t(0.2f);
    VoxelCell shape_target = make_cell({16, 0, 0, 0}, soft_mat, 0.5f, 103);
    ToolResult shape_result = shape_voxel(shape_target, organic_mat, 1.0f, strong_org);
    std::cout << "  SHAPE rock->organic: success=" << (shape_result.success ? "yes" : "no")
              << " energy=" << shape_result.energy_cost << std::endl;

    VoxelCell join_a = make_cell({20, 0, 0, 0}, organic_mat, 0.5f, 104);
    VoxelCell join_b = make_cell({25, 0, 0, 0}, organic_mat, 0.5f, 105);
    ToolResult join_result = join_voxels(join_a, join_b, 0.5f, strong_org);
    std::cout << "  JOIN distant voxels: success=" << (join_result.success ? "yes" : "no")
              << " bond_formed=" << (join_result.bond_formed ? "yes" : "no")
              << " new_pos=(" << join_a.coord.i << "," << join_a.coord.j << "," << join_a.coord.k << ")"
              << std::endl;
    std::cout << std::endl;

    // ========== 4. Material Science ==========
    std::cout << "--- Material Science ---" << std::endl;

    YieldMatrix diamond_like = YieldMatrix::default_rock();
    for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
        diamond_like.pillar_to_vertex[Integrity][v] = vn::fp20_t(0.95f);
        diamond_like.pillar_to_vertex[Cohesion][v] = vn::fp20_t(0.9f);
    }

    Molecule diamond_crystal;
    diamond_crystal.molecule_id = 100;
    for (int x = 0; x < 3; x++)
        for (int y = 0; y < 3; y++)
            for (int z = 0; z < 3; z++) {
                diamond_crystal.add_atom(make_cell(
                    {x, y, z, (x + y + z) & 1}, diamond_like, 0.3f,
                    static_cast<uint32_t>(200 + x * 9 + y * 3 + z)));
            }

    BulkMaterialProperties diamond_props = compute_bulk_properties(diamond_crystal);
    std::cout << "  Diamond-like crystal (" << diamond_crystal.atom_count() << " atoms):" << std::endl;
    std::cout << "    Strength=" << std::fixed << std::setprecision(1) << diamond_props.tensile_strength
              << " Hardness=" << diamond_props.hardness
              << " Melt=" << diamond_props.melting_point << "K" << std::endl;
    std::cout << "    Conductivity=" << std::setprecision(4) << diamond_props.conductivity
              << " Density=" << diamond_props.density
              << " Transparency=" << diamond_props.optical_transparency
              << " Flexibility=" << diamond_props.flexibility << std::endl;
    std::cout << "    Category: " << material_category(diamond_props) << std::endl;

    YieldMatrix copper_like = YieldMatrix::default_organic();
    for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
        copper_like.pillar_to_vertex[Flux][v] = vn::fp20_t(0.9f);
        copper_like.pillar_to_vertex[Cohesion][v] = vn::fp20_t(0.7f);
    }

    Molecule copper_wire;
    copper_wire.molecule_id = 101;
    for (int i = 0; i < 8; i++) {
        copper_wire.add_atom(make_cell({i, 0, 0, 0}, copper_like, 0.3f,
                             static_cast<uint32_t>(300 + i)));
    }

    BulkMaterialProperties copper_props = compute_bulk_properties(copper_wire);
    std::cout << "  Copper-like wire (" << copper_wire.atom_count() << " atoms):" << std::endl;
    std::cout << "    Strength=" << copper_props.tensile_strength
              << " Hardness=" << copper_props.hardness
              << " Conductivity=" << copper_props.conductivity
              << " Flexibility=" << copper_props.flexibility << std::endl;
    std::cout << "    Category: " << material_category(copper_props) << std::endl;
    std::cout << std::endl;

    // ========== 5. Material Database ==========
    std::cout << "--- Material Database ---" << std::endl;
    MaterialDatabase db;
    db.learn_material(diamond_crystal);
    db.learn_material(copper_wire);

    std::cout << "  Known materials: " << db.known_materials.size() << std::endl;
    for (const auto& [hash, mat] : db.known_materials) {
        (void)hash;
        std::cout << "    Hash=" << hash
                  << " category=" << material_category(mat.properties)
                  << " strength=" << mat.properties.tensile_strength
                  << " usage=" << mat.usage_count << std::endl;
    }

    BulkMaterialProperties best = db.find_best(50.0f, 100.0f);
    std::cout << "  Best for strength>50, hardness>100: "
              << material_category(best)
              << " (strength=" << best.tensile_strength
              << " hardness=" << best.hardness << ")" << std::endl;
    std::cout << std::endl;

    std::cout << "Demo complete." << std::endl;
    return 0;
}
