#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <cstdio>
#include <cstring>
#include <vector>

// VNES headers (relative to Van_Nueman_Engine root)
#include "../../../vnes/variant_types.h"
#include "../../../vnes/variant_registry.h"
#include "../../../vnes/vnes_generator.h"
#include "../../../vnes/lattice.h"
#include "../../../vnes/rule_matcher.h"
#include "../../../vnes/constraint_solver.h"

namespace py = pybind11;
using namespace pybind11::literals;

// ── Helper: variant metadata to Python dict ────────────────
static py::dict variant_metadata_to_dict(const VariantMetadata* md) {
    if (!md) return py::dict();
    return py::dict(
        "class_id"_a=std::string(md->class_id.id, strnlen(md->class_id.id, 16)),
        "load_bearing"_a=md->load_bearing,
        "compressive_priority"_a=md->compressive_priority,
        "fluid_priority"_a=md->fluid_priority,
        "bio_support"_a=static_cast<uint8_t>(md->bio_support),
        "square_face_role"_a=static_cast<uint8_t>(md->square_face_role),
        "hex_face_role"_a=static_cast<uint8_t>(md->hex_face_role),
        "frame_thickness_mm"_a=md->frame_thickness_mm,
        "supportless"_a=md->supportless,
        "min_wall_mm"_a=md->min_wall_mm,
        "min_feature_mm"_a=md->min_feature_mm
    );
}

// ── Mesh generation → numpy arrays ─────────────────────────
static py::tuple generate_mesh_py(const std::string& variant_id,
                                  uint32_t resolution = 128,
                                  bool cleanup = true)
{
    VariantClassID vid;
    std::memset(vid.id, 0, 16);
    std::memcpy(vid.id, variant_id.c_str(),
                std::min(variant_id.size(), (size_t)15));

    VNESGenConfig cfg;
    cfg.variant_id = vid;
    cfg.grid_resolution = resolution;
    cfg.cleanup = cleanup;
    cfg.output_filename = nullptr;

    float diameter = cfg.circumradius * 2.0f;
    float origin = -cfg.circumradius;
    float spacing = diameter / cfg.grid_resolution;

    VNESGrid3D grid(
        cfg.grid_resolution, cfg.grid_resolution, cfg.grid_resolution,
        origin, origin, origin, spacing
    );

    auto sdf_fn = [&](vnes_vec3 p) -> float {
        return variant_sdf(p, cfg.variant_id);
    };
    sample_sdf_to_grid(grid, sdf_fn);

    VNESMesh mesh;
    marching_cubes(grid, mesh);

    if (cfg.cleanup)
        mesh_cleanup(mesh);

    // Convert to numpy arrays
    size_t n_verts = mesh.vertices.size();
    size_t n_tris = mesh.triangle_count();

    py::array_t<float> verts(py::array::ShapeContainer{(py::ssize_t)n_verts, (py::ssize_t)3});
    py::array_t<uint32_t> tris(py::array::ShapeContainer{(py::ssize_t)n_tris, (py::ssize_t)3});
    py::array_t<float> norms(py::array::ShapeContainer{(py::ssize_t)n_verts, (py::ssize_t)3});

    auto v = verts.mutable_unchecked<2>();
    auto t = tris.mutable_unchecked<2>();
    auto n = norms.mutable_unchecked<2>();

    for (size_t i = 0; i < n_verts; i++) {
        v(i, 0) = mesh.vertices[i].x;
        v(i, 1) = mesh.vertices[i].y;
        v(i, 2) = mesh.vertices[i].z;
        n(i, 0) = mesh.normals[i].x;
        n(i, 1) = mesh.normals[i].y;
        n(i, 2) = mesh.normals[i].z;
    }
    for (size_t i = 0; i < n_tris; i++) {
        t(i, 0) = mesh.indices[i * 3];
        t(i, 1) = mesh.indices[i * 3 + 1];
        t(i, 2) = mesh.indices[i * 3 + 2];
    }

    return py::make_tuple(verts, tris, norms);
}

// ── Lattice placement → list of cell dicts ─────────────────
static py::list place_lattice_py(const py::dict& spec) {
    // spec: { "width": int, "height": int, "depth": int,
    //         "variant": str (optional),
    //         "spacing": float (optional) }
    int w = spec["width"].cast<int>();
    int h = spec["height"].cast<int>();
    int d = spec["depth"].cast<int>();

    std::string vid = spec.contains("variant")
        ? spec["variant"].cast<std::string>()
        : "VNE-A-250";

    const VariantMetadata* md = lookup_variant(vid.c_str());
    if (!md) {
        throw std::runtime_error("Unknown variant: " + vid);
    }

    ConstraintSolver solver;
    py::list results;

    for (int z = 0; z < d; z++) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                // BCC lattice: even parity only for simple cubic arrangement
                LatticeCoord coord(x * 2, y * 2, z * 2);
                auto r = solver.place_cell(coord, md);
                if (r.result == PlacementResult::Success) {
                    results.append(py::dict(
                        "i"_a=coord.i, "j"_a=coord.j, "k"_a=coord.k,
                        "variant"_a=vid,
                        "neighbors"_a=r.neighbor_count
                    ));
                }
            }
        }
    }
    return results;
}

// ── Pybind11 module ────────────────────────────────────────
PYBIND11_MODULE(vnes_cpp, m) {
    m.doc() = "VNES — Van Neumann Ecosystem Substrate C++ bindings";

    // ── Registry ───────────────────────────────────────────
    m.def("lookup_variant", [](const std::string& id) -> py::object {
        auto* md = lookup_variant(id.c_str());
        if (!md) return py::none();
        return variant_metadata_to_dict(md);
    }, "Look up variant metadata by ID string");

    m.def("get_all_variants", []() {
        py::list result;
        for (uint32_t i = 0; i < VNES_VARIANT_COUNT; i++) {
            result.append(variant_metadata_to_dict(VNES_ALL_VARIANTS[i]));
        }
        return result;
    }, "Return all 5 variant metadata dicts");

    m.def("faces_compatible", [](uint8_t a, uint8_t b) {
        return faces_compatible(static_cast<FaceRole>(a),
                                static_cast<FaceRole>(b));
    }, "Check if two FaceRole values are compatible");

    // ── FaceRole constants ─────────────────────────────────
    py::enum_<FaceRole>(m, "FaceRole")
        .value("LoadBearing", FaceRole::LoadBearing)
        .value("StructuralFlange", FaceRole::StructuralFlange)
        .value("StaticStructural", FaceRole::StaticStructural)
        .value("StructuralSeal", FaceRole::StructuralSeal)
        .value("CapillaryFlow", FaceRole::CapillaryFlow)
        .value("FluidAirExchange", FaceRole::FluidAirExchange)
        .value("HydroponicFlow", FaceRole::HydroponicFlow)
        .value("MainVascularPath", FaceRole::MainVascularPath)
        .value("UtilityTrunk", FaceRole::UtilityTrunk)
        .export_values();

    // ── Constants ──────────────────────────────────────────
    m.attr("CELL_RADIUS_MM") = VNES_CELL_RADIUS_MM;
    m.attr("CELL_SPACING_MM") = VNES_CELL_SPACING_MM;
    m.attr("HALF_SPACING_MM") = VNES_HALF_SPACING_MM;
    m.attr("FRAME_THICKNESS_MM") = VNES_FRAME_THICKNESS_MM;
    m.attr("PEG_DIAMETER_MM") = VNES_PEG_DIAMETER_MM;

    // ── Mesh generation ────────────────────────────────────
    m.def("generate_mesh", &generate_mesh_py,
          "variant_id"_a, "resolution"_a=128, "cleanup"_a=true,
          "Generate mesh for a variant. Returns (verts, tris, norms).");

    // ── LatticeCoord ───────────────────────────────────────
    py::class_<LatticeCoord>(m, "LatticeCoord")
        .def(py::init<int32_t, int32_t, int32_t>(),
             "i"_a=0, "j"_a=0, "k"_a=0)
        .def_readwrite("i", &LatticeCoord::i)
        .def_readwrite("j", &LatticeCoord::j)
        .def_readwrite("k", &LatticeCoord::k)
        .def("__repr__", [](const LatticeCoord& c) {
            return "<LatticeCoord " + std::to_string(c.i) + ","
                 + std::to_string(c.j) + "," + std::to_string(c.k) + ">";
        })
        .def("parity", &LatticeCoord::parity)
        .def("neighbor", &LatticeCoord::neighbor, "face_idx"_a)
        .def("face_to", &LatticeCoord::face_to, "other"_a)
        .def("to_world", [](const LatticeCoord& c) {
            float x, y, z;
            c.to_world(x, y, z);
            return py::make_tuple(x, y, z);
        });

    // ── ConstraintSolver ───────────────────────────────────
    py::class_<ConstraintSolver>(m, "ConstraintSolver")
        .def(py::init<>())
        .def("place_cell", [](ConstraintSolver& s,
                               int32_t i, int32_t j, int32_t k,
                               const std::string& variant_id) -> py::dict {
            auto* md = lookup_variant(variant_id.c_str());
            if (!md)
                throw std::runtime_error("Unknown variant: " + variant_id);
            auto r = s.place_cell(LatticeCoord(i, j, k), md);
            return py::dict(
                "success"_a=(r.result == PlacementResult::Success),
                "result"_a=static_cast<uint8_t>(r.result),
                "neighbor_count"_a=r.neighbor_count,
                "incompat_count"_a=r.incompat_count
            );
        }, "i"_a, "j"_a, "k"_a, "variant_id"_a)
        .def("place_auto", [](ConstraintSolver& s,
                               int32_t i, int32_t j, int32_t k) -> py::dict {
            auto r = s.place_cell_auto(LatticeCoord(i, j, k));
            return py::dict(
                "success"_a=(r.result == PlacementResult::Success),
                "result"_a=static_cast<uint8_t>(r.result),
                "variant"_a=(r.placed_metadata
                    ? std::string(r.placed_metadata->class_id.id,
                                  strnlen(r.placed_metadata->class_id.id, 16))
                    : ""),
                "neighbor_count"_a=r.neighbor_count
            );
        }, "i"_a, "j"_a, "k"_a)
        .def("reset", &ConstraintSolver::reset)
        .def_property_readonly("cell_count", [](ConstraintSolver& s) {
            return s.lattice().size();
        })
        .def("validate_all", [](ConstraintSolver& s) {
            return validate_all(s.lattice());
        });

    // ── Lattice placement (batch) ──────────────────────────
    m.def("place_lattice", &place_lattice_py, "spec"_a,
          "Place a rectangular lattice of VNES cells. "
          "spec: {'width':int, 'height':int, 'depth':int, "
          "'variant':str (optional)}");
}
