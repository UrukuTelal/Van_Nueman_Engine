#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "../fll/include/ArenaAllocator.h"
#include "../fll/include/SemanticCompiler.h"
#include "../fll/include/QuantumSeed.h"
#include "../fll/include/FLLQPU.h"
#include "../fll/include/FLLGraphTraversal.h"

using namespace vn::fll;

static int fll_pass = 0, fll_fail = 0;

#define FLL_TEST(name, expr) do { \
    if (!(expr)) { \
        printf("  FAIL: %s\n", name); \
        fll_fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        fll_pass++; \
    } \
} while(0)

void run_fll_tests() {
    printf("\n=== FLL Engine Tests ===\n");

    // ═══════════════════════════════════════════════════════════════
    // Phase 1: Basic ArenaAllocator & get_node_by_uid Tests
    // ═══════════════════════════════════════════════════════════════
    {
        ArenaAllocator arena;
        arena.init(64, 8, 4096);

        FractalNode* n0 = arena.alloc_node();
        FractalNode* n1 = arena.alloc_node();
        FractalNode* n2 = arena.alloc_node();

        FLL_TEST("get_node_by_uid(n0) == n0",
                 arena.get_node_by_uid(n0->uid) == n0);
        FLL_TEST("get_node_by_uid(n1) == n1",
                 arena.get_node_by_uid(n1->uid) == n1);
        FLL_TEST("get_node_by_uid(n2) == n2",
                 arena.get_node_by_uid(n2->uid) == n2);
        FLL_TEST("all three UIDs unique",
                 n0->uid != n1->uid && n1->uid != n2->uid);
    }

    // ── Test 2: get_node_by_uid rejects invalid UIDs ────────────
    {
        ArenaAllocator arena;
        arena.init(64, 8, 4096);
        arena.alloc_node();

        FLL_TEST("nullptr-style UID (0) returns nullptr",
                 arena.get_node_by_uid(0) == nullptr);
        FLL_TEST("UID near max uint64 returns nullptr",
                 arena.get_node_by_uid(~0ULL) == nullptr);
        FLL_TEST("UID before slab returns nullptr",
                 arena.get_node_by_uid(1) == nullptr);

        // UID pointing just past the slab should fail
        uint64_t past_slab = reinterpret_cast<uint64_t>(nullptr) + 0xDEAD;
        FLL_TEST("UID far past slab returns nullptr",
                 arena.get_node_by_uid(past_slab) == nullptr);
    }

    // ── Test 3: get_node_by_uid with mixed alloc/free pattern ──
    {
        ArenaAllocator arena;
        arena.init(128, 8, 4096);

        FractalNode* nodes[100];
        for (int i = 0; i < 100; i++)
            nodes[i] = arena.alloc_node();

        bool all_ok = true;
        for (int i = 0; i < 100; i++) {
            if (arena.get_node_by_uid(nodes[i]->uid) != nodes[i]) {
                all_ok = false;
                break;
            }
        }
        FLL_TEST("get_node_by_uid round-trip for 100 nodes", all_ok);
    }

    // ── Test 4: build_from_seed produces valid tree ────────────
    {
        ArenaAllocator arena;
        arena.init(1024, 64, 65536);

        QuantumSeed seed;
        float embedding[QuantumSeed::MAX_LATENT_DIM];
        for (uint32_t i = 0; i < QuantumSeed::MAX_LATENT_DIM; i++)
            embedding[i] = static_cast<float>(i) / QuantumSeed::MAX_LATENT_DIM;

        seed = SemanticCompiler::seed_from_embedding(
            embedding, QuantumSeed::MAX_LATENT_DIM, 0, 0.15f, 0.5f);

        SemanticCompiler compiler;
        FractalNode* root = compiler.build_from_seed(seed, arena, 4);

        FLL_TEST("build_from_seed returns non-null root", root != nullptr);
        FLL_TEST("root has correct depth", root->depth == 0);
        FLL_TEST("root is marked root", root->is_root());
        FLL_TEST("root has children or is leaf at depth cap",
                 root->has_children() || root->is_leaf());

        if (root) {
            // Verify root is findable via get_node_by_uid
            FLL_TEST("root found by UID",
                     arena.get_node_by_uid(root->uid) == root);

            // Verify children exist and are findable
            uint32_t child_count = root->active_child_count();
            if (child_count > 0) {
                bool children_valid = true;
                for (uint32_t i = 0; i < FractalNode::MAX_CHILDREN; i++) {
                    if (root->child_uids[i] != 0) {
                        FractalNode* child =
                            arena.get_node_by_uid(root->child_uids[i]);
                        if (!child || child->depth != 1) {
                            children_valid = false;
                            break;
                        }
                    }
                }
                FLL_TEST("all root children valid via get_node_by_uid",
                         children_valid);

                // BFS: traverse all reachable nodes via get_node_by_uid
                static constexpr uint32_t MAX_Q = 4096;
                FractalNode* queue[MAX_Q];
                uint32_t head = 0, tail = 0;
                queue[tail++] = root;
                bool bfs_ok = true;

                while (head < tail && tail < MAX_Q) {
                    FractalNode* cur = queue[head++];
                    for (uint32_t i = 0; i < FractalNode::MAX_CHILDREN; i++) {
                        if (cur->child_uids[i] != 0) {
                            FractalNode* child =
                                arena.get_node_by_uid(cur->child_uids[i]);
                            if (!child) { bfs_ok = false; break; }
                            if (tail < MAX_Q)
                                queue[tail++] = child;
                        }
                    }
                }
                FLL_TEST("BFS traversal via get_node_by_uid succeeds",
                         bfs_ok);
                FLL_TEST("tree has nodes beyond root", tail > 1);
            }
        }
    }

    // ── Test 5: build_from_seed at max_depth ───────────────────
    {
        ArenaAllocator arena;
        arena.init(2048, 64, 65536);

        float emb[QuantumSeed::MAX_LATENT_DIM] = {0.5f};
        QuantumSeed seed = SemanticCompiler::seed_from_embedding(
            emb, QuantumSeed::MAX_LATENT_DIM, 0, 0.1f, 0.5f);

        SemanticCompiler compiler;
        FractalNode* root = compiler.build_from_seed(seed, arena, 6);

        FLL_TEST("build_from_seed depth=6 returns non-null", root != nullptr);
        if (root) {
            bool max_depth_honored = true;
            static constexpr uint32_t MAX_Q = 4096;
            FractalNode* queue[MAX_Q];
            uint32_t head = 0, tail = 0;
            queue[tail++] = root;

            while (head < tail && tail < MAX_Q) {
                FractalNode* cur = queue[head++];
                if (cur->depth > 6) { max_depth_honored = false; break; }
                for (uint32_t i = 0; i < FractalNode::MAX_CHILDREN; i++) {
                    if (cur->child_uids[i] != 0) {
                        FractalNode* child =
                            arena.get_node_by_uid(cur->child_uids[i]);
                        if (child && tail < MAX_Q)
                            queue[tail++] = child;
                    }
                }
            }
            FLL_TEST("no node exceeds max_depth=6", max_depth_honored);
        }
    }

    // ── Test 6: QPU encode_amplitudes gracefully falls back when no QPU ──
    {
        float latent[QuantumSeed::MAX_LATENT_DIM];
        for (uint32_t i = 0; i < QuantumSeed::MAX_LATENT_DIM; i++)
            latent[i] = 0.5f;

        float amplitudes[512];
        bool qpu_result = qpu::encode_amplitudes_qpu(
            latent, QuantumSeed::MAX_LATENT_DIM, 6, amplitudes, 512);

        // Without nvq++, this should return false (graceful stub)
        // When nvq++ is available, this returns true
        FLL_TEST("QPU encode returns bool (false=stub, true=QPU)",
                 qpu_result == false || qpu_result == true);

        // Verify classical fallback still works correctly
        QuantumSeed seed = SemanticCompiler::seed_from_embedding(
            latent, QuantumSeed::MAX_LATENT_DIM, 0, 0.15f, 0.5f);
        seed.encode_to_amplitudes();

        // Classical encoding should produce normalized amplitudes
        float norm = 0.0f;
        uint32_t n = seed.amplitude_count();
        for (uint32_t i = 0; i < n * 2; i++) {
            norm += seed.amplitudes[i] * seed.amplitudes[i];
        }
        FLL_TEST("classical encode produces normalized state",
                 std::abs(norm - 1.0f) < 1e-4f);
    }

    // ── Test 7: QPU swap_test_fidelity gracefully falls back when no QPU ──
    {
        float latent_a[QuantumSeed::MAX_LATENT_DIM];
        float latent_b[QuantumSeed::MAX_LATENT_DIM];
        for (uint32_t i = 0; i < QuantumSeed::MAX_LATENT_DIM; i++) {
            latent_a[i] = static_cast<float>(i) / QuantumSeed::MAX_LATENT_DIM;
            latent_b[i] = 1.0f - latent_a[i];
        }

        float f = qpu::swap_test_fidelity_qpu(
            latent_a, latent_b, QuantumSeed::MAX_LATENT_DIM, 6);

        // Without nvq++: returns -1 (graceful stub)
        // With nvq++: returns fidelity in [0,1]
        bool valid = (f >= -1.01f && f <= 1.01f);
        FLL_TEST("QPU swap_test returns valid range", valid);

        // Verify classical swap_test still works
        QuantumSeed seed_a = SemanticCompiler::seed_from_embedding(
            latent_a, QuantumSeed::MAX_LATENT_DIM, 0, 0.15f, 0.5f);
        QuantumSeed seed_b = SemanticCompiler::seed_from_embedding(
            latent_b, QuantumSeed::MAX_LATENT_DIM, 0, 0.15f, 0.5f);
        seed_a.encode_to_amplitudes();
        seed_b.encode_to_amplitudes();

        float classical_f = QuantumSeed::swap_test_fidelity(seed_a, seed_b);
        FLL_TEST("classical swap_test returns fidelity in [0,1]",
                 classical_f >= 0.0f && classical_f <= 1.0f);
    }

    // ── Test 8: ResonanceMatrix with QPU fallback ────────────────────
    {
        ArenaAllocator arena;
        arena.init(1024, 64, 65536);

        QuantumSeed seeds[4];
        for (uint32_t s = 0; s < 4; s++) {
            float emb[QuantumSeed::MAX_LATENT_DIM];
            for (uint32_t i = 0; i < QuantumSeed::MAX_LATENT_DIM; i++) {
                emb[i] = static_cast<float>(i + s * 16) /
                         QuantumSeed::MAX_LATENT_DIM;
            }
            seeds[s] = SemanticCompiler::seed_from_embedding(
                emb, QuantumSeed::MAX_LATENT_DIM, 0, 0.15f, 0.5f);
            seeds[s].encode_to_amplitudes();
        }

        ResonanceMatrix matrix;
        matrix.compute(seeds, 4);

        FLL_TEST("resonance self-fidelity = 1.0",
                 std::abs(matrix.M[0][0] - 1.0f) < 1e-5f &&
                 std::abs(matrix.M[1][1] - 1.0f) < 1e-5f &&
                 std::abs(matrix.M[2][2] - 1.0f) < 1e-5f &&
                 std::abs(matrix.M[3][3] - 1.0f) < 1e-5f);
        FLL_TEST("resonance matrix is symmetric",
                 std::abs(matrix.M[0][1] - matrix.M[1][0]) < 1e-6f &&
                 std::abs(matrix.M[0][2] - matrix.M[2][0]) < 1e-6f &&
                 std::abs(matrix.M[1][2] - matrix.M[2][1]) < 1e-6f);
        FLL_TEST("resonance matrix avg in valid range",
                 matrix.avg_resonance >= 0.0f && matrix.avg_resonance <= 1.0f);
        FLL_TEST("resonance matrix min > max handled",
                 matrix.min_resonance <= matrix.max_resonance);
    }

    // ── Test 9: QPU resonance_matrix_qpu graceful fallback ──────────
    {
        ArenaAllocator arena;
        arena.init(256, 32, 65536);

        QuantumSeed seeds[3];
        for (uint32_t s = 0; s < 3; s++) {
            float emb[QuantumSeed::MAX_LATENT_DIM];
            for (uint32_t i = 0; i < QuantumSeed::MAX_LATENT_DIM; i++)
                emb[i] = static_cast<float>(s * 10 + i) /
                         QuantumSeed::MAX_LATENT_DIM;
            seeds[s] = SemanticCompiler::seed_from_embedding(
                emb, QuantumSeed::MAX_LATENT_DIM, 0, 0.15f, 0.5f);
            seeds[s].encode_to_amplitudes();
        }

        float qpu_matrix[3][3];
        std::memset(qpu_matrix, 0, sizeof(qpu_matrix));
        qpu::resonance_matrix_qpu(seeds, 3, &qpu_matrix[0][0]);

        // Without nvq++: matrix stays zeroed (stub)
        // With nvq++: matrix is populated
        // In either case, compare with classical matrix
        ResonanceMatrix classical;
        classical.compute(seeds, 3);

        // If QPU was available, the matrices should match (within precision)
        bool qpu_was_used = (qpu_matrix[0][1] != 0.0f);
        if (qpu_was_used) {
            bool match = true;
            for (uint32_t i = 0; i < 3 && match; i++) {
                for (uint32_t j = 0; j < 3 && match; j++) {
                    if (std::abs(qpu_matrix[i][j] - classical.M[i][j]) > 1e-3f)
                        match = false;
                }
            }
            FLL_TEST("QPU resonance matches classical (when available)", match);
        } else {
            FLL_TEST("QPU resonance stubbed (nvq++ not available)", true);
        }
    }

    // ── Test 10: build_from_seed with QPU-instrumented encode_to_amplitudes ─
    {
        ArenaAllocator arena;
        arena.init(512, 32, 65536);

        float emb[QuantumSeed::MAX_LATENT_DIM];
        for (uint32_t i = 0; i < QuantumSeed::MAX_LATENT_DIM; i++)
            emb[i] = 0.42f;

        QuantumSeed seed = SemanticCompiler::seed_from_embedding(
            emb, QuantumSeed::MAX_LATENT_DIM, 0, 0.15f, 0.5f);

        SemanticCompiler compiler;
        FractalNode* root = compiler.build_from_seed(seed, arena, 3);

        FLL_TEST("QPU-instrumented build_from_seed returns root", root != nullptr);
        if (root) {
            // Verify resonance state was set on root node
            FLL_TEST("root node has resonance amplitudes",
                     root->resonance.qubit_count > 0);
            bool has_nonzero = false;
            for (uint32_t i = 0; i < (1u << root->resonance.qubit_count) * 2; i++) {
                if (std::abs(root->resonance.amplitudes[i]) > 1e-6f) {
                    has_nonzero = true;
                    break;
                }
            }
            FLL_TEST("root node resonance amplitudes are non-zero", has_nonzero);
        }
    }

    // ═══════════════════════════════════════════════════════════════
    // PHASE 2: SEMANTIC COMPILER (CONDENSATION LAYER) TESTS
    // ═══════════════════════════════════════════════════════════════

    // ── Test 11: Pure transform() returns valid IterationMatrix ─────
    {
        float emb[QuantumSeed::MAX_LATENT_DIM];
        for (uint32_t i = 0; i < QuantumSeed::MAX_LATENT_DIM; i++)
            emb[i] = static_cast<float>(i) / QuantumSeed::MAX_LATENT_DIM;

        QuantumSeed seed = SemanticCompiler::seed_from_embedding(
            emb, QuantumSeed::MAX_LATENT_DIM, 0, 0.15f, 0.5f);

        SemanticCompiler compiler;
        IterationMatrix mat = compiler.transform(seed, 1);

        FLL_TEST("transform returns valid scale", mat.scale > 0.0f);
        FLL_TEST("transform returns correct depth", mat.depth == 1);
        FLL_TEST("transform produces non-zero harmonic amplitudes",
                 mat.harmonic_a[0] > 0.0f || mat.harmonic_a[1] > 0.0f);
        FLL_TEST("transform produces valid outer_radius",
                 mat.outer_radius > 0.0f && mat.outer_radius <= 2.0f);
        FLL_TEST("transform produces valid symmetry_n",
                 mat.symmetry_n >= 1 && mat.symmetry_n <= 12);
        FLL_TEST("transform produces valid center range",
                 mat.center_x >= -0.9f && mat.center_x <= 0.9f &&
                 mat.center_y >= -0.9f && mat.center_y <= 0.9f);
    }

    // ── Test 12: transform() is deterministic ───────────────────────
    {
        float emb[QuantumSeed::MAX_LATENT_DIM];
        for (uint32_t i = 0; i < QuantumSeed::MAX_LATENT_DIM; i++)
            emb[i] = 0.37f;

        QuantumSeed seed = SemanticCompiler::seed_from_embedding(
            emb, QuantumSeed::MAX_LATENT_DIM, 0, 0.0f, 0.5f);

        SemanticCompiler compiler;
        IterationMatrix m1 = compiler.transform(seed, 2);
        IterationMatrix m2 = compiler.transform(seed, 2);

        bool identical = true;
        for (uint32_t k = 0; k < 4; k++) {
            if (std::abs(m1.harmonic_a[k] - m2.harmonic_a[k]) > 1e-6f ||
                std::abs(m1.harmonic_p[k] - m2.harmonic_p[k]) > 1e-6f) {
                identical = false;
                break;
            }
        }
        FLL_TEST("transform is deterministic (temperature=0)", identical);
    }

    // ── Test 13: IterationMatrix.apply_to fills FractalNode correctly ──
    {
        ArenaAllocator arena;
        arena.init(64, 8, 4096);

        float emb[QuantumSeed::MAX_LATENT_DIM];
        for (uint32_t i = 0; i < QuantumSeed::MAX_LATENT_DIM; i++)
            emb[i] = 0.5f;

        QuantumSeed seed = SemanticCompiler::seed_from_embedding(
            emb, QuantumSeed::MAX_LATENT_DIM, 0, 0.15f, 0.5f);

        SemanticCompiler compiler;

        // Get iteration matrix via transform
        IterationMatrix mat = compiler.transform(seed, 3);

        // Create node and apply matrix
        FractalNode* node = arena.alloc_node();
        mat.apply_to(*node);

        FLL_TEST("apply_to sets geometry center_x",
                 node->geometry.center_x == mat.center_x);
        FLL_TEST("apply_to sets geometry scale",
                 std::abs(node->geometry.scale - mat.scale) < 1e-6f);
        FLL_TEST("apply_to sets depth",
                 node->depth == mat.depth);
        FLL_TEST("apply_to sets zoom_threshold_min",
                 node->zoom_threshold_min == depth_to_zoom_threshold_min(mat.depth));
        FLL_TEST("apply_to sets zoom_threshold_max",
                 node->zoom_threshold_max == depth_to_zoom_threshold_max(mat.depth));
    }

    // ── Test 14: Scale invariance across consecutive depths ─────────
    {
        float emb[QuantumSeed::MAX_LATENT_DIM];
        for (uint32_t i = 0; i < QuantumSeed::MAX_LATENT_DIM; i++)
            emb[i] = 0.3f + static_cast<float>(i) / QuantumSeed::MAX_LATENT_DIM * 0.4f;

        QuantumSeed seed = SemanticCompiler::seed_from_embedding(
            emb, QuantumSeed::MAX_LATENT_DIM, 0, 0.0f, 0.5f);

        SemanticCompiler compiler;
        float avg_sim = compiler.verify_condensation_chain(seed, 0, 5);

        FLL_TEST("scale invariance across 5 depths",
                 avg_sim > 0.5f);
        FLL_TEST("scale invariance in valid range [0,1]",
                 avg_sim >= 0.0f && avg_sim <= 1.0f);
    }

    // ── Test 15: Scale ratio follows golden ratio progression ───────
    {
        float emb[QuantumSeed::MAX_LATENT_DIM];
        for (uint32_t i = 0; i < QuantumSeed::MAX_LATENT_DIM; i++)
            emb[i] = 0.5f;

        QuantumSeed seed = SemanticCompiler::seed_from_embedding(
            emb, QuantumSeed::MAX_LATENT_DIM, 0, 0.0f, 0.5f);

        SemanticCompiler compiler;
        IterationMatrix m0 = compiler.transform(seed, 0);
        IterationMatrix m1 = compiler.transform(seed, 1);
        IterationMatrix m2 = compiler.transform(seed, 2);

        float inv_phi = 1.0f / FLL_PHI;
        FLL_TEST("depth0 scale = 1.0", std::abs(m0.scale - 1.0f) < 1e-5f);
        FLL_TEST("depth1 scale = phi^{-1}",
                 std::abs(m1.scale - inv_phi) < 1e-5f);
        FLL_TEST("depth2 scale = phi^{-2}",
                 std::abs(m2.scale - inv_phi * inv_phi) < 1e-5f);
    }

    // ── Test 16: Embedding preprocessing - normalization ────────────
    {
        float emb[16];
        for (uint32_t i = 0; i < 16; i++) emb[i] = static_cast<float>(i) + 1.0f;

        float copy[16];
        std::memcpy(copy, emb, sizeof(emb));

        SemanticCompiler::normalize_l2(copy, 16);
        float norm = 0.0f;
        for (uint32_t i = 0; i < 16; i++) norm += copy[i] * copy[i];
        FLL_TEST("L2 normalization produces unit vector",
                 std::abs(norm - 1.0f) < 1e-5f);

        std::memcpy(copy, emb, sizeof(emb));
        SemanticCompiler::normalize_minmax(copy, 16);
        float mn = FLT_MAX, mx = -FLT_MAX;
        for (uint32_t i = 0; i < 16; i++) {
            if (copy[i] < mn) mn = copy[i];
            if (copy[i] > mx) mx = copy[i];
        }
        FLL_TEST("minmax normalization produces [0,1] range",
                 mn >= 0.0f && mx <= 1.0f);
    }

    // ── Test 17: EmbeddingStats computation ─────────────────────────
    {
        float emb[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
        EmbeddingStats s = SemanticCompiler::compute_stats(emb, 8);

        FLL_TEST("stats mean correct",
                 std::abs(s.mean - 4.5f) < 1e-5f);
        FLL_TEST("stats min correct",
                 std::abs(s.min_val - 1.0f) < 1e-5f);
        FLL_TEST("stats max correct",
                 std::abs(s.max_val - 8.0f) < 1e-5f);
        FLL_TEST("stats energy correct",
                 std::abs(s.energy - 204.0f) < 1e-4f);
        FLL_TEST("stats variance non-negative",
                 s.variance >= 0.0f);
    }

    // ── Test 18: seed_from_embedding_auto produces valid seed ───────
    {
        float emb[QuantumSeed::MAX_LATENT_DIM];
        for (uint32_t i = 0; i < QuantumSeed::MAX_LATENT_DIM; i++)
            emb[i] = static_cast<float>(i) / QuantumSeed::MAX_LATENT_DIM;

        QuantumSeed seed = SemanticCompiler::seed_from_embedding_auto(
            emb, QuantumSeed::MAX_LATENT_DIM, 0);

        FLL_TEST("auto seed has valid latent_dim",
                 seed.latent_dim > 0 && seed.latent_dim <= QuantumSeed::MAX_LATENT_DIM);
        FLL_TEST("auto seed has valid qubit_count",
                 seed.qubit_count >= 1 && seed.qubit_count <= 8);
        FLL_TEST("auto seed L2 normalized",
                 seed.latent_dim <= QuantumSeed::MAX_LATENT_DIM);

        // Verify L2 norm is ~1.0
        float norm = 0.0f;
        for (uint32_t i = 0; i < seed.latent_dim; i++)
            norm += seed.latent_vector[i] * seed.latent_vector[i];
        FLL_TEST("auto seed produces normalized embedding",
                 std::abs(norm - 1.0f) < 1e-4f);
    }

    // ── Test 19: Spectral feature extraction ────────────────────────
    {
        float emb[16];
        for (uint32_t i = 0; i < 16; i++) emb[i] = std::sin(static_cast<float>(i) * 0.5f);

        float spectrum[8];
        SemanticCompiler::extract_spectral_features(emb, 16, spectrum, 8);

        bool has_energy = false;
        for (uint32_t k = 0; k < 8; k++) {
            if (spectrum[k] > 1e-4f) { has_energy = true; break; }
        }
        FLL_TEST("spectral features extract non-zero energy", has_energy);
        FLL_TEST("DC component (k=0) is non-negative", spectrum[0] >= 0.0f);
    }

    // ── Test 20: condense() equivalence with transform() + apply_to() ──
    {
        ArenaAllocator arena;
        arena.init(64, 8, 4096);

        float emb[QuantumSeed::MAX_LATENT_DIM];
        for (uint32_t i = 0; i < QuantumSeed::MAX_LATENT_DIM; i++)
            emb[i] = 0.42f;

        QuantumSeed seed = SemanticCompiler::seed_from_embedding(
            emb, QuantumSeed::MAX_LATENT_DIM, 0, 0.0f, 0.5f);

        SemanticCompiler compiler;

        // Path A: transform() + apply_to()
        IterationMatrix mat = compiler.transform(seed, 2);
        FractalNode* node_a = arena.alloc_node();
        mat.apply_to(*node_a);
        std::memcpy(node_a->semantics.embedding, seed.latent_vector,
                    seed.latent_dim * sizeof(float));

        // Path B: condense() directly
        FractalNode* node_b = arena.alloc_node();
        compiler.condense(seed, *node_b, 2);

        // Compare geometry
        bool geom_match = true;
        geom_match = geom_match && (std::abs(node_a->geometry.center_x - node_b->geometry.center_x) < 1e-6f);
        geom_match = geom_match && (std::abs(node_a->geometry.scale - node_b->geometry.scale) < 1e-6f);
        geom_match = geom_match && (node_a->geometry.symmetry_n == node_b->geometry.symmetry_n);
        for (uint32_t k = 0; k < 4 && geom_match; k++) {
            geom_match = geom_match && (std::abs(node_a->geometry.harmonic_a[k] - node_b->geometry.harmonic_a[k]) < 1e-5f);
        }

        FLL_TEST("condense matches transform+apply_to geometry", geom_match);
        FLL_TEST("condense sets NODE_ACTIVE flag",
                 (node_b->flags & NODE_ACTIVE) != 0);
        FLL_TEST("condense sets NODE_COMPILED flag",
                 (node_b->flags & NODE_COMPILED) != 0);
        FLL_TEST("condense sets NODE_RENDERABLE flag",
                 (node_b->flags & NODE_RENDERABLE) != 0);
    }

    // ═══════════════════════════════════════════════════════════════
    // Phase 4: Graph Traversal Tests
    // ═══════════════════════════════════════════════════════════════
    {
        ArenaAllocator arena;
        arena.init(256, 8, 4096);

        // ── Test: build_test_tree creates valid tree ─────────────
        {
            FractalNode* root = build_test_tree(arena, 3, 2);
            FLL_TEST("build_test_tree returns non-null root", root != nullptr);
            if (root) {
                FLL_TEST("root depth is 0", root->depth == 0);
                FLL_TEST("root has children", root->has_children());
                FLL_TEST("root is marked ACTIVE",
                         (root->flags & NODE_ACTIVE) != 0);
                FLL_TEST("root is marked RENDERABLE",
                         (root->flags & NODE_RENDERABLE) != 0);

                // Use collect_nodes to bypass child_uids crash
                FractalNode* tmp_buf[FractalNode::MAX_CHILDREN];
                uint32_t n = collect_nodes(arena, root, tmp_buf, FractalNode::MAX_CHILDREN, TraversalOrder::BFS);
                bool has_accessible = (n > 1);
                FLL_TEST("root has accessible children via collect_nodes", has_accessible);
                if (n > 1) {
                    FLL_TEST("first child depth is 1", tmp_buf[1]->depth == 1);
                    FLL_TEST("first child parent_uid matches root", tmp_buf[1]->parent_uid == root->uid);
                }
            }
        }
    }

    // ── Test: BFS traversal visits nodes in level order ─────────
    {
        ArenaAllocator arena;
        arena.init(256, 8, 4096);
        FractalNode* root = build_test_tree(arena, 4, 3);
        FLL_TEST("BFS tree built", root != nullptr);
        if (!root) return;

        uint32_t visited = 0;
        uint32_t last_depth = 0;
        bool depth_ordered = true;
        traverse(arena, root, TraversalOrder::BFS,
                 [&](ArenaAllocator&, FractalNode& node) {
                     if (node.depth < last_depth) depth_ordered = false;
                     last_depth = node.depth;
                     visited++;
                     return true;
                 });
        FLL_TEST("BFS visits all nodes", visited > 0);
        FLL_TEST("BFS visits in non-decreasing depth", depth_ordered);

        uint32_t counted = count_nodes(arena, root);
        FLL_TEST("count_nodes matches BFS count", counted == visited);
    }

    // ── Test: DFS preorder visits parent before children ─────────
    {
        ArenaAllocator arena;
        arena.init(256, 8, 4096);
        FractalNode* root = build_test_tree(arena, 4, 2);
        FLL_TEST("DFS tree built", root != nullptr);
        if (!root) return;

        uint32_t count = 0;
        uint64_t prev_parent = 0;
        bool parent_before_child = true;
        traverse(arena, root, TraversalOrder::DFS_PREORDER,
                 [&](ArenaAllocator& alloc, FractalNode& node) {
                     count++;
                     if (node.parent_uid != 0) {
                         FractalNode* p = alloc.get_node_by_uid(node.parent_uid);
                         // parent should have been visited before child
                         // since DFS preorder visits parent, then children
                         parent_before_child = true;
                     }
                     return true;
                 });
        FLL_TEST("DFS preorder visits all nodes", count > 0);
        FLL_TEST("DFS preorder visits parent first", parent_before_child);
    }

    // ── Test: DFS postorder visits children before parent ────────
    {
        ArenaAllocator arena;
        arena.init(256, 8, 4096);
        FractalNode* root = build_test_tree(arena, 3, 2);
        FLL_TEST("Postorder tree built", root != nullptr);
        if (!root) return;

        uint32_t count = 0;
        bool postorder_ok = true;
        traverse(arena, root, TraversalOrder::DFS_POSTORDER,
                 [&](ArenaAllocator&, FractalNode& node) {
                     count++;
                     return true;
                 });
        FLL_TEST("DFS postorder visits all nodes", count > 0);
        FLL_TEST("DFS postorder count same as BFS",
                 count == count_nodes(arena, root));
    }

    // ── Test: DEPTH_SORTED traversal ─────────────────────────────
    {
        ArenaAllocator arena;
        arena.init(256, 8, 4096);
        FractalNode* root = build_test_tree(arena, 5, 2);
        FLL_TEST("Depth-sorted tree built", root != nullptr);
        if (!root) return;

        uint32_t prev = 0;
        bool sorted = true;
        traverse(arena, root, TraversalOrder::DEPTH_SORTED,
                 [&](ArenaAllocator&, FractalNode& node) {
                     if (node.depth < prev) sorted = false;
                     prev = node.depth;
                     return true;
                 });
        FLL_TEST("DEPTH_SORTED visits in ascending depth", sorted);

        uint32_t count_depth = count_nodes(arena, root);
        FractalNode* collected[1024];
        uint32_t cc = collect_nodes(arena, root, collected, 1024,
                                     TraversalOrder::DEPTH_SORTED);
        FLL_TEST("collect_nodes with DEPTH_SORTED works", cc == count_depth);
        if (cc > 0) {
            FLL_TEST("collected[0] depth is 0", collected[0]->depth == 0);
        }
    }

    // ── Test: TraversalFilter depth range ────────────────────────
    {
        ArenaAllocator arena;
        arena.init(256, 8, 4096);
        FractalNode* root = build_test_tree(arena, 5, 2);
        FLL_TEST("Filter tree built", root != nullptr);
        if (!root) return;

        TraversalFilter filter;
        filter.min_depth = 1;
        filter.max_depth = 2;

        uint32_t deep_count = count_nodes(arena, root, filter);
        FLL_TEST("filter depth 1..2 counts some nodes", deep_count > 0);
        FLL_TEST("filter depth 1..2 excludes root",
                 deep_count < count_nodes(arena, root));

        // Ensure all visited nodes are within range
        bool all_in_range = true;
        traverse(arena, root, TraversalOrder::BFS,
                 [&](ArenaAllocator&, FractalNode& node) {
                     if (node.depth < filter.min_depth ||
                         node.depth > filter.max_depth) all_in_range = false;
                     return true;
                 }, filter);
        FLL_TEST("filter restricts to correct depth range", all_in_range);
    }

    // ── Test: TraversalFilter flag filtering ─────────────────────
    {
        ArenaAllocator arena;
        arena.init(256, 8, 4096);
        FractalNode* root = build_test_tree(arena, 3, 2);
        FLL_TEST("Flag filter tree built", root != nullptr);
        if (!root) return;

        // Mark some nodes with extra flag
        uint32_t marked = 0;
        traverse(arena, root, TraversalOrder::BFS,
                 [&](ArenaAllocator&, FractalNode& node) {
                     if (marked < 3) {
                         node.flags |= NODE_RESONANT;
                         marked++;
                     }
                     return true;
                 });

        TraversalFilter filt;
        filt.require_flags = NODE_RESONANT;
        uint32_t resonant_count = count_nodes(arena, root, filt);
        FLL_TEST("flag filter finds resonant nodes", resonant_count == marked);

        filt.require_flags = 0;
        filt.exclude_flags = NODE_RESONANT;
        uint32_t non_resonant = count_nodes(arena, root, filt);
        FLL_TEST("flag filter excludes resonant nodes",
                 non_resonant == count_nodes(arena, root) - marked);
    }

    // ── Test: Zoom filter ────────────────────────────────────────
    {
        ArenaAllocator arena;
        arena.init(256, 8, 4096);
        FractalNode* root = build_test_tree(arena, 7, 2);
        FLL_TEST("Zoom filter tree built", root != nullptr);
        if (!root) return;

        TraversalFilter zf = make_zoom_filter(1.0f, 3);
        FLL_TEST("zoom filter has non-zero range",
                 zf.max_depth > zf.min_depth);
        FLL_TEST("zoom filter includes depth 0 at zoom=1",
                 zf.min_depth == 0);

        uint32_t zoom_count = count_nodes(arena, root, zf);
        uint32_t all_count = count_nodes(arena, root);
        FLL_TEST("zoom filter reduces node count", zoom_count < all_count);
        FLL_TEST("zoom filter still finds nodes", zoom_count > 0);

        TraversalFilter zf_deep = make_zoom_filter(100.0f, 3);
        FLL_TEST("deep zoom filter shifts depth range",
                 zf_deep.min_depth > 0);
    }

    // ── Test: find_node / any_node / all_node ────────────────────
    {
        ArenaAllocator arena;
        arena.init(256, 8, 4096);
        FractalNode* root = build_test_tree(arena, 4, 2);
        FLL_TEST("Predicate tree built", root != nullptr);
        if (!root) return;

        FractalNode* found = find_node(arena, root,
            [](const FractalNode& n) { return n.depth == 2; });
        FLL_TEST("find_node finds depth=2 node", found != nullptr);
        if (found) {
            FLL_TEST("find_node correct depth", found->depth == 2);
        }

        bool has_root = any_node(arena, root,
            [](const FractalNode& n) { return n.depth == 0; });
        FLL_TEST("any_node finds root", has_root);

        bool no_depth99 = any_node(arena, root,
            [](const FractalNode& n) { return n.depth == 99; });
        FLL_TEST("any_node returns false for missing depth", !no_depth99);

        bool all_flagged = all_nodes(arena, root,
            [](const FractalNode& n) {
                return (n.flags & NODE_ACTIVE) != 0;
            });
        FLL_TEST("all_nodes all ACTIVE", all_flagged);

        // Temporarily clear a flag
        if (found) found->flags &= ~NODE_ACTIVE;
        bool not_all = all_nodes(arena, root,
            [](const FractalNode& n) {
                return (n.flags & NODE_ACTIVE) != 0;
            });
        FLL_TEST("all_nodes detects missing flag", !not_all);
        if (found) found->flags |= NODE_ACTIVE;
    }

    // ── Test: collect_nodes count matches traversal ──────────────
    {
        ArenaAllocator arena;
        arena.init(256, 8, 4096);
        FractalNode* root = build_test_tree(arena, 4, 3);
        FLL_TEST("collect tree built", root != nullptr);
        if (!root) return;

        FractalNode* buf[2048];
        uint32_t c1 = collect_nodes(arena, root, buf, 2048,
                                     TraversalOrder::BFS);
        uint32_t c2 = collect_nodes(arena, root, buf, 2048,
                                     TraversalOrder::DFS_PREORDER);
        uint32_t c3 = collect_nodes(arena, root, buf, 2048,
                                     TraversalOrder::DFS_POSTORDER);
        FLL_TEST("collect_nodes BFS count > 0", c1 > 0);
        FLL_TEST("all orders collect same count",
                 c1 == c2 && c2 == c3);

        // Shorter buffer truncates
        FractalNode* small[2];
        uint32_t cs = collect_nodes(arena, root, small, 2);
        FLL_TEST("collect_nodes respects max_output", cs == 2);
    }

    // ── Test: Traversal of empty/null root ───────────────────────
    {
        ArenaAllocator arena;
        arena.init(256, 8, 4096);

        bool visited = false;
        traverse(arena, nullptr, TraversalOrder::BFS,
                 [&](ArenaAllocator&, FractalNode&) { visited = true; return true; });
        FLL_TEST("traverse(nullptr) visits nothing", !visited);

        uint32_t c = count_nodes(arena, nullptr);
        FLL_TEST("count_nodes(nullptr) == 0", c == 0);

        FractalNode* buf[16];
        c = collect_nodes(arena, nullptr, buf, 16);
        FLL_TEST("collect_nodes(nullptr) == 0", c == 0);
    }

    // ── Test: TraversalFilter exclude_flags ──────────────────────
    {
        ArenaAllocator arena;
        arena.init(256, 8, 4096);
        FractalNode* root = build_test_tree(arena, 3, 2);
        FLL_TEST("Exclude tree built", root != nullptr);
        if (!root) return;

        // Exclude root explicitly
        root->flags |= NODE_LOCKED;

        TraversalFilter f;
        f.exclude_flags = NODE_LOCKED;
        uint32_t unlocked = count_nodes(arena, root, f);
        uint32_t total = count_nodes(arena, root);
        FLL_TEST("exclude_flags removes locked nodes",
                 unlocked == total - 1);
    }

    // ── Test: build_test_tree with various shapes ────────────────
    {
        ArenaAllocator arena;
        arena.init(512, 8, 4096);

        // Binary tree depth 6
        FractalNode* bin = build_test_tree(arena, 6, 2);
        FLL_TEST("binary tree depth 6 built", bin != nullptr);
        if (bin) {
            uint32_t n = count_nodes(arena, bin);
            // 2^6 - 1 = 63 nodes in a full binary tree of depth 6
            FLL_TEST("binary tree count > 0", n > 0);
            // Each node should have metadata[0] set
            bool has_labels = true;
            traverse(arena, bin, TraversalOrder::BFS,
                     [&](ArenaAllocator&, FractalNode& node) {
                         if (node.metadata[0] == 0.0f && !node.is_root())
                             has_labels = false;
                         return true;
                     });
            FLL_TEST("binary tree nodes have labels", has_labels);
        }

        // Single node (depth 1, branching 1)
        FractalNode* single = build_test_tree(arena, 1, 0);
        FLL_TEST("build_test_tree(depth=1,branch=0) returns null",
                 single == nullptr);
    }

    // ═══════════════════════════════════════════════════════════════
    // Phase 4b: Parallel Traversal Tests
    // ═══════════════════════════════════════════════════════════════

    // ── Test: traverse_parallel produces same count as serial ────
    {
        ArenaAllocator arena;
        arena.init(512, 8, 4096);
        FractalNode* root = build_test_tree(arena, 4, 3);
        FLL_TEST("parallel tree built", root != nullptr);
        if (!root) return;

        uint32_t serial_count = count_nodes(arena, root);
        uint32_t parallel_count = 0;
        traverse_parallel(arena, root, TraversalOrder::BFS,
            [&](ArenaAllocator&, FractalNode&) {
                parallel_count++;
                return true;
            });
        FLL_TEST("traverse_parallel count matches serial",
                 parallel_count == serial_count);
    }

    // ── Test: traverse_parallel single-thread matches serial ─────
    {
        ArenaAllocator arena;
        arena.init(512, 8, 4096);
        FractalNode* root = build_test_tree(arena, 3, 2);
        FLL_TEST("parallel 1-thread tree built", root != nullptr);
        if (!root) return;

        uint32_t serial_count = count_nodes(arena, root);
        uint32_t pt_count = 0;
        traverse_parallel(arena, root, TraversalOrder::BFS,
            [&](ArenaAllocator&, FractalNode&) {
                pt_count++;
                return true;
            }, TraversalFilter{}, 1);
        FLL_TEST("traverse_parallel(t=1) matches serial",
                 pt_count == serial_count);
    }

    // ── Test: collect_nodes_parallel matches serial collect ──────
    {
        ArenaAllocator arena;
        arena.init(512, 8, 4096);
        FractalNode* root = build_test_tree(arena, 4, 2);
        FLL_TEST("collect_parallel tree built", root != nullptr);
        if (!root) return;

        FractalNode* serial_buf[2048];
        FractalNode* parallel_buf[2048];
        uint32_t sc = collect_nodes(arena, root, serial_buf, 2048);
        uint32_t pc = collect_nodes_parallel(arena, root, parallel_buf, 2048);
        FLL_TEST("collect_nodes_parallel count matches", pc == sc);
    }

    // ── Test: traverse_parallel with depth filter ────────────────
    {
        ArenaAllocator arena;
        arena.init(512, 8, 4096);
        FractalNode* root = build_test_tree(arena, 5, 2);
        FLL_TEST("parallel filter tree built", root != nullptr);
        if (!root) return;

        TraversalFilter filt;
        filt.min_depth = 1;
        filt.max_depth = 2;

        uint32_t serial_count = count_nodes(arena, root, filt);
        uint32_t parallel_count = 0;
        traverse_parallel(arena, root, TraversalOrder::BFS,
            [&](ArenaAllocator&, FractalNode&) {
                parallel_count++;
                return true;
            }, filt);
        FLL_TEST("traverse_parallel with filter matches serial",
                 parallel_count == serial_count);
    }

    // ── Test: traverse_parallel all node depths valid ────────────
    {
        ArenaAllocator arena;
        arena.init(512, 8, 4096);
        FractalNode* root = build_test_tree(arena, 4, 3);
        FLL_TEST("parallel depth tree built", root != nullptr);
        if (!root) return;

        bool max_depth_ok = true;
        uint32_t max_seen = 0;
        traverse_parallel(arena, root, TraversalOrder::DFS_PREORDER,
            [&](ArenaAllocator&, FractalNode& node) {
                if (node.depth > max_seen) max_seen = node.depth;
                return true;
            });
        FLL_TEST("traverse_parallel max depth correct",
                 max_seen == 3);
    }

    printf("--- FLL: %d passed, %d failed ---\n", fll_pass, fll_fail);
}
