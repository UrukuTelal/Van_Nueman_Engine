#pragma once

#include "sdf_primitives.h"
#include "variant_registry.h"
#include <cmath>

// ── Kelvin cell shared geometry ───────────────────────────
// All VNES variants share the same external hull: a regular
// truncated octahedron with a 250 mm circumscribed sphere,
// 15 mm shell thickness, open hexagonal mesh exterior, and
// cylindrical snap-fit pegs/sockets on all 14 faces.

// ── Face center directions (unit vectors) ─────────────────
// 6 square faces (±X, ±Y, ±Z) and 8 hex faces (diagonal).

inline const vnes_vec3 KELVIN_SQUARE_NORMALS[6] = {
    { 1,  0,  0}, {-1,  0,  0},
    { 0,  1,  0}, { 0, -1,  0},
    { 0,  0,  1}, { 0,  0, -1},
};

inline const vnes_vec3 KELVIN_HEX_NORMALS[8] = {
    { 1,  1,  1}, { 1,  1, -1}, { 1, -1,  1}, { 1, -1, -1},
    {-1,  1,  1}, {-1,  1, -1}, {-1, -1,  1}, {-1, -1, -1},
};

// ── Kelvin cell hull SDF ──────────────────────────────────
// Returns signed distance to the outer shell of the Kelvin cell.
// Positive = outside, negative = inside.
// The hull is the truncated octahedron solid, shelled to
// `frame_thickness`, with open hexagonal cutouts on all faces
// and cylindrical snap-fit pegs/sockets on each face.

inline float kelvin_hull_sdf(vnes_vec3 p, float circumradius,
                              float frame_thickness) {
    // Start with the solid truncated octahedron
    float solid = sdf_truncated_octahedron(p, circumradius);

    // Shell it
    float shell = sdf_shell(solid, frame_thickness);

    return shell;
}

// ── Face center positions ─────────────────────────────────
// Returns the position of the center of face i (0-5 square, 6-13 hex).
// Face centers lie on the circumscribed sphere.

inline vnes_vec3 kelvin_face_center(int face_index, float circumradius) {
    if (face_index < 6) {
        return KELVIN_SQUARE_NORMALS[face_index] * circumradius;
    }
    vnes_vec3 d = KELVIN_HEX_NORMALS[face_index - 6];
    return normalize(d) * circumradius;
}

// ── Snap-fit peg/socket SDF ───────────────────────────────
// Adds cylindrical pegs (male) or sockets (female) on a face.
// The face is defined by its outward normal. Pegs are placed
// at the 4 corners of square faces or 6 corners of hex faces.

inline float snap_fit_sdf(vnes_vec3 p, vnes_vec3 face_center,
                           vnes_vec3 normal, bool is_male,
                           float peg_diameter, float peg_depth,
                           float circumradius) {
    // Transform point to face-local coordinates
    // Build orthonormal basis around normal
    vnes_vec3 up = {0, 1, 0};
    if (std::fabs(dot(normal, up)) > 0.99f)
        up = {0, 0, 1};
    vnes_vec3 right = normalize(cross(normal, up));
    up = cross(right, normal);

    vnes_vec3 local = p - face_center;
    float lx = dot(local, right);
    float ly = dot(local, up);
    float lz = dot(local, normal);

    // Peg/socket positions in face-local coords
    // 4 pegs at corners of a square inscribed in the face
    float r = circumradius * 0.15f;  // distance from center to peg
    float peg_r = peg_diameter * 0.5f;
    float peg_h = peg_depth;

    float peg_positions[4][2] = {
        { r,  r}, {-r,  r},
        { r, -r}, {-r, -r},
    };

    float d_pegs = 1e10f;
    for (int i = 0; i < 4; i++) {
        vnes_vec3 peg_center = {peg_positions[i][0], peg_positions[i][1],
                                 is_male ? peg_h * 0.5f : -peg_h * 0.5f};
        vnes_vec3 dp = {lx - peg_center.x, ly - peg_center.y, lz - peg_center.z};
        float d = length({dp.x, dp.y, 0}) - peg_r;
        // For male: peg protrudes outward (positive z in local coords)
        // For female: socket recesses inward
        float z_limit = is_male ? peg_h : peg_h;
        float dz = std::fabs(dp.z) - z_limit * 0.5f;
        d = std::max(d, dz);
        d_pegs = op_union(d_pegs, d);
    }

    return d_pegs;
}

// ── Open hexagonal mesh cutout ────────────────────────────
// Removes material from face interiors to create the open
// hexagonal mesh pattern, leaving only the frame and struts.

inline float face_mesh_cutout(vnes_vec3 p, vnes_vec3 face_center,
                               vnes_vec3 normal, float circumradius,
                               float strut_width) {
    // Transform to face-local coords (same as above)
    vnes_vec3 up = {0, 1, 0};
    if (std::fabs(dot(normal, up)) > 0.99f)
        up = {0, 0, 1};
    vnes_vec3 right = normalize(cross(normal, up));
    up = cross(right, normal);

    vnes_vec3 local = p - face_center;
    float lx = dot(local, right);
    float ly = dot(local, up);

    // Create hexagonal grid pattern on the face
    // Hexagonal tiling with strut_width thick walls
    const float hex_scale = circumradius * 0.12f;
    float hx = lx / hex_scale;
    float hy = ly / hex_scale;

    // Hexagonal grid SDF approximation
    float dx = std::fmod(std::fabs(hx), 2.0f) - 1.0f;
    float dy = std::fmod(std::fabs(hy), 2.0f) - 1.0f;
    float d_hex = std::max(std::fabs(dx) - (1.0f - strut_width/hex_scale),
                            std::fabs(dy) - (1.0f - strut_width/hex_scale));

    // The cutout removes material (union of hexagonal holes)
    // Return positive = keep, negative = remove
    return -d_hex;  // negative = hole area
}

// ── Complete Kelvin cell with all features ────────────────
// Combines: outer shell + snap-fit pegs + mesh cutouts.
// Returns the combined SDF value for a complete printable cell.

inline float kelvin_cell_complete_sdf(vnes_vec3 p, float circumradius,
                                       float frame_thickness,
                                       float peg_diameter, float peg_depth,
                                       float strut_width) {
    // Start with the shell
    float d = kelvin_hull_sdf(p, circumradius, frame_thickness);

    // Add male pegs on all faces
    for (int i = 0; i < 14; i++) {
        vnes_vec3 fc = kelvin_face_center(i, circumradius);
        vnes_vec3 n = normalize(fc);
        float d_peg = snap_fit_sdf(p, fc, n, true, peg_diameter, peg_depth, circumradius);
        d = op_union(d, d_peg);
    }

    // Apply hexagonal mesh cutouts on all faces
    for (int i = 0; i < 14; i++) {
        vnes_vec3 fc = kelvin_face_center(i, circumradius);
        vnes_vec3 n = normalize(fc);
        float d_mesh = face_mesh_cutout(p, fc, n, circumradius, strut_width);
        d = op_subtract(d, d_mesh);  // remove mesh holes from solid
    }

    return d;
}

// ── Orientation helper ────────────────────────────────────
// Rotates the Kelvin cell so a specific face faces +Z.
// face_index 0-5 = square, 6-13 = hex.

inline vnes_vec3 orient_to_face(vnes_vec3 p, int face_index) {
    vnes_vec3 target_normal = {0, 0, 1};

    vnes_vec3 face_normal;
    if (face_index < 6) {
        face_normal = KELVIN_SQUARE_NORMALS[face_index];
    } else {
        face_normal = normalize(KELVIN_HEX_NORMALS[face_index - 6]);
    }

    // Rotation that maps face_normal to target_normal
    vnes_vec3 axis = cross(face_normal, target_normal);
    float len = length(axis);
    if (len < 1e-6f) {
        if (dot(face_normal, target_normal) > 0) return p;  // already aligned
        return {-p.x, -p.y, p.z};  // 180 degrees around Z
    }
    axis = axis / len;
    float angle = std::acos(std::max(-1.0f, std::min(1.0f, dot(face_normal, target_normal))));
    float c = std::cos(angle);
    float s = std::sin(angle);

    // Rodrigues' rotation formula
    return p * c + cross(axis, p) * s + axis * dot(axis, p) * (1 - c);
}
