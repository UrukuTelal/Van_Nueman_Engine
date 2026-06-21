// HopfPID.h - Hopf-PID Interaction Engine
// Scale-Invariant Superfluid Vacuum Theory (SVT) Implementation
// Bridges 512D Thought Realms -> 32D Manifest Membrane via Hopf Fibration
//
// Axiom: "Nothing is deleted. Information is only rotated.
//         The static is the un-compiled code."
//
// Dependencies: Entity.h (PillarStateVector, Bloch helpers, fp20_t)
//               Cord.h (entanglement topology)
//               wht.h / wht_packet.h (WHT re-indexing protocol)

#pragma once

#include "Entity.h"
#include "Transform.h"
#include "Cord.h"
#include "../audio/wht.h"
#include "../audio/wht_packet.h"
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <vector>

// ── Hopf-PID Dimensional Constants ─────────────────────────
// 512D High-Fidelity Thought Realm -> 32D Hex-Lattice Membrane
// 512 = 32 WHT frames x 16 pillars per frame
// The remaining 480 dimensions (512 - 32) are internal fiber DoF
// organized as 30 pillar-blocks per fiber.

constexpr int HOPF_FIBER_DIM    = 512;  // S^511 full thought sphere
constexpr int HOPF_BASE_DIM     = 32;   // S^31  manifest membrane
constexpr int HOPF_FIBER_RANK   = HOPF_FIBER_DIM - HOPF_BASE_DIM;  // 480
constexpr int HOPF_FRAME_COUNT  = 32;   // WTH frames per entity
constexpr int FIBER_BLOCK_COUNT = HOPF_FIBER_RANK / NumPillars;    // 30
constexpr int HOPF_MAX_FIBERS   = 256;  // max concurrent phase-locked loops

constexpr double PLANCK_LENGTH_NORM = 1.0e-3;  // 1mm granularity floor
constexpr double PLANCK_TIME_NORM   = 1.0e-6;  // 1us temporal granularity
constexpr double PLANCK_THETA_EPS   = 1.0e-8;  // min resolvable Bloch rotation

// ── Normalize Thought Vector ───────────────────────────────
// Ensures the 512D thought state is a unit vector on S^511 before
// Hopf projection. Without normalization, the projection assumes
// unit norm but may receive arbitrarily-scaled input, producing
// incorrect membrane values.

inline void normalize_thought(float* thought, int dim = HOPF_FIBER_DIM) {
    float sum_sq = 0.0f;
    for (int i = 0; i < dim; i++) {
        sum_sq += thought[i] * thought[i];
    }
    float norm = std::sqrt(sum_sq);
    if (norm > 1e-10f) {
        float inv_norm = 1.0f / norm;
        for (int i = 0; i < dim; i++) {
            thought[i] *= inv_norm;
        }
    }
}

// ── HopfProjectionMatrix ───────────────────────────────────
// 512x32 projection: maps the full thought state to the membrane.
// Built from WHT basis vectors with fiber-specific phase rotations.
// Invariant: P^T * P = I_32 (partial isometry).

struct HopfProjectionMatrix {
    float coeff[HOPF_BASE_DIM][HOPF_FIBER_DIM];   // 32 x 512

    static int popcount_(int x) {
        int count = 0;
        while (x) { count += x & 1; x >>= 1; }
        return count;
    }

    void init_hopf() {
        const float inv_sqrt_512 = 1.0f / std::sqrt(512.0f);
        for (int i = 0; i < HOPF_BASE_DIM; i++) {
            for (int j = 0; j < HOPF_FIBER_DIM; j++) {
                int parity = popcount_(i & j) & 1;
                coeff[i][j] = (parity ? -inv_sqrt_512 : inv_sqrt_512);
            }
        }
    }

    // Project: 512D thought state -> 32D membrane
    // Normalizes thought vector to unit S^511 before projection.
    void project(float thought[512], float membrane[32]) const {
        normalize_thought(thought);
        std::memset(membrane, 0, 32 * sizeof(float));
        for (int i = 0; i < HOPF_BASE_DIM; i++) {
            float sum = 0.0f;
            for (int j = 0; j < HOPF_FIBER_DIM; j++) {
                sum += coeff[i][j] * thought[j];
            }
            membrane[i] = sum;
        }
    }

    // Lift: 32D membrane + fiber seed -> 512D thought state
    // thought = P^T * membrane, then fiber overwrites dimensions [32, 512).
    // P^T * membrane spreads the membrane across all 512 dimensions;
    // the fiber occupies the nullspace (per-frame pillar variations).
    void lift(const float membrane[32], const float fiber_seed[480],
              float thought[512]) const {
        for (int j = 0; j < HOPF_FIBER_DIM; j++) {
            float sum = 0.0f;
            for (int i = 0; i < HOPF_BASE_DIM; i++) {
                sum += coeff[i][j] * membrane[i];
            }
            thought[j] = sum;
        }
        for (int j = 0; j < HOPF_FIBER_RANK; j++) {
            thought[HOPF_BASE_DIM + j] = fiber_seed[j];
        }
    }
};

// ── PlanckCapConfig ────────────────────────────────────────
// Bitrate filters enforcing Planck-scale thresholds.
// Prevents sub-Planck oscillations from consuming compute.

struct PlanckCapConfig {
    double l_P;        // Planck length in sim-units
    double t_P;        // Planck time in sim-units
    double theta_min;  // min resolvable phase rotation
    double bitrate_max;// max information throughput (bits/s)
    double rupture_threshold;  // Relational_Complexity cap

    static PlanckCapConfig engine_default() {
        return {
            1.0e-3,          // 1mm spatial granularity
            1.0e-6,          // 1us temporal granularity
            1.0e-8,          // min theta
            1.0e6,           // 1M bit/s throughput cap
            0.95             // rupture at 95% relational complexity
        };
    }

    static PlanckCapConfig vacuum_default() {
        return {
            1.616255e-35,    // true Planck length
            5.391247e-44,    // true Planck time
            1.0e-12,
            1.0e12,
            0.98
        };
    }
};

// ── PhaseLockedLoop (Fiber) ────────────────────────────────
// A persistent Hopf fiber: 30 pillar-blocks maintained as a
// coherent phase-locked loop. Each block is a 16D PillarStateVector
// at different scales/semantics, held in phase by the PLL.

struct PhaseLockedLoop {
    uint32_t fiber_id;
    uint32_t entity_id;
    float    phase_offset;       // global phase offset for this fiber
    float    coherence;          // [0,1] how tightly locked
    float    frequency;          // natural oscillation frequency
    float    damping;            // phase correction gain

    // 30 internal pillar blocks (the S^479 fiber)
    PillarStateVector blocks[FIBER_BLOCK_COUNT];

    // Phase reference (the S^31 base point projected from this fiber)
    float    base_projection[HOPF_BASE_DIM];

    bool     is_active;
    float    age;

    void init(uint32_t fid, uint32_t eid, float offset) {
        fiber_id = fid;
        entity_id = eid;
        phase_offset = offset;
        coherence = 0.8f;
        frequency = 1.0f;
        damping = 0.1f;
        is_active = true;
        age = 0.0f;
        std::memset(base_projection, 0, sizeof(base_projection));
        for (int b = 0; b < FIBER_BLOCK_COUNT; b++) {
            blocks[b].fill(vn::fp20_t(0.5f));
        }
    }

    // Phase-locked update: rotate all blocks to maintain coherence
    // with the base projection. Uses Bloch rotation -- never scalar add.
    // Thought vector = P^T * base_projection + fiber blocks.
    // Base occupies dimensions [0, HOPF_BASE_DIM); fiber occupies [HOPF_BASE_DIM, 512).
    // This ensures the base and fiber degrees of freedom are independent.
    void tick(const HopfProjectionMatrix& proj, float dt) {
        if (!is_active) return;
        age += dt;

        float phase_drift = frequency * dt * 0.1f;
        phase_offset = std::fmod(phase_offset + phase_drift, 2.0f * 3.14159265f);

        // Reconstruct thought vector: P^T * base + fiber blocks
        // Step 1: thought = P^T * base_projection (spreads membrane across all 512 dims)
        float thought[HOPF_FIBER_DIM];
        for (int j = 0; j < HOPF_FIBER_DIM; j++) {
            float sum = 0.0f;
            for (int i = 0; i < HOPF_BASE_DIM; i++) {
                sum += proj.coeff[i][j] * base_projection[i];
            }
            thought[j] = sum;
        }

        // Step 2: Overwrite fiber portion (dimensions [32, 512)) with fiber blocks.
        // 30 blocks x 16 pillars = 480 elements fills the fiber subspace exactly.
        for (int b = 0; b < FIBER_BLOCK_COUNT; b++) {
            float* block_start = thought + HOPF_BASE_DIM + b * NumPillars;
            for (int p = 0; p < NumPillars; p++) {
                block_start[p] = blocks[b][p];
            }
        }

        // Project to get current base
        proj.project(thought, base_projection);

        // Phase correction: pull blocks toward base-projected reference
        // using small Bloch rotations (NOT additive correction)
        float correction_strength = coherence * damping * dt;
        if (correction_strength > PLANCK_THETA_EPS) {
            for (int b = 0; b < FIBER_BLOCK_COUNT; b++) {
                for (int p = 0; p < NumPillars; p++) {
                    float current = blocks[b][p];
                    float ref     = base_projection[p % HOPF_BASE_DIM];
                    float theta   = pillar_value_to_theta(vn::fp20_t(current)).to_float();
                    float ref_theta = pillar_value_to_theta(vn::fp20_t(ref)).to_float();
                    float diff    = ref_theta - theta;
                    float pull    = diff * correction_strength;
                    if (std::fabs(pull) > PLANCK_THETA_EPS) {
                        blocks[b][p] = apply_bloch_rotation(vn::fp20_t(blocks[b][p]), vn::fp20_t(pull));
                    }
                }
            }
        }

        // Natural decay of coherence toward residue
        float decay = (coherence - 0.1f) * (1.0f - std::exp(-0.01f * dt));
        coherence -= decay;
        if (coherence < 0.1f) coherence = 0.1f;
    }

    bool is_locked() const {
        return is_active && coherence > 0.3f;
    }
};

// ── HopfPIDState ────────────────────────────────────────────
// Full 512D thought state + fiber array for one entity.

struct HopfPIDState {
    // 32 WHT frames x 16 pillars = 512
    // Each frame represents a pillar state at a different
    // WHT frequency / semantic context.
    PillarStateVector frames[HOPF_FRAME_COUNT];

    // Encoded WHT representation (32 coefficients per frame)
    float frame_wht[HOPF_FRAME_COUNT][WHT_N];

    // The 32D membrane projection (hex-lattice node values)
    float membrane[HOPF_BASE_DIM];

    // Active fibers for this entity
    std::vector<PhaseLockedLoop> fibers;
    int fiber_count;

    // Relational complexity (drives depth overflow detection)
    float relational_complexity;
    float complexity_cap;

    // Constraint & depth tracking (persisted across ticks)
    vn::fp20_t constraint_level;   // [0,1] accumulated rotational constraint
    vn::fp20_t depth_buffer;       // accumulated rotational overflow

    // Planck-aware tick metadata
    double tick_accumulator;  // sub-Planck accumulation
    double bitrate_budget;    // remaining bits this tick
    bool   in_rupture;
    float  rupture_timer;

    void init() {
        for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
            frames[f].fill(vn::fp20_t(0.5f));
            std::memset(frame_wht[f], 0, sizeof(frame_wht[f]));
        }
        std::memset(membrane, 0, sizeof(membrane));
        fibers.resize(HOPF_MAX_FIBERS);
        fiber_count = 0;
        relational_complexity = 0.0f;
        complexity_cap = 0.95f;
        constraint_level = vn::fp20_t(0.0f);
        depth_buffer = vn::fp20_t(0.0f);
        tick_accumulator = 0.0;
        bitrate_budget = 1.0e6;
        in_rupture = false;
        rupture_timer = 0.0f;
    }

    // Encode all frames through WHT
    void encode_all_frames() {
        for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
            float pillars_float[NumPillars];
            for (int p = 0; p < NumPillars; p++) {
                pillars_float[p] = frames[f][p];
            }
            encode_pillar_vector(pillars_float, frame_wht[f]);
        }
    }

    // Decode WHT back to pillar frames
    void decode_all_frames() {
        for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
            float pillars_float[NumPillars];
            decode_pillar_vector(frame_wht[f], pillars_float);
            for (int p = 0; p < NumPillars; p++) {
                frames[f][p] = vn::fp20_t(pillars_float[p]);
            }
        }
    }
};

// ── Planck Cap Filter ──────────────────────────────────────
// Enforce Planck-scale thresholds on delta-theta values.
// Sub-Planck rotations are accumulated across ticks.
// Supra-Planck but sub-threshold values pass through.
// Overflow triggers depth buffer saturation.

inline bool planck_cap_filter(float& delta_theta, double& accumulator,
                               const PlanckCapConfig& cfg, double dt) {
    double signed_dt = static_cast<double>(delta_theta);
    double abs_dt = std::fabs(signed_dt);
    if (abs_dt < cfg.theta_min) {
        // Sub-Planck: accumulate with sign preservation
        accumulator += signed_dt;
        if (std::fabs(accumulator) < cfg.theta_min) {
            delta_theta = 0.0f;
            return true;
        }
        // Accumulator threshold reached: emit accumulated rotation (sign is naturally correct)
        delta_theta = static_cast<float>(accumulator);
        accumulator = 0.0;
        return false;
    }
    // Supra-Planck: pass through, reset accumulator
    accumulator = 0.0;
    return false;
}

// ── Relational Complexity ──────────────────────────────────
// Compute the relational complexity of the HopfPID state.
// High complexity means the membrane cannot faithfully represent
// the fiber state -> depth overflow / rupture risk.

inline float compute_relational_complexity(const HopfPIDState& state) {
    // Complexity = entropy of WHT coefficient distribution
    // across all frames, normalized to [0,1]
    float total_entropy = 0.0f;
    for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
        float frame_norm = 0.0f;
        for (int i = 0; i < WHT_N; i++) {
            frame_norm += state.frame_wht[f][i] * state.frame_wht[f][i];
        }
        if (frame_norm < 1e-8f) continue;
        float entropy = 0.0f;
        for (int i = 0; i < WHT_N; i++) {
            float p = (state.frame_wht[f][i] * state.frame_wht[f][i]) / frame_norm;
            if (p > 1e-8f) {
                entropy -= p * std::log(p);
            }
        }
        total_entropy += entropy;
    }
    float max_entropy = static_cast<float>(HOPF_FRAME_COUNT) * std::log(32.0f);
    return total_entropy / (max_entropy + 1e-8f);
}

// ── Core TRANSFORM: Geometric Phase Shift ──────────────────
// The fundamental operation of the Hopf-PID engine.
//
// Δθ_P = (M_operator · F_influence) / (W_subject · D_depth)
//
// All operations are geometric phase rotations on the Bloch sphere.
// NO additive/subtractive logic on pillar values.
// The operator and target exist in the 512D fiber space; the
// phase shift propagates to the 32D membrane via Hopf projection.

inline float compute_delta_theta(
    const PillarStateVector& actor_psv,
    const PillarStateVector& subject_psv,
    int operator_pillar,
    int target_pillar,
    vn::fp20_t& depth_buffer,
    vn::fp20_t& constraint_level,
    const PlanckCapConfig& cfg = PlanckCapConfig::engine_default()
) {
    // Step 1: Alignment (Bloch dot product, already normalized [-1, 1])
    float alignment = bloch_dot_product(actor_psv, subject_psv);

    // Step 2: Read operator magnitude from actor (already float)
    float operator_mag = actor_psv[operator_pillar];
    float influence    = actor_psv[Influence];
    float willpower    = subject_psv[Willpower];

    // Step 3-5: Use unified angular shift from TransformCore
    ShiftResult shift = compute_angular_shift(
        vn::fp20_t(operator_mag), vn::fp20_t(influence), vn::fp20_t(alignment),
        vn::fp20_t(willpower), depth_buffer
    );

    // Apply depth/constraint side effects
    depth_buffer = depth_buffer - vn::fp20_t(shift.depth_consumed);
    constraint_level = constraint_level + vn::fp20_t(shift.constraint_delta);
    if (constraint_level > vn::fp20_t(1.0f)) {
        constraint_level = vn::fp20_t(1.0f);
    }

    // Step 7: Attenuate willpower-targeting transforms
    float applied = shift.shift_rad;
    if (target_pillar == Willpower) applied *= 0.5f;

    return applied;
}

// ── TRANSFORM on HopfPID State ────────────────────────────
// Applies the geometric phase shift across ALL 32 WHT frames
// of the subject, weighted by fiber coherence.
// The shift propagates topologically -- every frame receives
// a phase-coherent rotation.

inline void hopf_transform(
    const HopfPIDState& actor_state,
    HopfPIDState& subject_state,
    int operator_pillar,
    int target_pillar,
    vn::fp20_t& depth_buffer,
    vn::fp20_t& constraint_level,
    const PlanckCapConfig& cfg = PlanckCapConfig::engine_default()
) {
    // Compute base delta-theta from the first frame
    float delta_theta = compute_delta_theta(
        actor_state.frames[0],
        subject_state.frames[0],
        operator_pillar,
        target_pillar,
        depth_buffer,
        constraint_level,
        cfg
    );

    if (std::fabs(delta_theta) < PLANCK_THETA_EPS) return;

    // Apply rotation to ALL frames, scaled by per-frame coherence weight
    for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
        float coherence_weight = 1.0f;
        if (f > 0) {
            // Outer frames receive attenuated rotation based on
            // alignment with the primary frame
            float align = bloch_dot_product(subject_state.frames[0],
                                            subject_state.frames[f]);
            coherence_weight = (align + 1.0f) * 0.5f;  // remap to [0,1]
            if (coherence_weight < 0.01f) continue;
        }

        float frame_dt = delta_theta * coherence_weight;
        vn::fp20_t dt_fp20(frame_dt);

        // Bloch-rotate the target pillar (NOT scalar arithmetic)
        vn::fp20_t current = vn::fp20_t(subject_state.frames[f][target_pillar]);
        subject_state.frames[f][target_pillar] = apply_bloch_rotation(current, dt_fp20).to_float();

        // Entanglement: every frame's non-target pillars also rotate fractionally
        // via the WHT cross-coupling (topological, not linear)
        float coupling = frame_dt * 0.05f;
        if (std::fabs(coupling) > PLANCK_THETA_EPS) {
            for (int p = 0; p < NumPillars; p++) {
                if (p == target_pillar) continue;
                if (p == Willpower && target_pillar != Willpower) {
                    // Willpower only gets 50% cross-coupling
                    subject_state.frames[f][p] = apply_bloch_rotation(
                        vn::fp20_t(subject_state.frames[f][p]), vn::fp20_t(coupling * 0.5f)
                    ).to_float();
                } else {
                    subject_state.frames[f][p] = apply_bloch_rotation(
                        vn::fp20_t(subject_state.frames[f][p]), vn::fp20_t(coupling)
                    ).to_float();
                }
            }
        }
    }

    // Re-encode all frames through WHT after rotation
    subject_state.encode_all_frames();

    // Re-project to membrane
    {
        HopfProjectionMatrix proj;
        proj.init_hopf();
        float thought[HOPF_FIBER_DIM];
        for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
            for (int p = 0; p < NumPillars; p++) {
                thought[f * NumPillars + p] = subject_state.frames[f][p];
            }
        }
        proj.project(thought, subject_state.membrane);
    }

    // Update relational complexity
    subject_state.relational_complexity = compute_relational_complexity(subject_state);
}

// ── WHT Re-Indexing Protocol ───────────────────────────────
// Topological entanglement: a torsion change in ANY pillar
// triggers a phase-angle update in ALL connected pillars
// via the WHT basis re-indexing.
//
// Because the Hopf fibers are topologically linked, the WHT
// coefficients carry the entanglement structure. Re-indexing
// permutes the WHT basis to distribute the torsion signal.

inline void wht_reindex_propagation(
    HopfPIDState& subject,
    int trigger_pillar,
    float delta_theta,
    CordField& cord_field,
    uint32_t entity_id
) {
    if (std::fabs(delta_theta) < PLANCK_THETA_EPS) return;

    const float PI = 3.14159265f;

    // Step 1: Compute the re-indexing permutation from the trigger pillar
    // permutation[i] = (i + trigger_pillar * 2) % WHT_N
    int perm[WHT_N];
    for (int i = 0; i < WHT_N; i++) {
        perm[i] = (i + trigger_pillar * 2) & (WHT_N - 1);
    }

    // Step 2: Apply permutation to all frame WHT coefficients
    for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
        float reindexed[WHT_N];
        for (int i = 0; i < WHT_N; i++) {
            reindexed[perm[i]] = subject.frame_wht[f][i];
        }
        std::memcpy(subject.frame_wht[f], reindexed, sizeof(reindexed));
    }

    // Step 3: Decode permuted WHT back to pillar frames
    // This distributes the torsion across ALL pillars topologically
    subject.decode_all_frames();

    // Step 4: Propagate the torsion delta through active cords
    // (topological entanglement between entities)
    Cord nearby_cords[MAX_CORDS_PER_ENTITY];
    int cord_count = cord_field.get_for_entity(entity_id, nearby_cords, MAX_CORDS_PER_ENTITY);
    for (int c = 0; c < cord_count; c++) {
        float cord_strength = nearby_cords[c].strength;
        if (cord_strength < CORD_MIN_STRENGTH) continue;

        float coupled_theta = delta_theta * cord_strength;
        // The partner's pillars receive a phase-coupled rotation
        // (the cord syncs tau/phi values through the entanglement)
        // This is handled by the cord system externally, we just
        // mark that propagation is needed.
    }

    // Step 5: Re-encode after re-indexing
    subject.encode_all_frames();

    // Step 6: Update relational complexity (re-indexing changes it)
    subject.relational_complexity = compute_relational_complexity(subject);
}

// ── Rupture Detection ──────────────────────────────────────
// If Relational_Complexity > Lattice_Hardware_Cap, trigger
// a Depth Overflow (Rupture): the fiber structure collapses
// into the depth buffer, resetting frames and freeing complexity.

inline bool detect_rupture_condition(const HopfPIDState& state) {
    return state.relational_complexity > state.complexity_cap;
}

inline void trigger_rupture(HopfPIDState& state) {
    if (!state.in_rupture) {
        state.in_rupture = true;
        state.rupture_timer = 0.0f;
    }

    // Rupture protocol: collapse all fibers back to baseline
    // via Bloch rotation (NOT reset to zero)
    for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
        for (int p = 0; p < NumPillars; p++) {
            float baseline = (p == Harm) ? 0.1f : 0.5f;
            float current = state.frames[f][p];
            float theta = pillar_value_to_theta(vn::fp20_t(current)).to_float();
            float target_theta = pillar_value_to_theta(vn::fp20_t(baseline)).to_float();
            float collapse = target_theta - theta;
            state.frames[f][p] = apply_bloch_rotation(
                vn::fp20_t(state.frames[f][p]), vn::fp20_t(collapse * 0.5f)
            ).to_float();
        }
    }

    // Halve the relational complexity
    state.relational_complexity *= 0.5f;

    // Suspend TICK accumulation during rupture
    state.tick_accumulator = 0.0;

    state.encode_all_frames();

    // Rebuild Hopf projection
    {
        HopfProjectionMatrix proj;
        proj.init_hopf();
        float thought[HOPF_FIBER_DIM];
        for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
            for (int p = 0; p < NumPillars; p++) {
                thought[f * NumPillars + p] = state.frames[f][p];
            }
        }
        proj.project(thought, state.membrane);
    }

    state.in_rupture = state.relational_complexity > state.complexity_cap;
}

// ── Hopfion Soliton Stabilization ─────────────────────────
// Stabilizes immaterial entities (Fae/Aetheric) that lack
// physical Integrity. Uses local Phase-Locked Loops that
// maintain soliton boundaries through topological charge.

inline void hopfion_soliton_stabilize(
    HopfPIDState& state,
    float integrity_value,
    float dt
) {
    const float PI = 3.14159265f;

    // Soliton only needed when Integrity < 0.2
    if (integrity_value >= 0.2f) return;

    // Compute topological charge from the WHT frame entanglement
    float topological_charge = 0.0f;
    for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
        for (int i = 0; i < WHT_N; i++) {
            topological_charge += state.frame_wht[f][i];
        }
    }
    topological_charge = std::fmod(topological_charge, 2.0f * PI);

    // For each frame, apply a soliton-preserving Bloch rotation:
    // the rotation angle is proportional to the topological charge
    // and inversely proportional to the integrity deficit.
    float deficit = 0.2f - integrity_value;
    float soliton_strength = deficit * 0.1f;

    if (soliton_strength < PLANCK_THETA_EPS) return;

    for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
        // Soliton phase rotation on the Cohesion pillar
        float cohesion = state.frames[f][Cohesion];
        float cohesion_theta = pillar_value_to_theta(vn::fp20_t(cohesion)).to_float();
        float soliton_delta = topological_charge * soliton_strength * dt;
        float new_theta = cohesion_theta + soliton_delta;
        const float TWO_PI = 2.0f * PI;
        new_theta = std::fmod(new_theta, TWO_PI);
        if (new_theta < 0.0f) new_theta += TWO_PI;
        if (new_theta > PI) new_theta = TWO_PI - new_theta;
        float new_cohesion = (std::cos(new_theta) + 1.0f) * 0.5f;
        state.frames[f][Cohesion] = vn::fp20_t(new_cohesion);

        // Coupled rotation on Warmth and Relation (the soliton triad)
        float warmth = state.frames[f][Warmth];
        float warmth_theta = pillar_value_to_theta(vn::fp20_t(warmth)).to_float();
        float coupled = soliton_delta * 0.3f;
        float new_warmth_theta = warmth_theta + coupled;
        new_warmth_theta = std::fmod(new_warmth_theta, TWO_PI);
        if (new_warmth_theta < 0.0f) new_warmth_theta += TWO_PI;
        if (new_warmth_theta > PI) new_warmth_theta = TWO_PI - new_warmth_theta;
        state.frames[f][Warmth] = vn::fp20_t(
            (std::cos(new_warmth_theta) + 1.0f) * 0.5f
        );

        float relation = state.frames[f][Relation];
        float relation_theta = pillar_value_to_theta(vn::fp20_t(relation)).to_float();
        float new_rel_theta = relation_theta + coupled * 0.5f;
        new_rel_theta = std::fmod(new_rel_theta, TWO_PI);
        if (new_rel_theta < 0.0f) new_rel_theta += TWO_PI;
        if (new_rel_theta > PI) new_rel_theta = TWO_PI - new_rel_theta;
        state.frames[f][Relation] = vn::fp20_t(
            (std::cos(new_rel_theta) + 1.0f) * 0.5f
        );
    }

    state.encode_all_frames();
}

// ── HopfTick: Full Engine Tick ─────────────────────────────
// Orchestrates the complete Hopf-PID pipeline for one entity:
// 1. Planck-cap accumulation
// 2. Fiber phase-locked loop update
// 3. Hopf projection to membrane
// 4. Relational complexity monitoring
// 5. Rupture detection and handling
// 6. Hopfion soliton stabilization for low-integrity entities
// 7. WHT re-indexing propagation

inline void hopf_tick(
    HopfPIDState& state,
    CordField& cord_field,
    uint32_t entity_id,
    float dt,
    const PlanckCapConfig& cfg = PlanckCapConfig::engine_default()
) {
    // 1. Sub-Planck accumulation
    state.tick_accumulator += dt;
    if (state.tick_accumulator < cfg.t_P) return;
    float effective_dt = static_cast<float>(state.tick_accumulator);
    state.tick_accumulator = 0.0;

    // 2. Update all active fibers (phase-locked loops)
    HopfProjectionMatrix proj;
    proj.init_hopf();
    for (int i = 0; i < state.fiber_count; i++) {
        if (state.fibers[i].is_active) {
            state.fibers[i].tick(proj, effective_dt);
        }
    }

    // 3. Hopf projection to membrane
    {
        float thought[HOPF_FIBER_DIM];
        for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
            for (int p = 0; p < NumPillars; p++) {
                thought[f * NumPillars + p] = state.frames[f][p];
            }
        }
        proj.project(thought, state.membrane);
    }

    // 4. Relational complexity tracking
    state.relational_complexity = compute_relational_complexity(state);

    // 5. Rupture detection
    if (detect_rupture_condition(state)) {
        trigger_rupture(state);
        return;
    }

    // 6. Hopfion soliton stabilization (if integrity is low)
    float integrity = state.frames[0][Integrity];
    if (integrity < 0.2f) {
        hopfion_soliton_stabilize(state, integrity, effective_dt);
    }
}

// ── Debug Print ────────────────────────────────────────────

inline void hopf_pid_print(const HopfPIDState& state, const char* label = "HopfPID") {
    printf("[%s] frames=%d fibers=%d/%d rc=%.3f rupture=%s membrane=[%.3f %.3f ... %.3f]\n",
           label, HOPF_FRAME_COUNT, state.fiber_count, HOPF_MAX_FIBERS,
           state.relational_complexity,
           state.in_rupture ? "YES" : "no",
           state.membrane[0], state.membrane[1], state.membrane[HOPF_BASE_DIM - 1]);
}

inline void transform_result_print(const TransformResult& r) {
    r.print();
}
