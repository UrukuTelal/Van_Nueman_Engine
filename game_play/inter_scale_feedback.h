// inter_scale_feedback.h - Inter-scale WHT feedback for fractal governance
// Logs feedback between scales (atom ↔ universe) using Walsh-Hadamard Transform
// After logarithmic scaling: scale_diff = log2(universe / atom)

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include "../audio/wht.h"  // Reuse existing WHT (WHT_N=32, WHT_LOG2_N=5)
#include "../vn/PillarTypes.h"

#ifndef MAX_SCALE_LEVELS
#define MAX_SCALE_LEVELS 9  // 0=atom, 1=molecule, ..., 8=universe
#endif

// Scale levels for fractal governance
enum ScaleLevel : uint32_t {
    SCALE_ATOM = 0,       // Quantum fluctuation
    SCALE_MOLECULE = 1,  // Atomic bond
    SCALE_CELL = 2,         // Organelle
    SCALE_ORGANISM = 3,    // Creature (Bob)
    SCALE_SOCIETY = 4,    // Multi-agent
    SCALE_PLANET = 5,      // Biosphere
    SCALE_STAR_SYSTEM = 6,// Van Nueman probe
    SCALE_GALAXY = 7,     // Cluster
    SCALE_UNIVERSE = 8    // All servers combined
};

// WHT Signal data (matches Pillar_10_Memory/signals/wht_signal_*.json format)
struct ScaleFeedback {
    uint32_t from_scale;          // 0=atom, 8=universe
    uint32_t to_scale;
    float wht_coefficients[128];  // 4 x WHT_N (32) for padding
    uint32_t timestamp;
    float signal_energy;         // sum of squares
    uint32_t non_zero_count;     // sparsity measure
    float sparsity;               // 1 - (non_zero / 128)
};

class InterScaleFeedback {
public:
    InterScaleFeedback();
    ~InterScaleFeedback();
    
    // Log feedback: from_scale → to_scale after logarithmic scaling
    // WHT coefficient at index 2^k = coupling strength at k orders of magnitude
    bool log_feedback(uint32_t from_scale, uint32_t to_scale,
                          const float* pillar_data, uint32_t length);
    
    // Read feedback for a scale difference
    bool read_feedback(int scale_diff, float* out_coefficients) const;
    
    // Get coefficient index for scale difference: index = log2(to / from)
    static int get_coefficient_index(uint32_t from, uint32_t to) {
        if (from > to) { uint32_t tmp = from; from = to; to = tmp; }
        float diff = (float)(to - from);  // Simplified: use log2 in production
        int idx = (int)log2f(diff > 0 ? diff : 1.0f);
        return (idx < 128) ? idx : 127;
    }
    
    // Get all logged feedback
    const std::vector<ScaleFeedback>& get_all_feedback() const { return feedback_log_; }
    
    // Save to Pillar_10_Memory/signals/ (matches existing format)
    bool save_to_pillar_memory(const ScaleFeedback& fb) const;
    
private:
    std::vector<ScaleFeedback> feedback_log_;
    
    // Transform pillar data to WHT domain (reuse audio/wht.cpp)
    void transform_to_wht(float* data, int log2n);
    
    // Calculate signal energy
    float calculate_energy(const float* data, uint32_t length);
};

// Fractal Governance: UID = PSV hash, scale from atom to universe
// "Pillars + Skelly(structure) = UID"
inline uint32_t compute_fractal_uid(const vn::fp20_t psv[16], uint32_t scale_level) {
    // Combine PSV with scale level for unique cross-scale ID
    float combined[16];
    for (int i = 0; i < 16; i++) {
        combined[i] = psv[i].to_float() + (float)scale_level * 0.01f;  // Scale affects UID slightly
    }
    // Use Entity.h's compute_entity_id() logic
    uint32_t hash = 2166136261u;
    for (int i = 0; i < 16; i++) {
        uint32_t bits;
        memcpy(&bits, &combined[i], sizeof(float));
        hash ^= bits;
        hash *= 16777619u;
    }
    return hash ? hash : 1u;
}
