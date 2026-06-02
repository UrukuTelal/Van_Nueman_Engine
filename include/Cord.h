// Cord.h - Entanglement Cord System
// Part of the COSMOS 16D Pillar Cosmology implementation
// Phase 4: Entanglement / Cord System
//
// Matches: DeveloperConsole/backend/models/entanglement.py
// Cords are bidirectional iT links between entities that enable
// tau/phi synchronization. They decay exponentially toward a
// permanent residue. Re-entanglement is cheaper than initial bonding.

#pragma once

#include "Entity.h"
#include <cmath>
#include <cstdio>
#include <cstring>

// ── Cord Constants ────────────────────────────────────────────

#define CORD_DECAY_RATE 0.005f
#define CORD_REINFORCE_BOOST 0.2f
#define CORD_MIN_STRENGTH 0.001f
#define CORD_SEVER_STRENGTH 0.1f
#define MAX_CORDS_PER_ENTITY 16

// ── Cord ──────────────────────────────────────────────────────

struct Cord {
    uint32_t entity_a;
    uint32_t entity_b;
    float strength;
    float residue;
    float decay_rate;
    float age;
    float last_reinforced_at;
    bool is_active;
    
    void init(uint32_t a, uint32_t b, float initial_strength = 0.3f) {
        entity_a = (a < b) ? a : b;
        entity_b = (a < b) ? b : a;
        strength = (initial_strength > CORD_MIN_STRENGTH) ? initial_strength : CORD_MIN_STRENGTH;
        residue = CORD_MIN_STRENGTH;
        decay_rate = CORD_DECAY_RATE;
        age = 0.0f;
        last_reinforced_at = 0.0f;
        is_active = true;
    }
    
    bool is_alive() const {
        return is_active && strength > CORD_MIN_STRENGTH;
    }
    
    void tick(float delta_time = 1.0f) {
        if (!is_alive()) return;
        age += delta_time;
        float decay = (strength - CORD_MIN_STRENGTH) * (1.0f - std::exp(-decay_rate * delta_time));
        strength -= decay;
        if (strength < CORD_MIN_STRENGTH) strength = CORD_MIN_STRENGTH;
    }
    
    void reinforce(float amount = CORD_REINFORCE_BOOST) {
        strength += amount;
        if (strength > 1.0f) strength = 1.0f;
        last_reinforced_at = age;
    }
    
    float sever() {
        is_active = false;
        strength = CORD_MIN_STRENGTH;
        return CORD_SEVER_STRENGTH;
    }
    
    bool is_detectable_by(float awareness) const {
        return strength > awareness * 0.3f;
    }
    
    bool involves(uint32_t entity_id) const {
        return entity_a == entity_id || entity_b == entity_id;
    }
    
    uint32_t partner(uint32_t entity_id) const {
        return (entity_a == entity_id) ? entity_b : entity_a;
    }
    
    // Sync tau values between two entities through the cord.
    // Pull strength is proportional to cord strength.
    void sync_tau(float& tau_a, float& tau_b) const {
        float delta = (tau_b - tau_a) * strength * 0.1f;
        tau_a += delta;
        tau_b -= delta;
    }
    
    // Sync phi values (phase coherence).
    void sync_phi(float& phi_a, float& phi_b) const {
        float delta = (phi_b - phi_a) * strength * 0.05f;
        phi_a += delta;
        phi_b -= delta;
    }
    
    void print() const {
        printf("CORD %u<->%u: strength=%.3f residue=%.3f age=%.1f %s\n",
               entity_a, entity_b, strength, residue, age,
               is_alive() ? "ALIVE" : "RESIDUAL");
    }
};

// ── Cord Field ────────────────────────────────────────────────

struct CordField {
    Cord cords[256];  // fixed pool
    int cord_count;
    
    void init() {
        cord_count = 0;
    }
    
    int get_active_count() const {
        int count = 0;
        for (int i = 0; i < cord_count; i++) {
            if (cords[i].is_alive()) count++;
        }
        return count;
    }
    
    // Find cord between two entities
    Cord* find(uint32_t a, uint32_t b) {
        uint32_t ea = (a < b) ? a : b;
        uint32_t eb = (a < b) ? b : a;
        for (int i = 0; i < cord_count; i++) {
            if (cords[i].entity_a == ea && cords[i].entity_b == eb) {
                return &cords[i];
            }
        }
        return nullptr;
    }
    
    // Create a new cord (or return existing)
    Cord* create(uint32_t entity_a, uint32_t entity_b, float initial_strength = 0.3f) {
        Cord* existing = find(entity_a, entity_b);
        if (existing) return existing;
        
        if (cord_count >= 256) return nullptr;
        
        Cord& c = cords[cord_count++];
        c.init(entity_a, entity_b, initial_strength);
        return &c;
    }
    
    // Get all cords involving an entity (returns count)
    int get_for_entity(uint32_t entity_id, Cord* out_buffer, int max_count) const {
        int count = 0;
        for (int i = 0; i < cord_count && count < max_count; i++) {
            if (cords[i].involves(entity_id) && cords[i].is_alive()) {
                out_buffer[count++] = cords[i];
            }
        }
        return count;
    }
    
    // Age all cords
    void tick_all(float delta_time = 1.0f) {
        for (int i = 0; i < cord_count; i++) {
            cords[i].tick(delta_time);
        }
    }
    
    // Apply relation rotation from detectable cords
    // FIXED: Made const since it doesn't modify CordField members (only modifies the passed psv parameter)
    void affect_entity(PillarStateVector& psv, uint32_t entity_id, float awareness) const {
        const float PI = 3.14159265f;
        Cord nearby[MAX_CORDS_PER_ENTITY];
        int count = get_for_entity(entity_id, nearby, MAX_CORDS_PER_ENTITY);
        
        for (int i = 0; i < count; i++) {
            if (nearby[i].is_detectable_by(awareness)) {
                vn::fp20_t current_rel = psv[PILLAR_RELATION];
                vn::fp20_t theta = pillar_value_to_theta(current_rel);
                vn::fp20_t delta = vn::fp20_t(nearby[i].strength * 0.01f);
                psv[PILLAR_RELATION] = apply_bloch_rotation(psv[PILLAR_RELATION], delta);
            }
        }
    }
    
    void print_all() const {
        printf("CordField: %d total, %d active\n", cord_count, get_active_count());
        for (int i = 0; i < cord_count; i++) {
            cords[i].print();
        }
    }
};
