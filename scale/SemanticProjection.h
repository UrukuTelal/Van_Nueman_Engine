#pragma once

#include "../include/Entity.h"
#include "ScaleExponent.h"
#include <cstdint>
#include <cmath>

// ── Section XIII: 4-Layer Semantic Projection Stack ──────────────────────
// The 16 Pillars are invariant substrate operators (Layer 0).
// All biological, cognitive, civilizational, and symbolic meanings
// are applied dynamically through this projection stack.
//
// Layer 0 (Substrate Mechanics) — NOT in this file. See Entity.h, Transform.h.
//   The pillars are pre-semantic operators:
//     Warmth     = energetic transfer throughput
//     Integrity  = boundary coherence
//     Attraction = resonance gradient
//     Harm       = forced rotational transformation
//     Distortion = phase-angle torsion
//
// Layer 1 (Biological Interpretation) — survival-oriented embodiment
// Layer 2 (Cognitive Interpretation)  — internal subjective phenomenology
// Layer 3 (Civilizational Encoding)   — institutional/societal externalization
// Layer 4 (Mythic/Symbolic Projection) — narrative/archetypal compression
// ──────────────────────────────────────────────────────────────────────────

struct ProjectionContext {
    uint32_t layer;             // 0-4
    uint32_t scale;             // SCALE_ORGANISM, etc.
    float species_id;           // 0 = generic, 1 = human, etc.
    float cultural_offset;      // cultural modulation factor [0,1]
};

static constexpr ProjectionContext DEFAULT_BIOLOGICAL  = {1, SCALE_ORGANISM, 0.0f, 0.0f};
static constexpr ProjectionContext DEFAULT_COGNITIVE   = {2, SCALE_ORGANISM, 0.0f, 0.0f};
static constexpr ProjectionContext DEFAULT_CIVILIZATIONAL = {3, SCALE_SOCIETY, 0.0f, 0.5f};
static constexpr ProjectionContext DEFAULT_SYMBOLIC    = {4, SCALE_SOCIETY, 0.0f, 0.8f};

// ── Layer 1: Biological Interpretation ──────────────────────────────────
// Maps substrate operators to survival-oriented biological meaning:
//   Warmth     → metabolic heat, energy throughput
//   Attraction → resource-seeking, mating drive
//   Integrity  → cellular barrier coherence, membrane stability
//   Harm       → tissue stress, rotational displacement from rest
//   Distortion → sensory noise, neurological phase scrambling

struct BiologicalProjection {
    float metabolic_rate;       // from Warmth (energetic throughput)
    float membrane_integrity;   // from Integrity (boundary coherence)
    float stress_level;         // from Harm (rotational displacement from rest)
    float sensory_noise;        // from Distortion (phase torsion in perception)
    float resource_affinity;    // from Attraction (resonance gradient toward resources)
    float awareness_orientation; // from Awareness (inward/outward locus × -1/1)
    float willpower_stamina;    // from Willpower (vector inertia against state change)
    float force_output;         // from Force (kinetic energy driving action)
    float flux_metabolism;      // from Flux (clock cycle velocity)
    float depth_reservoir;      // from Depth (latent metabolic reserves)

    static BiologicalProjection project(const PillarStateVector& psv,
                                         const ProjectionContext& ctx = DEFAULT_BIOLOGICAL) {
        BiologicalProjection bp{};
        bp.metabolic_rate        = psv.pillars[Warmth];
        bp.membrane_integrity     = psv.pillars[Integrity];
        bp.stress_level           = psv.pillars[Harm];
        bp.sensory_noise          = psv.pillars[Distortion];
        bp.resource_affinity      = psv.pillars[Attraction];
        bp.awareness_orientation   = psv.pillars[Awareness] * 2.0f - 1.0f;
        bp.willpower_stamina      = psv.pillars[Willpower];
        bp.force_output           = psv.pillars[Force];
        bp.flux_metabolism        = psv.pillars[Flux];
        bp.depth_reservoir        = psv.pillars[Depth];
        (void)ctx;
        return bp;
    }
};

// ── Layer 2: Cognitive Interpretation ───────────────────────────────────
// Maps substrate operators to internal subjective phenomenology:
//   Warmth     → comfort, emotional safety
//   Integrity  → identity coherence, self-boundary
//   Attraction → curiosity, affinity, interest
//   Distortion → imagination, illusion, cognitive flexibility
//   Relation   → social memory, relationship topology
//   Memory     → experiential history buffer
//   Harm       → internal conflict, cognitive dissonance
//   Awareness  → focus direction (inward reflection / outward perception)

struct CognitiveProjection {
    float comfort;              // from Warmth
    float identity_coherence;   // from Integrity
    float curiosity;            // from Attraction
    float imaginativeness;      // from Distortion (positive torsion = creativity)
    float social_memory;        // from Relation
    float experiential_memory;  // from Memory
    float cognitive_load;       // from Harm (internal rotational tension)
    float focus_inward;         // from Awareness (negative locus)
    float agency;               // from Willpower + Force
    float depth_capacity;       // from Depth (cognitive reserve)

    static CognitiveProjection project(const PillarStateVector& psv,
                                        const ProjectionContext& ctx = DEFAULT_COGNITIVE) {
        CognitiveProjection cp{};
        cp.comfort              = psv.pillars[Warmth];
        cp.identity_coherence    = psv.pillars[Integrity];
        cp.curiosity             = psv.pillars[Attraction];
        cp.imaginativeness       = psv.pillars[Distortion];
        cp.social_memory         = psv.pillars[Relation];
        cp.experiential_memory   = psv.pillars[Memory];
        cp.cognitive_load        = psv.pillars[Harm];
        cp.focus_inward          = 1.0f - psv.pillars[Awareness];
        cp.agency                = (psv.pillars[Willpower] +
                                     psv.pillars[Force]) * 0.5f;
        cp.depth_capacity        = psv.pillars[Depth];
        (void)ctx;
        return cp;
    }
};

// ── Layer 3: Civilizational Encoding ────────────────────────────────────
// Maps substrate operators to institutional/societal structures:
//   Integrity  → laws, borders, security protocols
//   Memory     → archives, databases, historical records
//   Flux       → logistics, economic throughput, communication velocity
//   Cohesion   → tribal unity, network stability, social bonds
//   Willpower  → sovereignty, resistance to external domination
//   Force      → production capacity, military/economic power
//   Relation   → diplomacy, trade networks, alliances
//   Awareness  → surveillance, intelligence gathering

struct CivilizationalProjection {
    float legal_integrity;      // from Integrity
    float archival_depth;       // from Memory
    float economic_velocity;    // from Flux
    float social_cohesion;      // from Cohesion
    float sovereignty;          // from Willpower
    float production_force;     // from Force
    float diplomatic_relation;  // from Relation
    float surveillance;         // from Awareness
    float cultural_influence;   // from Influence
    float institutional_stability; // composite

    static CivilizationalProjection project(const PillarStateVector& psv,
                                             const ProjectionContext& ctx = DEFAULT_CIVILIZATIONAL) {
        CivilizationalProjection cp{};
        cp.legal_integrity       = psv.pillars[Integrity];
        cp.archival_depth        = psv.pillars[Memory];
        cp.economic_velocity      = psv.pillars[Flux];
        cp.social_cohesion        = psv.pillars[Cohesion];
        cp.sovereignty           = psv.pillars[Willpower];
        cp.production_force       = psv.pillars[Force];
        cp.diplomatic_relation    = psv.pillars[Relation];
        cp.surveillance           = psv.pillars[Awareness];
        cp.cultural_influence     = psv.pillars[Influence];
        float coherence = psv.pillars[Integrity];
        float stability = psv.pillars[Cohesion];
        float resistance = psv.pillars[Resistance];
        cp.institutional_stability = (coherence + stability + resistance) / 3.0f;
        (void)ctx;
        return cp;
    }
};

// ── Layer 4: Mythic/Symbolic Projection ─────────────────────────────────
// Compresses substrate dynamics into narrative archetypes and symbols:
//   Warmth     → divine fire, compassion, hearth
//   Distortion → trickster spirits, dream magic, illusion
//   Attraction → destiny, love, gravity, fate
//   Harm       → corruption, initiation, curse, transformation
//   Depth      → abyss, soul, hidden reservoir, underworld
//   Memory     → ancestor voices, collective unconscious
//   Integrity  → sacred boundary, purity, cosmic order
//   Willpower  → divine will, fate resistance, hero's resolve

struct SymbolicProjection {
    float numinous_warmth;      // from Warmth (divine fire / compassion)
    float trickster_phase;      // from Distortion (torsion as creative/destructive force)
    float gravitational_love;   // from Attraction (destiny/fate gravity)
    float transformational_harm; // from Harm (initiation through rotational crisis)
    float abyss_depth;          // from Depth (soul reservoir / underworld proximity)
    float ancestor_memory;      // from Memory (collective unconscious signal)
    float sacred_boundary;      // from Integrity (cosmic order / purity)
    float heroic_will;          // from Willpower (fate resistance)
    float flux_weaving;         // from Flux (time-thread / destiny path)
    float presence_mana;        // from Presence (spiritual density / charisma)

    static SymbolicProjection project(const PillarStateVector& psv,
                                       const ProjectionContext& ctx = DEFAULT_SYMBOLIC) {
        SymbolicProjection sp{};
        sp.numinous_warmth        = psv.pillars[Warmth];
        sp.trickster_phase         = psv.pillars[Distortion];
        sp.gravitational_love      = psv.pillars[Attraction];
        sp.transformational_harm   = psv.pillars[Harm];
        sp.abyss_depth             = psv.pillars[Depth];
        sp.ancestor_memory         = psv.pillars[Memory];
        sp.sacred_boundary         = psv.pillars[Integrity];
        sp.heroic_will             = psv.pillars[Willpower];
        sp.flux_weaving            = psv.pillars[Flux];
        sp.presence_mana           = psv.pillars[Presence];
        (void)ctx;
        return sp;
    }
};

// ── Canonical Ontological Rule (Section XIII-E) ─────────────────────────
// The Pillars define mechanics.
// Observers define meaning.
// The SemanticProjection binds them together.
// ──────────────────────────────────────────────────────────────────────────
