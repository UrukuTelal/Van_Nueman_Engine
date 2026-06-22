#pragma once

#include "kelvin_hull.h"

// ── Shared: clamp point inside Kelvin hull ────────────────
// Every variant SDF combines internal TPMS geometry with the
// Kelvin cell outer shell. We use the hull as a bounding volume:
// outside the hull, return the hull SDF; inside, combine
// internal TPMS with optional subtraction for features.

inline float bound_to_kelvin(float internal_sdf, float hull_sdf) {
    // The cell is the intersection of internal geometry with the hull.
    // Outside the hull, the hull distance dominates.
    return std::max(internal_sdf, hull_sdf);
}

// ── Variant A: Structural Core (Schwarz P) ────────────────
// Interior: triply periodic Schwarz P minimal surface, three
// hierarchical scales (1 mm, 2 mm, 3 mm pores), fully interconnected.

inline float variant_a_sdf(vnes_vec3 p) {
    float circumradius = VNES_CELL_RADIUS_MM;
    float frame = VNES_FRAME_THICKNESS_MM;

    // Hull
    float hull = kelvin_hull_sdf(p, circumradius, frame);

    // Three-scale Schwarz P
    float scale1 = 2.0f * 3.14159f / 3.0f;  // 3 mm period
    float scale2 = 2.0f * 3.14159f / 2.0f;  // 2 mm period
    float scale3 = 2.0f * 3.14159f / 1.0f;  // 1 mm period

    float sp1 = sdf_schwarz_p_shell(p, scale1, 0.25f);
    float sp2 = sdf_schwarz_p_shell(p, scale2, 0.20f);
    float sp3 = sdf_schwarz_p_shell(p, scale3, 0.15f);

    // Combine all three scales via union
    float internal = op_union(op_union(sp1, sp2), sp3);

    // Bound to Kelvin hull
    return bound_to_kelvin(internal, hull);
}

// ── Variant B: Harvest Surface (gradient gyroid + openings) ──
// Interior: density-gradient gyroid transitioning from dense
// (3 mm) to open (6 mm) toward the designated growth face.
// The growth face has open chambers (20-30 mm diameter).

inline float variant_b_sdf(vnes_vec3 p) {
    float circumradius = VNES_CELL_RADIUS_MM;
    float frame = VNES_FRAME_THICKNESS_MM;

    // Hull
    float hull = kelvin_hull_sdf(p, circumradius, frame);

    // Determine growth face direction (assume +Z is growth)
    // t goes from 0 at core to 1 at +Z face
    float t = (p.z / circumradius) * 0.5f + 0.5f;
    t = std::max(0.0f, std::min(1.0f, t));

    // Gradient gyroid
    float base_scale = 2.0f * 3.14159f / 4.0f;
    float internal = sdf_gradient_gyroid(p, base_scale,
                                          base_scale * 0.8f,   // scale_min (dense core)
                                          base_scale * 0.4f,   // scale_max (open growth face)
                                          t, 0.25f);

    // Add open chambers at growth face (+Z)
    // Place 3 chambers in a triangle pattern
    if (t > 0.7f) {
        // Chamber centers at the growth face
        vnes_vec3 chambers[3] = {
            { 15.0f,  15.0f, circumradius * 0.9f},
            {-15.0f,  15.0f, circumradius * 0.9f},
            {  0.0f, -20.0f, circumradius * 0.9f},
        };
        float chamber_r = 12.5f;  // 25 mm diameter
        float d_chamber = 1e10f;
        for (int i = 0; i < 3; i++) {
            float dc = length(p - chambers[i]) - chamber_r;
            d_chamber = op_union(d_chamber, dc);
        }
        // Subtract chambers from internal structure
        internal = op_subtract(internal, d_chamber);
    }

    return bound_to_kelvin(internal, hull);
}

// ── Variant C: Universal Rooting (open-cell gyroid) ───────
// Interior: open-cell gyroid with 3-6 mm pores, fully connected
// with no dead ends or bottlenecks. Minimum hydraulic pathway 3 mm.

inline float variant_c_sdf(vnes_vec3 p) {
    float circumradius = VNES_CELL_RADIUS_MM;
    float frame = VNES_FRAME_THICKNESS_MM;

    // Hull
    float hull = kelvin_hull_sdf(p, circumradius, frame);

    // Single-scale gyroid with thicker walls for open structure
    float scale = 2.0f * 3.14159f / 5.0f;
    float internal = sdf_gyroid_shell(p, scale, 0.35f);

    // Erode the gyroid to create larger open channels
    // (subtract a spherical erosion kernel)
    // This creates the 3-6 mm interconnected pore space
    float erosion = sdf_gyroid(p, scale);
    internal = std::max(internal, -erosion * 0.3f);

    return bound_to_kelvin(internal, hull);
}

// ── Variant D: Transport Spine (3 vascular trunks) ────────
// Interior: three 20 mm diameter smooth-walled conduits
// intersecting at the cell center via Steinmetz-style junction,
// surrounded by gyroid support lattice (3-6 mm pores).

inline float variant_d_sdf(vnes_vec3 p) {
    float circumradius = VNES_CELL_RADIUS_MM;
    float frame = VNES_FRAME_THICKNESS_MM;

    // Hull
    float hull = kelvin_hull_sdf(p, circumradius, frame);

    // Three vascular trunks along X, Y, Z axes
    // Each is a cylinder of 10 mm radius (20 mm diameter)
    float trunk_r = 10.0f;
    float trunk_x = sdf_cylinder({p.x, p.y, p.z}, trunk_r, circumradius);
    float trunk_y = sdf_cylinder({p.y, p.z, p.x}, trunk_r, circumradius);
    float trunk_z = sdf_cylinder({p.z, p.x, p.y}, trunk_r, circumradius);

    // Union of all three trunks
    float conduits = op_union(op_union(trunk_x, trunk_y), trunk_z);

    // Add Steinmetz-style blend at junction
    // Smooth the intersection region
    float junction_r = 4.0f;
    float d_center = length(p) - trunk_r;
    if (d_center < junction_r) {
        conduits = op_smooth_union(conduits, d_center, junction_r);
    }

    // Gyroid support lattice in the remaining volume
    float g_scale = 2.0f * 3.14159f / 5.0f;
    float lattice = sdf_gyroid_shell(p, g_scale, 0.25f);

    // The interior is: lattice everywhere, with conduits subtracted
    float internal = op_subtract(lattice, conduits);

    return bound_to_kelvin(internal, hull);
}

// ── Variant E: Utility Service (corridors + sensor mounts) ──
// Interior: gyroid support lattice with dedicated cable conduits,
// fluid channels, and sensor mounting bays.

inline float variant_e_sdf(vnes_vec3 p) {
    float circumradius = VNES_CELL_RADIUS_MM;
    float frame = VNES_FRAME_THICKNESS_MM;

    // Hull
    float hull = kelvin_hull_sdf(p, circumradius, frame);

    // Gyroid support lattice
    float g_scale = 2.0f * 3.14159f / 5.0f;
    float lattice = sdf_gyroid_shell(p, g_scale, 0.25f);

    // Cable conduits (4 channels, 8 mm diameter)
    float cable_r = 4.0f;
    float cable_spacing = 20.0f;
    float d_cables = 1e10f;
    for (int i = -1; i <= 2; i++) {
        vnes_vec3 offset = {i * cable_spacing, 0, 0};
        float dc = sdf_cylinder(p - offset, cable_r, circumradius);
        d_cables = op_union(d_cables, dc);
    }

    // Fluid channels (2 channels, 6 mm diameter)
    float fluid_r = 3.0f;
    float d_fluid = 1e10f;
    for (int i = -1; i <= 0; i++) {
        vnes_vec3 offset = {0, i * 25.0f, 0};
        float df = sdf_cylinder(p - offset, fluid_r, circumradius);
        d_fluid = op_union(d_fluid, df);
    }

    // Inspection port (30 mm diameter sphere)
    float d_port = sdf_sphere(p, 15.0f);

    // Combine: lattice minus (cables + fluid + port)
    float internal = lattice;
    internal = op_subtract(internal, d_cables);
    internal = op_subtract(internal, d_fluid);
    internal = op_subtract(internal, d_port);

    return bound_to_kelvin(internal, hull);
}

// ── Variant dispatch ──────────────────────────────────────
// Returns the SDF for a given variant class ID.

inline float variant_sdf(vnes_vec3 p, const VariantClassID& id) {
    if (id == VNE_A_250.class_id) return variant_a_sdf(p);
    if (id == VNE_B_250.class_id) return variant_b_sdf(p);
    if (id == VNE_C_250.class_id) return variant_c_sdf(p);
    if (id == VNE_D_250.class_id) return variant_d_sdf(p);
    if (id == VNE_E_250.class_id) return variant_e_sdf(p);
    return 1e10f;  // unknown variant
}
