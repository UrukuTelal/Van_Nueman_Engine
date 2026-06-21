// PhotonicWrapper.h - RYGCBM Photonic Wrapper System
// Part of the COSMOS 16D Pillar Cosmology implementation
// Phase 2: Photonic Wrapper System (RYGCBM + Alpha Core)
//
// Matches: DeveloperConsole/backend/models/photonic_wrapper.py
// An entity selects 3 of 6 colors + Alpha core to form a Photonic Wrapper
// that shapes the semantic meaning of its interactions with the SVT.

#pragma once

#include "Entity.h"
#include "../scale/SemanticProjection.h"
#include <cmath>
#include <cstring>
#include <cstdio>

// ── Color Constants ──────────────────────────────────────────

#define COLOR_RED      0
#define COLOR_YELLOW   1
#define COLOR_GREEN    2
#define COLOR_CYAN     3
#define COLOR_BLUE     4
#define COLOR_MAGENTA  5
#define NUM_COLORS     6

#define ALPHA_WHITE    0
#define ALPHA_BLACK    1

// ── Color Semantics (Layer 4 Symbolic Projection) ────────────
// Each RYGCBM color maps to pillar indices (Layer 0 substrate ops)
// and carries symbolic meaning via SymbolicProjection.

struct ColorSemantic {
    int pillar_indices[3];   // max 3 pillars per color, -1 terminated
    float direction;         // 1.0 = positive rotation, -1.0 = negative
    const char* name;
};

static const ColorSemantic COLOR_SEMANTICS[NUM_COLORS] = {
    // Red   → Willpower(heroic_will), Force — agency, kinetic will
    { {1, 2, -1},  1.0f, "Red"     },
    // Yellow → Awareness(presence_mana), Presence — radiant awareness
    { {0, 8, -1},  1.0f, "Yellow"  },
    // Green  → Integrity(sacred_boundary), Cohesion(social_cohesion) — boundary & unity
    { {5, 6, -1},  1.0f, "Green"   },
    // Cyan   → Flux(flux_weaving) — time-thread / destiny path
    { {14, -1, -1}, 1.0f, "Cyan"   },
    // Blue   → Resistance — sacred boundary / cosmic order
    { {4, -1, -1}, 1.0f, "Blue"    },
    // Magenta → Relation(ancestor_memory), Attraction(gravitational_love)
    { {7, 11, -1}, 1.0f, "Magenta" },
};

// ── Photonic Wrapper ─────────────────────────────────────────

struct PhotonicWrapper {
    int colors[3];      // indices into COLOR_SEMANTICS
    int alpha;          // ALPHA_WHITE or ALPHA_BLACK
    bool inward;        // true = enchantment, false = broadcast
    
    // Initialize a default wrapper (Red + Green + Blue, White, outward)
    void init() {
        colors[0] = COLOR_RED;
        colors[1] = COLOR_GREEN;
        colors[2] = COLOR_BLUE;
        alpha = ALPHA_WHITE;
        inward = false;
    }
    
    // Initialize with specific colors
    void init_with(int c1, int c2, int c3, int a, bool is_inward) {
        colors[0] = c1;
        colors[1] = c2;
        colors[2] = c3;
        alpha = a;
        inward = is_inward;
    }
    
    // Get the rotation multiplier for a specific pillar index.
    // White Alpha: positive (amplify), Black Alpha: negative (invert/drain).
    float get_multiplier(int pillar_index) const {
        float base = (alpha == ALPHA_WHITE) ? 1.0f : -0.5f;
        float result = 0.0f;
        for (int i = 0; i < 3; i++) {
            const ColorSemantic& sem = COLOR_SEMANTICS[colors[i]];
            for (int j = 0; j < 3; j++) {
                if (sem.pillar_indices[j] == pillar_index) {
                    result += base * 0.33f;
                }
            }
        }
        return result;
    }
    
    // Apply the enchantment (inner face) — a persistent gate rotation
    // on the entity's own pillars each tick.
    void apply_enchantment(PillarStateVector& psv, float delta_time = 1.0f) const {
        float target_value = (alpha == ALPHA_WHITE) ? 1.0f : 0.0f;
        float rotation_rate = 0.01f * delta_time;
        
        for (int i = 0; i < 3; i++) {
            const ColorSemantic& sem = COLOR_SEMANTICS[colors[i]];
            for (int j = 0; j < 3; j++) {
                int p = sem.pillar_indices[j];
                if (p < 0) break;  // -1 terminated
                
                vn::fp20_t current = vn::fp20_t(psv[p]);
                vn::fp20_t target(target_value);

                vn::fp20_t theta = pillar_value_to_theta(current);
                vn::fp20_t target_theta = pillar_value_to_theta(target);
                vn::fp20_t diff = target_theta - theta;
                vn::fp20_t apply = diff * vn::fp20_t(rotation_rate);

                vn::fp20_t apply_abs = apply < vn::fp20_t(0) ? vn::fp20_t() - apply : apply;
                if (apply_abs > vn::fp20_t(0.0001f)) {
                    psv[p] = apply_bloch_rotation(vn::fp20_t(psv[p]), apply);
                }
            }
        }
    }
    
    // Get a human-readable label for this wrapper
    void get_label(char* buffer, int buffer_size) const {
        const char* direction_str = inward ? "Enchantment" : "Broadcast";
        const char* alpha_str = (alpha == ALPHA_WHITE) ? "Emission" : "Siphon";
        snprintf(buffer, buffer_size, "%s [%s]: %s + %s + %s",
                 direction_str, alpha_str,
                 COLOR_SEMANTICS[colors[0]].name,
                 COLOR_SEMANTICS[colors[1]].name,
                 COLOR_SEMANTICS[colors[2]].name);
    }
};

// ── Preset Wrappers ──────────────────────────────────────────

inline PhotonicWrapper make_shield_wrapper() {
    // White + Blue(Resistance) + Green(Integrity) + Magenta(Cohesion)
    PhotonicWrapper w;
    w.init_with(COLOR_BLUE, COLOR_GREEN, COLOR_MAGENTA, ALPHA_WHITE, false);
    return w;
}

inline PhotonicWrapper make_strike_wrapper() {
    // White + Red(Force) + Yellow(Awareness) + Cyan(Flux)
    PhotonicWrapper w;
    w.init_with(COLOR_RED, COLOR_YELLOW, COLOR_CYAN, ALPHA_WHITE, false);
    return w;
}

inline PhotonicWrapper make_heal_wrapper() {
    // White + Green(Growth) + Blue(Stability) + Magenta(Connection)
    PhotonicWrapper w;
    w.init_with(COLOR_GREEN, COLOR_BLUE, COLOR_MAGENTA, ALPHA_WHITE, false);
    return w;
}

inline PhotonicWrapper make_hollow_wrapper() {
    // Black + Red(Force) + Yellow(Awareness) — The Hollow
    PhotonicWrapper w;
    w.init_with(COLOR_RED, COLOR_YELLOW, COLOR_BLUE, ALPHA_BLACK, false);
    return w;
}

inline PhotonicWrapper make_enchant_wrapper() {
    // Red + Green + Blue inward-facing persistent gate
    PhotonicWrapper w;
    w.init_with(COLOR_RED, COLOR_GREEN, COLOR_BLUE, ALPHA_WHITE, true);
    return w;
}

inline PhotonicWrapper make_poison_wrapper() {
    // Black Alpha + Red + Yellow inward — siphons Force and Awareness
    PhotonicWrapper w;
    w.init_with(COLOR_RED, COLOR_YELLOW, COLOR_CYAN, ALPHA_BLACK, true);
    return w;
}

// ── Wrapper-Aware TRANSFORM ──────────────────────────────────
// Applies a PhotonicWrapper's rotation multipliers to a TRANSFORM operation.
// This modifies the operator value based on the wrapper's color composition.

inline float wrapper_modify_operator(float operator_value, const PhotonicWrapper& wrapper, int operator_pillar) {
    float mult = wrapper.get_multiplier(operator_pillar);
    return operator_value * (1.0f + mult);
}
