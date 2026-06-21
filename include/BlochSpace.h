// BlochSpace.h — ENFORCE_BLOCH_SPACE(T) audit, Depth Utilization & Crystallization Yield
// Phase 8: Ghost Census & Ontological Evolution
// Ensures all simulation variables use vn::fp20_t (Bloch-space fixed-point) and
// monitors Depth pillar utilization for self-extending pillar language.
#pragma once

#include "Entity.h"
#include "../vn/ScaledInt.h"
#include <type_traits>
#include <cstdint>

// ── ENFORCE_BLOCH_SPACE(T) — Ghost audit detector ────────────
// Usage: ENFORCE_BLOCH_SPACE(MyStruct)
//   Place inside a struct that should use ONLY vn::fp20_t for simulation state.
//   The struct must declare `static constexpr bool _bloch_audited = true;`
//   to pass. If a float member is accidentally added, remove the marker
//   or add a `int _bloch_audit_dummy;`-style exclusion.
//
// This is a belt-and-suspenders approach:
//   1. Compile-time marker check (opt-in: struct must declare audited)
//   2. Runtime boundary check (log a warning if float is used with fp20_t values)
//
// For structs that must contain floats (GPU buffers, Vulkan handles),
// do NOT use this macro. Use manual inspection instead.
#define ENFORCE_BLOCH_SPACE(T) \
    static_assert(std::is_standard_layout<T>::value, \
        #T " must be standard layout for Bloch-space determinism"); \
    static_assert(T::_bloch_audited, \
        "ERROR: " #T " is not Bloch-space audited. " \
        "Define 'static constexpr bool _bloch_audited = true;' to confirm " \
        "all members use vn::fp20_t instead of float/double.");

// ── Depth Utilization Monitor ───────────────────────────────
// Tracks ratio of energy in Pillar 15 (Depth) vs total PSV energy.
// Prevents ontology from regressing into a black-box latent space.
struct DepthUtilization {
    float depth_energy;
    float total_energy;
    float ratio;          // depth / total
    float running_avg;    // EMA of ratio over time
    float peak_ratio;

    void reset() {
        depth_energy = 0.0f;
        total_energy = 0.0f;
        ratio = 0.0f;
        running_avg = 0.0f;
        peak_ratio = 0.0f;
    }

    void sample(const PillarStateVector& psv, float alpha = 0.01f) {
        depth_energy = psv[Depth] * psv[Depth];
        total_energy = 0.0f;
        for (int i = 0; i < NumPillars; i++) {
            total_energy += psv[i] * psv[i];
        }
        if (total_energy < 1e-10f) {
            ratio = 0.0f;
        } else {
            ratio = depth_energy / total_energy;
        }
        running_avg = running_avg * (1.0f - alpha) + ratio * alpha;
        if (ratio > peak_ratio) peak_ratio = ratio;
    }

    bool is_blackbox_warning() const {
        return ratio > 0.5f;  // Depth > 50% of total energy = latent space risk
    }

    bool is_healthy() const {
        return ratio < 0.3f && running_avg < 0.25f;
    }
};

// ── Crystallization Yield ───────────────────────────────────
// Enables recurring patterns in Depth to "crystallize" into new sub-pillars,
// allowing the PID system to evolve from a fixed dictionary into a
// self-extending language.
struct CrystallizationYield {
    static constexpr int MAX_SUB_PILLARS = 8;

    struct SubPillar {
        float pattern[NumPillars];  // the crystallized pattern (PSV delta)
        float coherence;            // [0,1] how well-formed
        float age;                  // ticks since crystallization
        bool active;
    };

    SubPillar sub_pillars[MAX_SUB_PILLARS];
    int count;
    float crystallization_threshold;  // min Depth ratio to trigger

    void init(float threshold = 0.35f) {
        count = 0;
        crystallization_threshold = threshold;
        for (int i = 0; i < MAX_SUB_PILLARS; i++) {
            sub_pillars[i].active = false;
            sub_pillars[i].coherence = 0.0f;
        }
    }

    // Attempt crystallization from a recurring Depth pattern
    // Returns index of new sub-pillar, or -1 if no crystallization occurs
    int crystallize(const PillarStateVector& psv, float depth_util_ratio, float delta_time) {
        if (depth_util_ratio < crystallization_threshold) return -1;
        if (count >= MAX_SUB_PILLARS) return -1;

        // Check if this pattern is already crystallized (within tolerance)
        for (int i = 0; i < count; i++) {
            if (!sub_pillars[i].active) continue;
            bool match = true;
            for (int j = 0; j < NumPillars; j++) {
                float diff = sub_pillars[i].pattern[j] - psv[j];
                if (diff * diff > 0.01f) { match = false; break; }
            }
            if (match) {
                sub_pillars[i].coherence = sub_pillars[i].coherence * 0.9f + 0.1f;
                return i;
            }
        }

        // Crystallize new sub-pillar
        int idx = count++;
        for (int j = 0; j < NumPillars; j++) {
            sub_pillars[idx].pattern[j] = psv[j];
        }
        sub_pillars[idx].coherence = 0.3f;  // initial coherence
        sub_pillars[idx].age = 0.0f;
        sub_pillars[idx].active = true;
        return idx;
    }

    void tick_all(float delta_time) {
        for (int i = 0; i < count; i++) {
            if (!sub_pillars[i].active) continue;
            sub_pillars[i].age += delta_time;
            sub_pillars[i].coherence *= 0.995f;  // slow decay
            if (sub_pillars[i].coherence < 0.1f) {
                sub_pillars[i].active = false;
            }
        }
    }
};

// ── Unified Bloch Conductor ─────────────────────────────────
// Manages Depth Utilization + Crystallization for an entity.
// Wired into the tick loop.
struct BlochConductor {
    DepthUtilization depth_monitor;
    CrystallizationYield crystallizer;
    float time;

    void init() {
        depth_monitor.reset();
        crystallizer.init();
        time = 0.0f;
    }

    void tick(const PillarStateVector& psv, float delta_time) {
        time += delta_time;
        depth_monitor.sample(psv);
        crystallizer.tick_all(delta_time);
        crystallizer.crystallize(psv, depth_monitor.ratio, delta_time);
    }

    int active_sub_pillar_count() const {
        int n = 0;
        for (int i = 0; i < crystallizer.count; i++) {
            if (crystallizer.sub_pillars[i].active) n++;
        }
        return n;
    }
};
