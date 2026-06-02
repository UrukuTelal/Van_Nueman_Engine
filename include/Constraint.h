// Constraint.h - Depth Overflow & Constraint System
// Part of the COSMOS 16D Pillar Cosmology implementation
// Phase 3: Depth Overflow, Constraint & Vortex Generation
//
// Matches: DeveloperConsole/backend/models/constraint.py
// Constraint is NOT death — it reduces SVT manifestation. Recovery is
// achieved via Integrity/Cohesion Bloch rotations, not scalar healing.

#pragma once

#include "Entity.h"
#include <cmath>
#include <cstdio>
#include <cstring>

// ── Constraint Constants ──────────────────────────────────────

#define DEFAULT_CONSTRAINT 0.0f
#define MAX_CONSTRAINT 1.0f
#define MIN_CONSTRAINT 0.0f

// ── Constraint Event ──────────────────────────────────────────

struct ConstraintEvent {
    char event_type[24];   // "accumulation", "recovery", "overflow", "full_constraint"
    float constraint_before;
    float constraint_after;
    float depth_before;
    float depth_after;
    float delta;
    int source_pillar;
    
    void print() const {
        printf("CONSTRAINT EVENT [%s]: constraint %.3f->%.3f depth %.3f->%.3f delta=%.4f\n",
               event_type, constraint_before, constraint_after,
               depth_before, depth_after, delta);
    }
};

// ── Accumulation Result ───────────────────────────────────────

struct AccumulationResult {
    float delta_theta_applied;
    float delta_theta_absorbed;
    float depth_before;
    float depth_after;
    float constraint_before;
    float constraint_after;
    int event_count;
    ConstraintEvent events[8];  // max 8 events per accumulation
    
    void print() const {
        printf("ACCUMULATION: applied=%.4f absorbed=%.4f depth=%.3f->%.3f constraint=%.3f->%.3f (%d events)\n",
               delta_theta_applied, delta_theta_absorbed,
               depth_before, depth_after,
               constraint_before, constraint_after, event_count);
        for (int i = 0; i < event_count; i++) {
            events[i].print();
        }
    }
};

// ── Constraint State ──────────────────────────────────────────

struct ConstraintState {
    float level;
    int event_count;
    ConstraintEvent events[32];  // circular-ish event log
    
    void init(float initial = DEFAULT_CONSTRAINT) {
        level = (initial < MIN_CONSTRAINT) ? MIN_CONSTRAINT :
                (initial > MAX_CONSTRAINT) ? MAX_CONSTRAINT : initial;
        event_count = 0;
    }
    
    bool is_fully_constrained() const {
        return level >= MAX_CONSTRAINT;
    }
    
    // How much the entity can manifest in SVT.
    // Constraint 0.0 → 1.0 (full), 1.0 → 0.1 (never zero).
    float get_manifestation_multiplier() const {
        float raw = 1.0f - level * 0.9f;
        return (raw < 0.1f) ? 0.1f : raw;
    }
    
    // Scales TRANSFORM output influence.
    float get_transform_output_scale() const {
        float raw = 1.0f - level * 0.95f;
        return (raw < 0.05f) ? 0.05f : raw;
    }
    
    // ── Record an event ────────────────────────────────────────
    void record_event(const char* type, float cb, float ca, float db, float da, float d, int sp = -1) {
        if (event_count < 32) {
            std::strncpy(events[event_count].event_type, type, sizeof(events[event_count].event_type) - 1);
            events[event_count].event_type[sizeof(events[event_count].event_type) - 1] = '\0';
            events[event_count].constraint_before = cb;
            events[event_count].constraint_after = ca;
            events[event_count].depth_before = db;
            events[event_count].depth_after = da;
            events[event_count].delta = d;
            events[event_count].source_pillar = sp;
            event_count++;
        }
    }
    
    // ── Absorb overflow ────────────────────────────────────────
    // Process a raw angular shift through the Depth buffer.
    // If shift <= willpower: fully applied, no Depth drain.
    // If shift > willpower: excess drains Depth. Depleted Depth → constraint rises.
    AccumulationResult absorb_overflow(float raw_angular_shift, float willpower, float depth) {
        AccumulationResult result;
        std::memset(&result, 0, sizeof(result));
        
        float depth_before = depth;
        float depth_after = depth;
        float constraint_before = level;
        
        float shift_abs = (raw_angular_shift < 0.0f) ? -raw_angular_shift : raw_angular_shift;
        const float PI = 3.14159265f;
        float willpower_theta = pillar_value_to_theta(vn::fp20_t(willpower)).to_float();
        float angular_capacity = PI - willpower_theta;
        float delta_applied = 0.0f;
        float delta_absorbed = 0.0f;
        
        if (shift_abs <= angular_capacity) {
            delta_applied = raw_angular_shift;
        } else {
            delta_applied = (raw_angular_shift < 0.0f) ? -angular_capacity : angular_capacity;
            float excess = shift_abs - angular_capacity;
            float depth_cost = excess * 0.5f;
            
            if (depth - depth_cost <= 0.0f) {
                float remaining = depth_cost - depth;
                depth_after = 0.0f;
                level += remaining;
                if (level > MAX_CONSTRAINT) level = MAX_CONSTRAINT;
                delta_absorbed = excess;
                
                result.events[result.event_count++] = {
                    "overflow", constraint_before, level,
                    depth_before, 0.0f, remaining, -1
                };
                
                if (level >= MAX_CONSTRAINT) {
                    result.events[result.event_count++] = {
                        "full_constraint", constraint_before, MAX_CONSTRAINT,
                        depth_before, 0.0f, MAX_CONSTRAINT - constraint_before, -1
                    };
                }
            } else {
                depth_after = depth - depth_cost;
                delta_absorbed = excess;
                
                result.events[result.event_count++] = {
                    "accumulation", level, level,
                    depth_before, depth_after, depth_cost, -1
                };
            }
        }
        
        result.delta_theta_applied = delta_applied;
        result.delta_theta_absorbed = delta_absorbed;
        result.depth_before = depth_before;
        result.depth_after = depth_after;
        result.constraint_before = constraint_before;
        result.constraint_after = level;
        
        // Forward events to global log
        for (int i = 0; i < result.event_count; i++) {
            record_event(result.events[i].event_type,
                         result.events[i].constraint_before,
                         result.events[i].constraint_after,
                         result.events[i].depth_before,
                         result.events[i].depth_after,
                         result.events[i].delta,
                         result.events[i].source_pillar);
        }
        
        return result;
    }
    
    // ── Apply constraint pull to pillar array ──────────────────
    // Gradually rotates all pillars toward the equator, reflecting
    // diminished SVT manifestation.
    void apply_to_pillars(PillarStateVector& psv) const {
        if (level < 0.01f) return;
        const float PI = 3.14159265f;
        float pull_strength = level * 0.005f;
        float equator = PI * 0.5f;
        
        for (int i = 0; i < NUM_PILLARS; i++) {
            float theta = pillar_value_to_theta(psv[i]).to_float();
            float diff = equator - theta;
            if (fabsf(diff) > 0.001f) {
                psv[i] = apply_bloch_rotation(psv[i], vn::fp20_t(diff * pull_strength));
            }
        }
    }
    
    // ── Self-recovery via Integrity/Cohesion ───────────────────
    // Rotates pillars toward the north pole (θ=0, value=1.0) to counteract
    // the equator-pull that accumulated constraint.  The level counter is
    // reduced proportionally as a bookkeeping side-effect.
    void recover(PillarStateVector& psv, float delta_time = 1.0f, float recovery_rate = 0.01f) {
        if (level <= 0.0f) return;
        
        float integrity = psv[PILLAR_INTEGRITY].to_float();
        float cohesion = psv[PILLAR_COHESION].to_float();
        float recovery_strength = integrity * cohesion * recovery_rate * delta_time;
        
        float before = level;
        // Rotate all pillars toward north pole (θ=0) to counteract the equator pull
        for (int i = 0; i < NUM_PILLARS; i++) {
            float theta = pillar_value_to_theta(psv[i]).to_float();
            float north_pole = 0.0f;
            float diff = north_pole - theta;
            psv[i] = apply_bloch_rotation(psv[i], vn::fp20_t(diff * recovery_strength));
        }
        
        level -= recovery_strength;
        if (level < MIN_CONSTRAINT) level = MIN_CONSTRAINT;
        
        float depth = psv[PILLAR_DEPTH].to_float();
        float depth_recovery = recovery_strength * 0.3f;
        depth += depth_recovery;
        if (depth > 1.0f) depth = 1.0f;
        psv[PILLAR_DEPTH] = vn::fp20_t(depth);
        
        record_event("recovery", before, level, depth - depth_recovery, depth, recovery_strength, PILLAR_INTEGRITY);
    }
    
    // ── External restoration ───────────────────────────────────
    // Another entity rotates all pillars toward the north pole via
    // their Integrity & Cohesion (Bloch rotation, NOT scalar decrement).
    void apply_restoration(PillarStateVector& subject, float healer_integrity, float healer_cohesion) {
        float before = level;
        float operator_val = healer_integrity * healer_cohesion;
        float delta = operator_val * 0.1f;
        
        // Rotate all pillars toward north pole (θ=0) to counteract equator pull
        for (int i = 0; i < NUM_PILLARS; i++) {
            float theta = pillar_value_to_theta(subject[i]).to_float();
            float north_pole = 0.0f;
            float diff = north_pole - theta;
            subject[i] = apply_bloch_rotation(subject[i], vn::fp20_t(diff * delta));
        }
        
        level -= delta;
        if (level < MIN_CONSTRAINT) level = MIN_CONSTRAINT;
        
        float depth = subject[PILLAR_DEPTH].to_float();
        float depth_restore = delta * 0.5f;
        depth += depth_restore;
        if (depth > 1.0f) depth = 1.0f;
        subject[PILLAR_DEPTH] = vn::fp20_t(depth);
        
        record_event("recovery", before, level, depth - depth_restore, depth, delta, PILLAR_INTEGRITY);
    }
};

// ── Standalone helper ─────────────────────────────────────────

inline AccumulationResult compute_depth_overflow(float raw_angular_shift, float willpower, float depth, float constraint_level = 0.0f) {
    ConstraintState cs;
    cs.init(constraint_level);
    return cs.absorb_overflow(raw_angular_shift, willpower, depth);
}
