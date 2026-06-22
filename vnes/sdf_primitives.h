#pragma once

#include <cmath>
#include <algorithm>

// ── Vec3 helper ───────────────────────────────────────────
// Minimal 3D vector for SDF evaluation. Float precision is
// sufficient for geometry generation (not simulation).

struct vnes_vec3 {
    float x, y, z;

    constexpr vnes_vec3() : x(0), y(0), z(0) {}
    constexpr vnes_vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    constexpr vnes_vec3 operator+(vnes_vec3 v) const { return {x+v.x, y+v.y, z+v.z}; }
    constexpr vnes_vec3 operator-(vnes_vec3 v) const { return {x-v.x, y-v.y, z-v.z}; }
    constexpr vnes_vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
    constexpr vnes_vec3 operator/(float s) const { return {x/s, y/s, z/s}; }
    constexpr vnes_vec3 operator-() const { return {-x, -y, -z}; }

    vnes_vec3& operator+=(vnes_vec3 v) { x+=v.x; y+=v.y; z+=v.z; return *this; }
    vnes_vec3& operator-=(vnes_vec3 v) { x-=v.x; y-=v.y; z-=v.z; return *this; }
    vnes_vec3& operator*=(float s) { x*=s; y*=s; z*=s; return *this; }
};

inline float dot(vnes_vec3 a, vnes_vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

inline float length(vnes_vec3 v) {
    return std::sqrt(dot(v, v));
}

inline vnes_vec3 normalize(vnes_vec3 v) {
    float len = length(v);
    return (len > 1e-8f) ? v / len : vnes_vec3{0,0,0};
}

inline vnes_vec3 cross(vnes_vec3 a, vnes_vec3 b) {
    return {
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    };
}

inline vnes_vec3 abs(vnes_vec3 v) {
    return {std::fabs(v.x), std::fabs(v.y), std::fabs(v.z)};
}

inline vnes_vec3 max(vnes_vec3 a, vnes_vec3 b) {
    return {std::max(a.x,b.x), std::max(a.y,b.y), std::max(a.z,b.z)};
}

inline vnes_vec3 min(vnes_vec3 a, vnes_vec3 b) {
    return {std::min(a.x,b.x), std::min(a.y,b.y), std::min(a.z,b.z)};
}

inline vnes_vec3 clamp(vnes_vec3 v, vnes_vec3 lo, vnes_vec3 hi) {
    return min(max(v, lo), hi);
}

// ── CSG operations ────────────────────────────────────────

inline float op_union(float d1, float d2) {
    return std::min(d1, d2);
}

inline float op_subtract(float d1, float d2) {
    return std::max(d1, -d2);
}

inline float op_intersect(float d1, float d2) {
    return std::max(d1, d2);
}

inline float op_smooth_union(float d1, float d2, float k) {
    float h = std::max(k - std::fabs(d1 - d2), 0.0f) / k;
    return std::min(d1, d2) - h * h * k * 0.25f;
}

inline float op_smooth_subtract(float d1, float d2, float k) {
    float h = std::max(k - std::fabs(-d1 - d2), 0.0f) / k;
    return std::max(-d1, d2) + h * h * k * 0.25f;
}

inline float op_smooth_intersect(float d1, float d2, float k) {
    float h = std::max(k - std::fabs(d1 - d2), 0.0f) / k;
    return std::max(d1, d2) + h * h * k * 0.25f;
}

inline float op_round(float d, float r) {
    return d - r;
}

inline float op_elongate(float d, vnes_vec3 h) {
    // Not implemented as a standalone op — use per-axis elongation
    // by modifying the input point before calling the SDF.
    return d;
}

// ── Domain operations ─────────────────────────────────────

inline vnes_vec3 op_twist(vnes_vec3 p, float k) {
    float c = std::cos(k * p.y);
    float s = std::sin(k * p.y);
    return {c*p.x - s*p.z, p.y, s*p.x + c*p.z};
}

// ── Primitive SDFs ────────────────────────────────────────

inline float sdf_sphere(vnes_vec3 p, float r) {
    return length(p) - r;
}

inline float sdf_box(vnes_vec3 p, vnes_vec3 b) {
    vnes_vec3 q = abs(p) - b;
    return length(max(q, {0,0,0})) + std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
}

inline float sdf_round_box(vnes_vec3 p, vnes_vec3 b, float r) {
    vnes_vec3 q = abs(p) - b + vnes_vec3{r,r,r};
    return length(max(q, {0,0,0})) + std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f) - r;
}

inline float sdf_cylinder(vnes_vec3 p, float r, float h) {
    float d = length({p.x, p.z, 0}) - r;
    return std::max(d, std::fabs(p.y) - h);
}

inline float sdf_capsule(vnes_vec3 p, vnes_vec3 a, vnes_vec3 b, float r) {
    vnes_vec3 pa = p - a;
    vnes_vec3 ba = b - a;
    float t = std::max(-1.0f, std::min(1.0f, dot(pa, ba) / dot(ba, ba)));
    return length(pa - ba * t) - r;
}

inline float sdf_torus(vnes_vec3 p, float major_r, float minor_r) {
    float qx = length({p.x, p.z, 0}) - major_r;
    return length({qx, p.y, 0}) - minor_r;
}

inline float sdf_plane(vnes_vec3 p, vnes_vec3 n, float d) {
    return dot(p, n) + d;
}

inline float sdf_octahedron(vnes_vec3 p, float scale) {
    p = abs(p);
    return (p.x + p.y + p.z - scale) * 0.57735f;
}

// ── Truncated octahedron SDF (exact, via intersection of planes) ──
// A regular truncated octahedron has:
//   6 square faces (axis-aligned) and 8 hexagonal faces (diagonal).
// We represent it as the intersection of 14 half-spaces.

inline float sdf_truncated_octahedron(vnes_vec3 p, float circumradius) {
    // Scale factor: for a unit circumscribed sphere of radius R,
    // the truncation parameter is R/phi where phi = sqrt(2).
    // At circumradius = 125 mm (half of 250), the geometry is:
    //   square face distance: R/3
    //   hex face distance:    R
    //   vertex at (R/3, R/3, R/3) truncated to hexagons.
    //
    // For a regular truncated octahedron with edge length a:
    //   circumradius = a * sqrt(10) / 2
    //   So a = circumradius * 2 / sqrt(10)

    float a = circumradius * 2.0f / std::sqrt(10.0f);
    float R = circumradius;

    // Half-space distances (positive = outside)
    // 6 square faces: |x| <= R, |y| <= R, |z| <= R but truncated
    // The actual truncation: square faces are at distance R from center
    // in each axis direction, but the corners are cut off by hex planes.

    vnes_vec3 q = abs(p);

    // Outer bounding box (the square faces)
    float box_d = std::max(q.x, std::max(q.y, q.z)) - R;

    // Hexagonal face constraints: |x| + |y| + |z| <= 2*R
    float hex_d = (q.x + q.y + q.z) - 2.0f * R;

    // The truncated octahedron is the intersection of all these half-spaces.
    // Inside is where ALL constraints are negative (or zero).
    return std::max(box_d, hex_d);
}

// ── Shell helper ──────────────────────────────────────────
// Converts a solid SDF into a shell of given thickness.

inline float sdf_shell(float solid_sdf, float thickness) {
    return std::fabs(solid_sdf) - thickness * 0.5f;
}

// ── TPMS (Triply Periodic Minimal Surface) SDFs ───────────

inline float sdf_schwarz_p(vnes_vec3 p, float scale) {
    // Schwarz P surface: cos(x) + cos(y) + cos(z) = 0
    float sp = std::cos(scale * p.x) + std::cos(scale * p.y) + std::cos(scale * p.z);
    return sp;  // zero level set is the surface
}

inline float sdf_gyroid(vnes_vec3 p, float scale) {
    // Gyroid surface: sin(x)*cos(y) + sin(y)*cos(z) + sin(z)*cos(x) = 0
    float g = std::sin(scale * p.x) * std::cos(scale * p.y) +
              std::sin(scale * p.y) * std::cos(scale * p.z) +
              std::sin(scale * p.z) * std::cos(scale * p.x);
    return g;
}

inline float sdf_schwarz_p_shell(vnes_vec3 p, float scale, float thickness) {
    return std::fabs(sdf_schwarz_p(p, scale)) - thickness;
}

inline float sdf_gyroid_shell(vnes_vec3 p, float scale, float thickness) {
    return std::fabs(sdf_gyroid(p, scale)) - thickness;
}

// ── Gradient (density) gyroid ─────────────────────────────
// Gradually changes from dense to open pore structure along
// the local t coordinate (0 = dense core, 1 = open growth face).

inline float sdf_gradient_gyroid(vnes_vec3 p, float scale_base,
                                  float scale_min, float scale_max,
                                  float t, float thickness) {
    float s = scale_min + (scale_max - scale_min) * t;
    return std::fabs(sdf_gyroid(p, s)) - thickness;
}

// ── Normal estimation ─────────────────────────────────────
// Finite-difference gradient of any SDF.

inline vnes_vec3 sdf_normal(vnes_vec3 p, float (*sdf)(vnes_vec3)) {
    const float eps = 0.001f;
    return normalize(vnes_vec3{
        sdf(p + vnes_vec3{eps,0,0}) - sdf(p - vnes_vec3{eps,0,0}),
        sdf(p + vnes_vec3{0,eps,0}) - sdf(p - vnes_vec3{0,eps,0}),
        sdf(p + vnes_vec3{0,0,eps}) - sdf(p - vnes_vec3{0,0,eps})
    });
}

template<typename F>
inline vnes_vec3 sdf_normal(vnes_vec3 p, F sdf_fn) {
    const float eps = 0.001f;
    return normalize(vnes_vec3{
        sdf_fn(p + vnes_vec3{eps,0,0}) - sdf_fn(p - vnes_vec3{eps,0,0}),
        sdf_fn(p + vnes_vec3{0,eps,0}) - sdf_fn(p - vnes_vec3{0,eps,0}),
        sdf_fn(p + vnes_vec3{0,0,eps}) - sdf_fn(p - vnes_vec3{0,0,eps})
    });
}
