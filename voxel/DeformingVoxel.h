#pragma once

#include "../vn/ScaledInt.h"
#include "../scale/ScaleExponent.h"
#include "TruncatedOctahedron.h"
#include <cstdint>

struct DeformingVertex {
    vn::fp20_t x, y, z;
    vn::fp20_t rest_x, rest_y, rest_z;
    vn::fp20_t velocity_x, velocity_y, velocity_z;
    vn::fp20_t force_x, force_y, force_z;
    vn::fp20_t mass;
    vn::fp20_t spring_constant;
    vn::fp20_t damping;
    uint32_t material_id;
};

struct MaterialSpringCoefficients {
    vn::fp20_t spring_k;
    vn::fp20_t damping_c;
    vn::fp20_t yield_stress;
    vn::fp20_t fracture_threshold;
};

inline vn::fp20_t compute_displacement_sq(const DeformingVertex& v) {
    vn::fp20_t dx = v.x - v.rest_x;
    vn::fp20_t dy = v.y - v.rest_y;
    vn::fp20_t dz = v.z - v.rest_z;
    return dx * dx + dy * dy + dz * dz;
}

inline vn::fp20_t compute_strain(const DeformingVertex& v) {
    return vn::fp_sqrt(compute_displacement_sq(v));
}

inline void compute_spring_force(const DeformingVertex& v,
                                  vn::fp20_t& fx, vn::fp20_t& fy, vn::fp20_t& fz)
{
    vn::fp20_t dx = v.x - v.rest_x;
    vn::fp20_t dy = v.y - v.rest_y;
    vn::fp20_t dz = v.z - v.rest_z;
    fx = vn::fp20_t(0) - (v.spring_constant * dx);
    fy = vn::fp20_t(0) - (v.spring_constant * dy);
    fz = vn::fp20_t(0) - (v.spring_constant * dz);
}

inline void compute_neighbor_force(const DeformingVertex& v,
                                    const DeformingVertex& neighbor,
                                    vn::fp20_t neighbor_spring_k,
                                    vn::fp20_t& fx, vn::fp20_t& fy, vn::fp20_t& fz)
{
    vn::fp20_t dx = neighbor.x - v.x;
    vn::fp20_t dy = neighbor.y - v.y;
    vn::fp20_t dz = neighbor.z - v.z;
    fx = neighbor_spring_k * dx;
    fy = neighbor_spring_k * dy;
    fz = neighbor_spring_k * dz;
}

inline void update_vertex_verlet(DeformingVertex& v, vn::fp20_t dt) {
    vn::fp20_t damp_x = v.damping * v.velocity_x;
    vn::fp20_t damp_y = v.damping * v.velocity_y;
    vn::fp20_t damp_z = v.damping * v.velocity_z;

    vn::fp20_t inv_mass = vn::fp20_t(1.0f) / v.mass;
    vn::fp20_t ax = (v.force_x - damp_x) * inv_mass;
    vn::fp20_t ay = (v.force_y - damp_y) * inv_mass;
    vn::fp20_t az = (v.force_z - damp_z) * inv_mass;

    v.velocity_x = v.velocity_x + ax * dt;
    v.velocity_y = v.velocity_y + ay * dt;
    v.velocity_z = v.velocity_z + az * dt;

    v.x = v.x + v.velocity_x * dt;
    v.y = v.y + v.velocity_y * dt;
    v.z = v.z + v.velocity_z * dt;

    v.force_x = vn::fp20_t(0);
    v.force_y = vn::fp20_t(0);
    v.force_z = vn::fp20_t(0);
}
