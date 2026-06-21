#pragma once

// ── Fractal Logoramical Language (FLL) Engine ───────────────────────
// Unified include for the complete FLL compiler and runtime environment.
//
// The FLL Engine abandons linear syntax strings in favor of
// scale-invariant, multi-dimensional, geometric data structures stored
// directly in contiguous GPU memory pools.
//
// Components:
//   FractalNode.h         — 64-byte aligned multi-scale semantic nodes
//   QuantumSeed.h          — Quantum semantic tokens with Swap Test
//   ArenaAllocator.h       — GPU-backed UVM arena allocator
//   SemanticCompiler.h     — Embedding → geometric coefficient mapping
//   FLLShaders.h           — Vulkan push constants and descriptor layout
//   FLLRenderer.h          — Vulkan rendering pipeline for glyphs
//
// Quick Start:
//   vn::fll::ArenaAllocator arena;
//   arena.init();
//
//   vn::fll::SemanticCompiler compiler;
//   vn::fll::QuantumSeed seed =
//       compiler.seed_from_embedding(my_embedding, 64);
//   seed.encode_to_amplitudes();
//
//   vn::fll::FractalNode* root =
//       compiler.build_from_seed(seed, arena, 8);

#include "FractalNode.h"
#include "QuantumSeed.h"
#include "ArenaAllocator.h"
#include "SemanticCompiler.h"
#include "FLLQPU.h"
#include "FLLShaders.h"
#include "FLLRenderer.h"
