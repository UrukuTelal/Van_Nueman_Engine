#pragma once

#include "../audio/wht_packet.h"
#include "../include/Entity.h"
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace vn {
namespace quantum {

// ── WHT Token Vocabulary ──────────────────────────────────
// 64 canonical 32D WHT signal patterns that serve as a
// discrete token vocabulary, replacing text tokenization.
//
// Layout:
//   0-15:   Single-pillar (one pillar = 1.0)
//   16-31:  Harmonic (one harmonic band dominant)
//   32-47:  Dual-pillar (adjacent pillar pairs = 0.5 each)
//   48-57:  Composite (canonical PSV archetypes)
//   58-63:  Special (NULL, PAD, START, END, UNK, RESERVED)

struct WHTSignalVocabulary {
    static constexpr int VOCAB_SIZE = 64;
    float tokens[VOCAB_SIZE][WHT_N];
    bool initialized;

    WHTSignalVocabulary() : initialized(false) {
        std::memset(tokens, 0, sizeof(tokens));
        init();
    }

    void init() {
        if (initialized) return;
        float psv[NumPillars];

        // 0-15: Single-pillar tokens
        for (int p = 0; p < NumPillars; p++) {
            std::memset(psv, 0, sizeof(psv));
            psv[p] = 1.0f;
            encode_pillar_vector(psv, tokens[p]);
        }

        // 16-31: Harmonic tokens
        // Encode a base state then boost each harmonic band
        for (int h = 0; h < 16; h++) {
            std::memset(psv, 0, sizeof(psv));
            psv[0] = 0.5f;
            encode_pillar_vector(psv, tokens[16 + h]);
            // Emphasize the h-th harmonic element
            tokens[16 + h][16 + h] = 1.0f;
        }

        // 32-47: Dual-pillar tokens (adjacent pairs)
        for (int p = 0; p < NumPillars; p++) {
            std::memset(psv, 0, sizeof(psv));
            psv[p] = 0.5f;
            psv[(p + 1) % NumPillars] = 0.5f;
            encode_pillar_vector(psv, tokens[32 + p]);
        }

        // 48-57: Composite patterns (common PSV archetypes)
        // 48: Balanced (all 0.5)
        for (int i = 0; i < NumPillars; i++) psv[i] = 0.5f;
        encode_pillar_vector(psv, tokens[48]);

        // 49: High awareness, low harm
        std::memset(psv, 0, sizeof(psv));
        psv[Awareness] = 0.9f; psv[Harm] = 0.1f;
        encode_pillar_vector(psv, tokens[49]);

        // 50: High willpower, high force
        std::memset(psv, 0, sizeof(psv));
        psv[Willpower] = 0.9f; psv[Force] = 0.8f;
        encode_pillar_vector(psv, tokens[50]);

        // 51: High integrity, high cohesion
        std::memset(psv, 0, sizeof(psv));
        psv[Integrity] = 0.9f; psv[Cohesion] = 0.8f;
        encode_pillar_vector(psv, tokens[51]);

        // 52: High relation, high warmth
        std::memset(psv, 0, sizeof(psv));
        psv[Relation] = 0.9f; psv[Warmth] = 0.8f;
        encode_pillar_vector(psv, tokens[52]);

        // 53: High resistance, high presence
        std::memset(psv, 0, sizeof(psv));
        psv[Resistance] = 0.9f; psv[Presence] = 0.8f;
        encode_pillar_vector(psv, tokens[53]);

        // 54: High memory, high flux
        std::memset(psv, 0, sizeof(psv));
        psv[Memory] = 0.9f; psv[Flux] = 0.8f;
        encode_pillar_vector(psv, tokens[54]);

        // 55: High attraction, high depth
        std::memset(psv, 0, sizeof(psv));
        psv[Attraction] = 0.9f; psv[Depth] = 0.8f;
        encode_pillar_vector(psv, tokens[55]);

        // 56: Distortion dominant
        std::memset(psv, 0, sizeof(psv));
        psv[Distortion] = 0.9f;
        encode_pillar_vector(psv, tokens[56]);

        // 57: Influence dominant
        std::memset(psv, 0, sizeof(psv));
        psv[Influence] = 0.9f;
        encode_pillar_vector(psv, tokens[57]);

        // 58: NULL (all zeros — already zero-initialized)

        // 59: PAD (weak dual)
        std::memset(psv, 0, sizeof(psv));
        psv[0] = 0.5f; psv[1] = 0.5f;
        encode_pillar_vector(psv, tokens[59]);

        // 60: START (all ones)
        for (int i = 0; i < NumPillars; i++) psv[i] = 1.0f;
        encode_pillar_vector(psv, tokens[60]);

        // 61: END (all zeros — equal to NULL but semantically distinct)
        std::memset(psv, 0, sizeof(psv));
        encode_pillar_vector(psv, tokens[61]);

        // 62: UNK (uniform mid)
        for (int i = 0; i < NumPillars; i++) psv[i] = 0.5f;
        encode_pillar_vector(psv, tokens[62]);

        // 63: RESERVED (stays zeroed)

        initialized = true;
    }

    int size() const { return VOCAB_SIZE; }

    const float* token_signal(int id) const {
        if (id < 0 || id >= VOCAB_SIZE) return nullptr;
        return tokens[id];
    }

    const char* label(int id) const {
        static const char* labels[VOCAB_SIZE] = {
            "Awareness","Willpower","Force","Influence",
            "Resistance","Integrity","Cohesion","Relation",
            "Presence","Warmth","Memory","Attraction",
            "Harm","Distortion","Flux","Depth",
            "Harmonic_0","Harmonic_1","Harmonic_2","Harmonic_3",
            "Harmonic_4","Harmonic_5","Harmonic_6","Harmonic_7",
            "Harmonic_8","Harmonic_9","Harmonic_10","Harmonic_11",
            "Harmonic_12","Harmonic_13","Harmonic_14","Harmonic_15",
            "Dual_Awareness","Dual_Willpower","Dual_Force","Dual_Influence",
            "Dual_Resistance","Dual_Integrity","Dual_Cohesion","Dual_Relation",
            "Dual_Presence","Dual_Warmth","Dual_Memory","Dual_Attraction",
            "Dual_Harm","Dual_Distortion","Dual_Flux","Dual_Depth",
            "Balanced","HighAwareness","HighWillForce",
            "HighIntegrity","HighRelation","HighResistance",
            "HighMemory","HighAttraction","DistortionDominant",
            "InfluenceDominant",
            "NULL","PAD","START","END","UNK","RESERVED"
        };
        if (id < 0 || id >= VOCAB_SIZE) return "INVALID";
        return labels[id];
    }
};

// ── WHT Tokenizer ─────────────────────────────────────────
// Maps PSV ↔ token ID via nearest-neighbor search in WHT signal space.
// Replaces text tokenization with WHT-domain semantic matching.

struct WHTTokenizer {
    WHTSignalVocabulary vocab;

    WHTTokenizer() { vocab.init(); }

    // Encode PSV → closest token ID (cosine similarity on 32D WHT signal)
    int encode(const PillarStateVector& psv) const {
        float signal[WHT_N];
        encode_pillar_vector(psv.pillars, signal);

        int best_id = 0;
        float best_sim = -FLT_MAX;
        for (int i = 0; i < WHTSignalVocabulary::VOCAB_SIZE; i++) {
            float sim = cosine_similarity(signal, vocab.tokens[i]);
            if (sim > best_sim) { best_sim = sim; best_id = i; }
        }
        return best_id;
    }

    // Encode → top-K token IDs with scores
    void encode_top_k(const PillarStateVector& psv, int k,
                      int* out_ids, float* out_scores) const {
        float signal[WHT_N];
        encode_pillar_vector(psv.pillars, signal);

        int best_ids[WHTSignalVocabulary::VOCAB_SIZE];
        float best_scores[WHTSignalVocabulary::VOCAB_SIZE];
        for (int i = 0; i < WHTSignalVocabulary::VOCAB_SIZE; i++) {
            best_ids[i] = i;
            best_scores[i] = cosine_similarity(signal, vocab.tokens[i]);
        }
        // Bubble top K
        int limit = k < WHTSignalVocabulary::VOCAB_SIZE ? k : WHTSignalVocabulary::VOCAB_SIZE;
        for (int i = 0; i < limit; i++) {
            int best = i;
            for (int j = i + 1; j < WHTSignalVocabulary::VOCAB_SIZE; j++)
                if (best_scores[j] > best_scores[best]) best = j;
            int tmp_id = best_ids[i]; best_ids[i] = best_ids[best]; best_ids[best] = tmp_id;
            float tmp_s = best_scores[i]; best_scores[i] = best_scores[best]; best_scores[best] = tmp_s;
            out_ids[i] = best_ids[i];
            out_scores[i] = best_scores[i];
        }
    }

    // Decode token ID → PSV
    PillarStateVector decode(int token_id) const {
        const float* signal = vocab.token_signal(token_id);
        PillarStateVector result;
        if (!signal) { result.fill(0.5f); return result; }
        decode_pillar_vector(signal, result.pillars);
        for (int i = 0; i < NumPillars; i++) {
            if (result.pillars[i] < 0.0f) result.pillars[i] = 0.0f;
            if (result.pillars[i] > 1.0f) result.pillars[i] = 1.0f;
        }
        return result;
    }

    // Decode a sequence of token IDs → averaged PSV
    PillarStateVector decode_sequence(const int* token_ids, int count) const {
        PillarStateVector result;
        result.fill(0.0f);
        if (count <= 0) { result.fill(0.5f); return result; }
        for (int i = 0; i < count; i++) {
            PillarStateVector psv = decode(token_ids[i]);
            for (int p = 0; p < NumPillars; p++)
                result.pillars[p] += psv.pillars[p];
        }
        float inv = 1.0f / (float)count;
        for (int p = 0; p < NumPillars; p++)
            result.pillars[p] *= inv;
        return result;
    }

    static float cosine_similarity(const float a[WHT_N], const float b[WHT_N]) {
        float dot = 0.0f, na = 0.0f, nb = 0.0f;
        for (int i = 0; i < WHT_N; i++) {
            dot += a[i] * b[i]; na += a[i] * a[i]; nb += b[i] * b[i];
        }
        float denom = std::sqrt(na) * std::sqrt(nb);
        return (denom > 1e-12f) ? (dot / denom) : 0.0f;
    }

    static float pillar_distance(const PillarStateVector& a, const PillarStateVector& b) {
        float dist = 0.0f;
        for (int i = 0; i < NumPillars; i++) {
            float d = a.pillars[i] - b.pillars[i];
            dist += d * d;
        }
        return std::sqrt(dist);
    }
};

}} // namespace vn::quantum
