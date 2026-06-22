#pragma once

#include "export.h"
#include "variant_sdf.h"
#include "variant_registry.h"
#include <cstdio>
#include <cstring>

// ── Generation parameters ─────────────────────────────────
struct VNESGenConfig {
    VariantClassID variant_id;
    uint32_t grid_resolution;          // cells per axis (default 128)
    float circumradius;                // default VNES_CELL_RADIUS_MM
    float frame_thickness;             // default VNES_FRAME_THICKNESS_MM
    const char* output_filename;
    const char* output_format;         // "stl", "ply", "ascii.stl", or nullptr (auto)
    bool cleanup;                      // run dedup + degenerate removal

    VNESGenConfig()
        : grid_resolution(VNES_DEFAULT_GRID_RES)
        , circumradius(VNES_CELL_RADIUS_MM)
        , frame_thickness(VNES_FRAME_THICKNESS_MM)
        , output_filename("output.stl")
        , output_format(nullptr)
        , cleanup(true)
    {
        variant_id = VariantClassID{"VNE-A-250"};
    }
};

// ── Generate single variant mesh ──────────────────────────
// Orchestrates: SDF → grid sampling → marching cubes →
// cleanup → export.

inline bool generate_variant_mesh(const VNESGenConfig& cfg) {
    // 1. Compute grid bounds from circumscribed sphere diameter
    float diameter = cfg.circumradius * 2.0f;
    float origin = -cfg.circumradius;
    float spacing = diameter / cfg.grid_resolution;

    VNESGrid3D grid(
        cfg.grid_resolution,
        cfg.grid_resolution,
        cfg.grid_resolution,
        origin, origin, origin,
        spacing
    );

    // 2. Sample SDF onto grid
    auto sdf_fn = [&](vnes_vec3 p) -> float {
        return variant_sdf(p, cfg.variant_id);
    };
    sample_sdf_to_grid(grid, sdf_fn);

    // 3. Marching cubes
    VNESMesh mesh;
    marching_cubes(grid, mesh);

    if (mesh.triangle_count() == 0) {
        fprintf(stderr, "VNES: no triangles generated for %s\n", cfg.variant_id.id);
        return false;
    }

    // 4. Cleanup (optional)
    if (cfg.cleanup) {
        mesh_cleanup(mesh);
    }

    // 5. Determine output filename
    const char* out_file = cfg.output_filename;
    if (!out_file || out_file[0] == '\0') {
        // Generate filename from variant ID and format
        static char auto_name[64];
        snprintf(auto_name, sizeof(auto_name), "%s.stl", cfg.variant_id.id);
        out_file = auto_name;
    }

    // 6. Export
    bool ok;
    if (cfg.output_format) {
        if (strcmp(cfg.output_format, "ply") == 0) {
            ok = export_ply(mesh, out_file);
        } else if (strcmp(cfg.output_format, "ascii.stl") == 0) {
            ok = export_stl_ascii(mesh, out_file);
        } else {
            ok = export_stl_binary(mesh, out_file);
        }
    } else {
        ok = export_mesh(mesh, out_file);
    }

    if (ok) {
        printf("VNES: generated %s -> %s (%u vertices, %u triangles)\n",
               cfg.variant_id.id, out_file,
               (unsigned)mesh.vertices.size(),
               (unsigned)mesh.triangle_count());
    } else {
        fprintf(stderr, "VNES: export failed for %s\n", cfg.variant_id.id);
    }

    return ok;
}

// ── Generate all 5 variants ───────────────────────────────

inline bool generate_all_variants(uint32_t grid_resolution = VNES_DEFAULT_GRID_RES) {
    const VariantClassID ids[] = {
        VariantClassID{"VNE-A-250"},
        VariantClassID{"VNE-B-250"},
        VariantClassID{"VNE-C-250"},
        VariantClassID{"VNE-D-250"},
        VariantClassID{"VNE-E-250"},
    };
    const char* names[] = {
        "VNE-A-250",
        "VNE-B-250",
        "VNE-C-250",
        "VNE-D-250",
        "VNE-E-250",
    };

    bool all_ok = true;
    for (int i = 0; i < 5; i++) {
        VNESGenConfig cfg;
        cfg.variant_id = ids[i];
        cfg.grid_resolution = grid_resolution;

        char filename[64];
        snprintf(filename, sizeof(filename), "%s.stl", names[i]);
        cfg.output_filename = filename;

        bool ok = generate_variant_mesh(cfg);
        if (!ok) all_ok = false;
    }
    return all_ok;
}
