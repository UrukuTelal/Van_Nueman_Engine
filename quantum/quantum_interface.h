#pragma once

#include "../include/Entity.h"
#include "../spatial/SpatialGrid.h"
#include <cstdint>
#include <vector>

namespace vn {
namespace quantum {

enum class QuantumBackendType : uint8_t {
    CLASSICAL_FALLBACK = 0,
    CUDA_QUANTUM = 1,
    NATIVE_QUANTUM = 2
};

enum class QuantumSubroutine : uint8_t {
    WHT_TRANSFORM = 0,
    HOPF_PROJECTION = 1,
    SPATIAL_SEARCH = 2,
    PILLAR_COUPLING = 3,
    BLOCH_EVOLUTION = 4
};

struct QuantumResult {
    bool success;
    std::vector<float> amplitudes;
    std::vector<uint32_t> measurements;
    double fidelity_estimate;
    uint64_t execution_time_ns;
};

class QuantumBackend {
public:
    virtual ~QuantumBackend() = default;

    virtual QuantumBackendType type() const = 0;
    virtual const char* name() const = 0;

    virtual bool is_available() const = 0;
    virtual bool supports_subroutine(QuantumSubroutine s) const = 0;

    // WHT: encode 32-element pillar vector, apply quantum WHT, decode
    virtual QuantumResult execute_wht(
        const float input[32],
        float output[32]
    ) = 0;

    // Hopf projection: 512D thought -> 32D membrane via QFT
    virtual QuantumResult execute_hopf_projection(
        const float thought[512],
        float membrane[32]
    ) = 0;

    // Spatial search: Grover-style query of SpatialGrid
    virtual QuantumResult execute_spatial_search(
        const SpatialGrid<128>& grid,
        vn::fp20_t x, vn::fp20_t y, vn::fp20_t z,
        uint32_t range,
        std::vector<uint32_t>& out
    ) = 0;

    // Pillar coupling: quantum harmonic oscillator evolution
    virtual QuantumResult execute_pillar_coupling(
        const PillarStateVector& actor,
        const PillarStateVector& subject,
        int operator_pillar,
        int target_pillar,
        PillarStateVector& result
    ) = 0;

    // Bloch evolution: batch Bloch rotations via quantum gates
    virtual QuantumResult execute_bloch_evolution(
        const float pillar_values[16],
        const float delta_thetas[16],
        float result[16]
    ) = 0;

    // Calibration / noise mitigation
    virtual double estimate_noise_level() const { return 0.0; }
    virtual size_t available_qubits() const { return 0; }
};

} // namespace quantum
} // namespace vn
