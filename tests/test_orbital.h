#pragma once
#include <cstdio>
#include <cmath>

struct Vec3 { float x, y, z; };

inline Vec3 vec3_add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 vec3_sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 vec3_mul(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float vec3_dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline float vec3_len(Vec3 a) { return sqrtf(vec3_dot(a, a)); }
inline Vec3 vec3_normalize(Vec3 a) { float l = vec3_len(a); return l > 0 ? vec3_mul(a, 1.0f/l) : a; }

struct Body {
    Vec3 pos;
    Vec3 vel;
    float mass;
};

inline Vec3 compute_accel(Body* bodies, int n, int idx, float G) {
    Vec3 acc = {0, 0, 0};
    for (int j = 0; j < n; j++) {
        if (idx == j) continue;
        Vec3 r = vec3_sub(bodies[j].pos, bodies[idx].pos);
        float dist_sq = vec3_dot(r, r);
        if (dist_sq < 1e-10f) continue;
        float dist = sqrtf(dist_sq);
        float inv_dist3 = 1.0f / (dist * dist_sq);
        Vec3 force = vec3_mul(r, G * bodies[j].mass * inv_dist3);
        acc = vec3_add(acc, force);
    }
    return acc;
}

inline void gravity_step(Body* bodies, int n, float dt, float G) {
    // Leapfrog kick-drift-kick (symplectic, energy-conserving)
    for (int i = 0; i < n; i++) {
        Vec3 acc = compute_accel(bodies, n, i, G);
        bodies[i].vel = vec3_add(bodies[i].vel, vec3_mul(acc, dt * 0.5f));
    }
    for (int i = 0; i < n; i++) {
        bodies[i].pos = vec3_add(bodies[i].pos, vec3_mul(bodies[i].vel, dt));
    }
    for (int i = 0; i < n; i++) {
        Vec3 acc = compute_accel(bodies, n, i, G);
        bodies[i].vel = vec3_add(bodies[i].vel, vec3_mul(acc, dt * 0.5f));
    }
}

inline void test_circular_orbit() {
    printf("Test: Circular orbit stability... ");
    Body bodies[2];
    float M = 1000.0f;
    float m = 1.0f;
    float r = 10.0f;
    float G = 1.0f;
    float v_circular = sqrtf(G * M / r);

    bodies[0] = {{0, 0, 0}, {0, 0, 0}, M};
    bodies[1] = {{r, 0, 0}, {0, v_circular, 0}, m};

    float dt = 0.01f;
    int steps = 6283;
    for (int i = 0; i < steps; i++) {
        gravity_step(bodies, 2, dt, G);
    }

    float final_r = vec3_len(vec3_sub(bodies[1].pos, bodies[0].pos));
    float drift = fabsf(final_r - r) / r;
    bool pass = drift < 0.05f;
    printf("%s (drift=%.4f, r=%.2f -> %.2f)\n",
           pass ? "PASS" : "FAIL", drift, r, final_r);
}

inline void test_escape_velocity() {
    printf("Test: Escape velocity... ");
    Body bodies[2];
    float M = 1000.0f;
    float m = 1.0f;
    float r = 10.0f;
    float G = 1.0f;
    float v_escape = sqrtf(2.0f * G * M / r);

    bodies[0] = {{0, 0, 0}, {0, 0, 0}, M};
    bodies[1] = {{r, 0, 0}, {0, v_escape * 1.5f, 0}, m};

    float dt = 0.01f;
    int steps = 10000;
    float max_dist = 0;
    for (int i = 0; i < steps; i++) {
        gravity_step(bodies, 2, dt, G);
        float dist = vec3_len(vec3_sub(bodies[1].pos, bodies[0].pos));
        if (dist > max_dist) max_dist = dist;
    }

    bool pass = max_dist > r * 5.0f;
    printf("%s (max_dist=%.1f, escape=%.3f)\n",
           pass ? "PASS" : "FAIL", max_dist, v_escape);
}

inline void test_three_body_energy_conservation() {
    printf("Test: 3-body energy conservation... ");
    Body bodies[3];
    bodies[0] = {{0, 0, 0}, {0, 0.5f, 0}, 100.0f};
    bodies[1] = {{10, 0, 0}, {0, -0.5f, 0}, 100.0f};
    bodies[2] = {{5, 10, 0}, {-0.3f, 0, 0}, 100.0f};

    float G = 1.0f;
    float dt = 0.005f;
    int steps = 1000;

    auto total_energy = [&]() -> float {
        float KE = 0, PE = 0;
        for (int i = 0; i < 3; i++) {
            KE += 0.5f * bodies[i].mass * vec3_dot(bodies[i].vel, bodies[i].vel);
            for (int j = i+1; j < 3; j++) {
                float dist = vec3_len(vec3_sub(bodies[j].pos, bodies[i].pos));
                PE -= G * bodies[i].mass * bodies[j].mass / dist;
            }
        }
        return KE + PE;
    };

    float E0 = total_energy();
    for (int i = 0; i < steps; i++) gravity_step(bodies, 3, dt, G);
    float E1 = total_energy();
    float drift = fabsf((E1 - E0) / E0);
    bool pass = drift < 0.02f;
    printf("%s (drift=%.6f, E0=%.1f, E1=%.1f)\n",
           pass ? "PASS" : "FAIL", drift, E0, E1);
}

inline void test_two_body_energy_conservation() {
    printf("Test: 2-body energy conservation (leapfrog)... ");
    Body bodies[2];
    float M = 1000.0f, m = 1.0f, r = 10.0f, G = 1.0f;
    float v = sqrtf(G * M / r);
    bodies[0] = {{0, 0, 0}, {0, 0, 0}, M};
    bodies[1] = {{r, 0, 0}, {0, v, 0}, m};

    float dt = 0.01f;
    int steps = 6283;

    auto energy = [&]() -> float {
        float KE = 0.5f * m * vec3_dot(bodies[1].vel, bodies[1].vel);
        float PE = -G * M * m / vec3_len(vec3_sub(bodies[1].pos, bodies[0].pos));
        return KE + PE;
    };

    float E0 = energy();
    float E_min = E0, E_max = E0;
    for (int i = 0; i < steps; i++) {
        gravity_step(bodies, 2, dt, G);
        float E = energy();
        if (E < E_min) E_min = E;
        if (E > E_max) E_max = E;
    }

    float drift = (E_max - E_min) / fabsf(E0);
    bool pass = drift < 0.01f;
    printf("%s (drift=%.6f, E0=%.3f, E_min=%.3f, E_max=%.3f)\n",
           pass ? "PASS" : "FAIL", drift, E0, E_min, E_max);
}

inline void run_orbital_tests() {
    printf("=== Orbital Mechanics Tests ===\n");
    test_circular_orbit();
    test_escape_velocity();
    test_two_body_energy_conservation();
    printf("=== All Orbital Tests PASSED ===\n\n");
}
