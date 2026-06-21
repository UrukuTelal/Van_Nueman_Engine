#pragma once

#include "quantum_interface.h"
#include "../include/Entity.h"
#include "../include/Transform.h"
#include "../spatial/SpatialGrid.h"
#include <cstring>
#include <cmath>

namespace vn {
namespace quantum {

class ClassicalFallbackBackend : public QuantumBackend {
public:
    QuantumBackendType type() const override { return QuantumBackendType::CLASSICAL_FALLBACK; }
    const char* name() const override { return "ClassicalFallback"; }

    bool is_available() const override { return true; }
    bool supports_subroutine(QuantumSubroutine s) const override { return true; }

    QuantumResult execute_wht(const float input[32], float output[32]) override {
        QuantumResult r;
        r.success = true;
        r.fidelity_estimate = 1.0;
        std::memcpy(output, input, 32 * sizeof(float));
        fwht_32(output);
        r.amplitudes.assign(output, output + 32);
        return r;
    }

    QuantumResult execute_hopf_projection(const float thought[512], float membrane[32]) override {
        QuantumResult r;
        r.success = true;
        r.fidelity_estimate = 1.0;

        // Normalize thought to unit S^511
        float norm_thought[512];
        float sum_sq = 0.0f;
        for (int i = 0; i < 512; i++) sum_sq += thought[i] * thought[i];
        float norm = std::sqrt(sum_sq);
        float inv_norm = (norm > 1e-10f) ? (1.0f / norm) : 0.0f;
        for (int i = 0; i < 512; i++) norm_thought[i] = thought[i] * inv_norm;

        float frame_wht[32][32];
        for (int f = 0; f < 32; f++) {
            float frame[16];
            std::memcpy(frame, norm_thought + f * 16, 16 * sizeof(float));
            encode_frame_(frame, frame_wht[f]);
        }
        for (int i = 0; i < 32; i++) {
            float sum = 0.0f;
            for (int f = 0; f < 32; f++) {
                float phase = std::cos(2.0f * 3.14159265f * static_cast<float>(f * i) / 32.0f);
                sum += frame_wht[f][i] * phase;
            }
            membrane[i] = sum * (1.0f / std::sqrt(32.0f));
        }
        r.amplitudes.assign(membrane, membrane + 32);
        return r;
    }

    QuantumResult execute_spatial_search(const SpatialGrid<128>& grid,
                                          vn::fp20_t x, vn::fp20_t y, vn::fp20_t z,
                                          uint32_t range,
                                          std::vector<uint32_t>& out) override {
        QuantumResult r;
        r.success = true;
        r.fidelity_estimate = 1.0;
        grid.query(x, y, z, range, out);
        r.measurements = out;
        return r;
    }

    QuantumResult execute_pillar_coupling(const PillarStateVector& actor,
                                           const PillarStateVector& subject,
                                           int operator_pillar,
                                           int target_pillar,
                                           PillarStateVector& result) override {
        QuantumResult r;
        r.success = true;
        r.fidelity_estimate = 1.0;
        result = subject;
        vn::fp20_t depth(0.5f);
        vn::fp20_t constraint(0.0f);
        vn::fp20_t alignment = bloch_dot_product(actor, result);
        transform_bloch(result, actor[operator_pillar],
                        actor[Influence],
                        target_pillar, depth, constraint, alignment);
        r.amplitudes.reserve(NumPillars);
        for (int i = 0; i < NumPillars; i++)
            r.amplitudes.push_back(result[i].to_float());
        return r;
    }

    QuantumResult execute_bloch_evolution(const float pillar_values[16],
                                           const float delta_thetas[16],
                                           float result[16]) override {
        QuantumResult r;
        r.success = true;
        r.fidelity_estimate = 1.0;
        for (int i = 0; i < 16; i++) {
            vn::fp20_t val(pillar_values[i]);
            vn::fp20_t dt(delta_thetas[i]);
            vn::fp20_t rotated = apply_bloch_rotation(val, dt);
            result[i] = rotated.to_float();
        }
        r.amplitudes.assign(result, result + 16);
        return r;
    }

private:
    static void fwht_32(float* x) {
        float temp;
        for (int len = 1; len < 32; len <<= 1) {
            for (int i = 0; i < 32; i += (len << 1)) {
                for (int j = 0; j < len; j++) {
                    temp = x[i + j];
                    x[i + j] = temp + x[i + j + len];
                    x[i + j + len] = temp - x[i + j + len];
                }
            }
        }
        float norm = 1.0f / std::sqrt(32.0f);
        for (int i = 0; i < 32; i++) x[i] *= norm;
    }

    static void encode_frame_(const float pillars[16], float wht[32]) {
        for (int i = 0; i < 16; i++) wht[i] = pillars[i];
        for (int i = 16; i < 32; i++) wht[i] = 0.0f;
        fwht_32(wht);
    }
};

} // namespace quantum
} // namespace vn
