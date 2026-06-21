#pragma once

#include "../include/Entity.h"
#include "../voxel/VoxelCell.h"
#include <cmath>
#include <cstdint>
#include <vector>

struct pKaValue {
    float pKa;
    uint32_t acid_pillar;   // pillar index for the acid (protonated) form
    uint32_t base_pillar;   // pillar index for the conjugate base form
    float total_conc;       // total concentration of acid+base species

    pKaValue() : pKa(7.0f), acid_pillar(Attraction),
                 base_pillar(Flux), total_conc(0.1f) {}
};

inline float compute_ph(const vn::fp20_t pillars[NumPillars]) {
    float flux = pillars[Flux].to_float();
    float integrity = pillars[Integrity].to_float();
    float harm = pillars[Harm].to_float();

    float proton_ratio = (flux + 1e-10f) /
                         (flux + integrity + 1e-10f);
    float ph = 7.0f + 3.0f * (0.5f - proton_ratio) - harm * 2.0f;
    if (ph < 0.0f) ph = 0.0f;
    if (ph > 14.0f) ph = 14.0f;
    return ph;
}

inline float protonation_state(float ph, float pKa) {
    float diff = ph - pKa;
    return 1.0f / (1.0f + powf(10.0f, diff));
}

inline float deprotonation_state(float ph, float pKa) {
    return 1.0f - protonation_state(ph, pKa);
}

inline float redox_potential(const vn::fp20_t pillars[NumPillars]) {
    float flux = pillars[Flux].to_float();
    float cohesion = pillars[Cohesion].to_float();
    float warmth = pillars[Warmth].to_float();

    float e0 = 0.0f;
    float ratio = (flux + 1e-10f) /
                  (flux + cohesion + 1e-10f);
    e0 = (ratio - 0.5f) * 2.0f;
    e0 += warmth * 0.3f;
    if (e0 > 1.0f) e0 = 1.0f;
    if (e0 < -1.0f) e0 = -1.0f;
    return e0; // -1 (reducing) to +1 (oxidizing)
}

inline float buffer_capacity(const vn::fp20_t pillars[NumPillars],
                              float acid_conc, float base_conc) {
    float integrity = pillars[Integrity].to_float();
    float cohesion = pillars[Cohesion].to_float();
    float stability = (integrity + cohesion) * 0.5f;

    float total = acid_conc + base_conc;
    if (total < 1e-10f) return 0.0f;
    float beta = 2.303f * (acid_conc * base_conc) / total;
    return beta * (0.5f + stability * 0.5f);
}

inline float ph_after_addition(float current_ph, float pKa,
                                float acid_added, float base_added,
                                float buff_acid_conc, float buff_base_conc) {
    float new_acid = buff_acid_conc + acid_added;
    float new_base = buff_base_conc + base_added;
    if (new_acid < 0.0f) new_acid = 0.0f;
    if (new_base < 0.0f) new_base = 0.0f;
    if (new_acid < 1e-10f || new_base < 1e-10f)
        return current_ph + (base_added - acid_added) * 0.5f;
    return pKa + log10f(new_base / new_acid);
}

inline bool is_reducing_environment(const vn::fp20_t pillars[NumPillars]) {
    return redox_potential(pillars) < -0.3f;
}

inline bool is_oxidizing_environment(const vn::fp20_t pillars[NumPillars]) {
    return redox_potential(pillars) > 0.3f;
}

inline float reduction_effect(const vn::fp20_t pillars[NumPillars],
                               const pKaValue& pka) {
    float red = redox_potential(pillars);
    float ph = compute_ph(pillars);
    float protonated = protonation_state(ph, pka.pKa);
    if (red < 0) {
        return protonated * (1.0f + fabsf(red));
    }
    return (1.0f - protonated) * (1.0f + red);
}

struct GradientField {
    float ph;
    float redox;
    float buffer_beta;
    float temperature;
};

inline GradientField sample_environment(const vn::fp20_t pillars[NumPillars],
                                         float acid_conc = 0.05f,
                                         float base_conc = 0.05f) {
    GradientField gf;
    gf.ph = compute_ph(pillars);
    gf.redox = redox_potential(pillars);
    gf.buffer_beta = buffer_capacity(pillars, acid_conc, base_conc);
    gf.temperature = pillars[Warmth].to_float() * 100.0f;
    return gf;
}
