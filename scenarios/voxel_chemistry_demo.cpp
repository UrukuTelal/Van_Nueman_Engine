#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include "../chemistry/ChemicalBond.h"
#include "../chemistry/Molecule.h"
#include "../chemistry/ReactionSystem.h"
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

    std::cout << "=== Voxel Chemistry Demo ===" << std::endl;
    std::cout << "Phase 8: Chemistry / Molecule Layer (Scale 10)" << std::endl;
    std::cout << std::endl;

    YieldMatrix oxygen_like = YieldMatrix::default_organic();
    YieldMatrix hydrogen_like = YieldMatrix::default_stem();
    YieldMatrix catalyst = YieldMatrix::default_rock();

    for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
        oxygen_like.pillar_to_vertex[Attraction][v] = vn::fp20_t(0.8f);
        oxygen_like.pillar_to_vertex[Flux][v] = vn::fp20_t(0.7f);
        oxygen_like.pillar_to_vertex[Integrity][v] = vn::fp20_t(0.6f);
        oxygen_like.pillar_to_vertex[Cohesion][v] = vn::fp20_t(0.5f);

        hydrogen_like.pillar_to_vertex[Attraction][v] = vn::fp20_t(0.4f);
        hydrogen_like.pillar_to_vertex[Flux][v] = vn::fp20_t(0.3f);
        hydrogen_like.pillar_to_vertex[Warmth][v] = vn::fp20_t(0.7f);
        hydrogen_like.pillar_to_vertex[Integrity][v] = vn::fp20_t(0.3f);

        catalyst.pillar_to_vertex[Awareness][v] = vn::fp20_t(0.8f);
        catalyst.pillar_to_vertex[Harm][v] = vn::fp20_t(0.1f);
        catalyst.pillar_to_vertex[Attraction][v] = vn::fp20_t(0.5f);
        catalyst.pillar_to_vertex[Cohesion][v] = vn::fp20_t(0.6f);
    }

    Molecule water;
    water.molecule_id = 1;

    VoxelCell o1 = make_atom({0, 0, 0, 0}, oxygen_like, 0.5f, 1);
    VoxelCell h1 = make_atom({1, 0, 0, 1}, hydrogen_like, 0.4f, 2);
    VoxelCell h2 = make_atom({0, 1, 0, 1}, hydrogen_like, 0.4f, 3);

    water.add_atom(o1);
    water.add_atom(h1);
    water.add_atom(h2);

    std::cout << "Molecule created with " << water.atom_count() << " atoms" << std::endl;
    std::cout << std::endl;

    std::cout << "=== Detecting Bonds ===" << std::endl;
    for (size_t i = 0; i < water.atoms.size(); i++) {
        for (size_t j = i + 1; j < water.atoms.size(); j++) {
            vn::fp20_t a_pillars[NumPillars], b_pillars[NumPillars];
            for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                for (uint32_t p = 0; p < NumPillars; p++) {
                    a_pillars[p] = water.atoms[i].pyramids[f].material_composition[p];
                    b_pillars[p] = water.atoms[j].pyramids[f].material_composition[p];
                }
            }

            ChemicalBond bond = detect_bond(
                bcc_hash(water.atoms[i].coord),
                bcc_hash(water.atoms[j].coord),
                a_pillars, b_pillars);

            if (bond.type != BondType::None) {
                water.add_bond(bond);
                std::cout << "  Bond between atom " << i << " and " << j
                          << ": " << bond_type_name(bond.type)
                          << " (strength=" << std::fixed << std::setprecision(3) << bond.strength
                          << ", energy=" << bond.energy << ")" << std::endl;
            }
        }
    }

    std::cout << std::endl;
    std::cout << "=== Molecule Properties ===" << std::endl;
    std::cout << "  Connected: " << (water.is_connected() ? "yes" : "no") << std::endl;
    std::cout << "  Bonds: " << water.bond_count() << std::endl;
    std::cout << "  Total bond energy: " << std::fixed << std::setprecision(1)
              << water.total_bond_energy() << " kJ/mol" << std::endl;
    std::cout << "  Polarity: " << std::fixed << std::setprecision(4) << water.polarity() << std::endl;
    std::cout << "  Avg Integrity: " << water.avg_pillar(Integrity).to_float() << std::endl;
    std::cout << std::endl;

    // Catalyst test
    VoxelCell cat_cell;
    cat_cell.init({5, 0, 0, 0}, vn::fp20_t(0.5f), 99, catalyst);
    for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
        for (uint32_t p = 0; p < NumPillars; p++) {
            cat_cell.pyramids[f].material_composition[p] = catalyst.pillar_to_vertex[p][0];
        }
    }

    std::vector<VoxelCell> env = { cat_cell };
    std::cout << "Catalyst present: "
              << (is_catalyst(cat_cell.pyramids[0].material_composition, 0.6f) ? "yes" : "no")
              << std::endl;
    std::cout << "Catalyst boost factor: " << catalyst_boost(env) << std::endl;
    std::cout << std::endl;

    // Reaction test
    Molecule water2;
    water2.molecule_id = 2;
    VoxelCell o2 = make_atom({2, 0, 0, 0}, oxygen_like, 0.5f, 4);
    VoxelCell h3 = make_atom({3, 0, 0, 1}, hydrogen_like, 0.4f, 5);
    water2.add_atom(o2);
    water2.add_atom(h3);

    ChemicalBond bond2 = detect_bond(
        bcc_hash(water2.atoms[0].coord),
        bcc_hash(water2.atoms[1].coord),
        oxygen_like.pillar_to_vertex[0],
        hydrogen_like.pillar_to_vertex[0]);
    if (bond2.type != BondType::None) water2.add_bond(bond2);

    std::cout << "=== Collision/Reaction Test ===" << std::endl;
    std::cout << "Molecule 1 atoms: " << water.atom_count()
              << ", bonds: " << water.bond_count() << std::endl;
    std::cout << "Molecule 2 atoms: " << water2.atom_count()
              << ", bonds: " << water2.bond_count() << std::endl;

    CollisionEvent ce = collide(water, water2, &env);
    std::cout << "  Can collide: " << (ce.approach_energy > 0 ? "yes" : "no") << std::endl;
    std::cout << "  Reacted: " << (ce.reacted ? "yes" : "no") << std::endl;
    std::cout << "  Energy released: " << std::fixed << std::setprecision(2)
              << ce.bond_energy_released << " kJ/mol" << std::endl;
    std::cout << "  Total bonds after: " << (water.bond_count() + water2.bond_count()) << std::endl;
    std::cout << std::endl;

    std::cout << "Demo complete." << std::endl;
    return 0;
}
