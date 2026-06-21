#pragma once

#include "thought_engine.h"

namespace vn {
namespace quantum {

// ── NativeCognitivePipeline ──────────────────────────────────
// Drop-in replacement for LLMBridge::query().
// Uses the ThoughtEngine (Hopf-PID + WHT Tokenizer) for agent
// reasoning instead of sending HTTP requests to Ollama.
//
// The creativity parameter maps to LLM "temperature": higher
// values produce more divergent cognitive patterns via the
// 480D Hopf fiber space. Coherence controls how quickly fibers
// converge back toward the base membrane.

struct NativeCognitivePipeline {
    ThoughtEngine engine;
    float creativity;
    float coherence;

    NativeCognitivePipeline(float creativity = 0.2f, float coherence = 0.8f)
        : creativity(creativity), coherence(coherence) {
        engine.creativity = creativity;
        engine.coherence = coherence;
    }

    // Run N ticks of cognitive processing on the input PSV.
    // Returns the updated PSV after Hopf-PID cycles through
    // the 512D thought space.
    PillarStateVector reason(
        const PillarStateVector& input,
        int ticks = 3,
        float dt = 0.016f,
        bool tokenized = false
    ) {
        engine.creativity = creativity;
        engine.coherence = coherence;
        return engine.think(input, ticks, dt, tokenized);
    }

    // Single-step alias for convenience
    PillarStateVector process(const PillarStateVector& input) {
        return reason(input, 3, 0.016f, false);
    }
};

}} // namespace vn::quantum
