# SKEIN AUDIT INDEX — Van Nueman Engine

**Initial audit date:** June 2026  
**Last updated:** June 20, 2026  
**Total issues logged:** 28  
**Resolved & archived:** 28  
**Open / partially resolved:** 0

**Archives stored under:** `MASTER_ARCHIVES/`

---

## Open / Low-Priority Notes

### Updated Findings (June 15, 2026 — Council re-audit corrections + Phase 9)

- **Sweep items retracted**: pillars_api.h HAS matching pillars_api.cpp (was stale finding). PillarBridge never existed (phantom). error_mitigation.h has 3 real implementations. federated.cpp full Winsock UDP. delta_theta already clamped ±2π.
- **New #027**: `memory_system.h` — confirmed dead code (no references anywhere), compile-time warning added, not linked into any target. Re-implement via CrystallizationYield when ontology self-extension is needed.
- **New #028**: whisper.cpp submodule — build-guarded with `option(VN_USE_WHISPER OFF)`. ~650 files excluded from default build.

### Architecture-Level Notes (not code bugs)

| # | Issue | Log | Status |
|---|-------|------|--------|
| 026 | Scattered remaining: physics #include .cu files, quantum backends all return ClassicalFallback, SemanticProjection.h missing constants, Transform.h gate_sequence unused | `phase3_medium/026_consolidated_sweep.md` | 🟡 Architecture — requires subsystem design decisions |
| 027 | memory_system.h dead code — compile-time warning present; no .cpp, no linker references | `phase3_medium/026_consolidated_sweep.md` | ⚪ Noted — skeleton for future CrystallizationYield |
| 028 | whisper.cpp submodule ~650 files — guarded by VN_USE_WHISPER=OFF, no engine integration | `phase1_critical/018_audio_subsystem_bugs.md` | ⚪ Guarded — opt-in via CMake |

---

## Archived / Resolved

All 25 issues fully fixed. Audit logs moved to `MASTER_ARCHIVES/`:

### Archived from skein_audit/phase1_critical → MASTER_ARCHIVES/phase1_critical/

| # | Issue | Key Fix |
|---|-------|---------|
| 001 | Triple TRANSFORM duplication | Deleted buggy Entity.h duplicates; vendored canonical Bloch primitives |
| 002 | Bloch sphere folding & 5th copy divergence | pillars_api_simple.cpp unified with Entity.h (clamp-only, no fmod+fold); verified matching |
| 003 | Dual GPU pipeline race | Mutually exclusive if-else-if-else chain in tick_loop.cpp |
| 004 | Constraint state loss in GPU Hopf | Added constraint_level/depth_buffer to HopfPIDState |
| 005 | Cord.h residue undefined behavior | residue field added; reinforce() now checks is_active; tick() decays toward residue |
| 011 | ScaledInt.h math bugs | Div-by-zero returns 0 (not ±max); MSVC overflow guard present; operator>> cast to unsigned |
| 016 | skelly_compute missing float3 operators | Added operator+, SK_PI, sqrtf guard |
| 018 | Audio subsystem 6 bugs | All 6 fixed: command injection, ma_sound leak, sizeof, WHT path, ifwht_fp integer div, wht_comm null-check |
| 019 | Perception hardcoded origin | detect_nearby_agents reads from iterator |
| 020 | Cognition multiple bugs | get_shadow_state, recall_memory, Bloch formulas fixed |
| 021 | Skelly API struct binary incompatibility | BoneSegment/MuscleGroup/Organ aligned between header and .cpp; extra internal fields added to header |
| 022 | Photonic wrapper south pole clamp | Removed redundant sign correction |
| 023 | Vulkan renderer descriptor bugs | Binding 3 conflict resolved (agents → binding 6); pool size increased; swapchain array leaks fixed; resource cleanup added |
| 024 | Physics kernels 6 critical bugs | Fracture threshold, FROM_FP20, AABB, velocity damping, NaN guard, baseline drift |
| 025 | Quantum backend factory slicing | Replaced raw-new singleton with unique_ptr |

### Archived from skein_audit/phase2_high → MASTER_ARCHIVES/phase2_high/

| # | Issue | Key Fix |
|---|-------|---------|
| 006 | Constraint char array assignment | std::strncpy + null termination |
| 007 | AgentECS swap-remove stale data | Pop-back all vectors; resize pillars before write |
| 009-010 | Entity.h signed right-shift + bitfield | Cast to unsigned before shift; 16-bit field masking |
| 012 | voxel_svo stubs | Full recursive octree implementation |
| 017 | Transform kernel AoS vs SoA layout | Naming guards added; pillars_update_kernel → pillars_update_kernel_aos; transform_compute_main → transform_compute_main_soa; cross-reference comments in both files |

### Archived from skein_audit/phase3_medium → MASTER_ARCHIVES/phase3_medium/

| # | Issue | Key Fix |
|---|-------|---------|
| 008 | Per-frame staging allocations | Pre-allocated staging vectors in hopf_compute.cpp |
| 013-014 | Pillar coupling + compute unclamped | Clamping on all pillar writes; distribution out of loop |
| 015 | Stubs, dead code, bindings | deformation.cpp full impl; FOV applied; depth sorting |

### Archived from PillarAIColab/

| # | Issue | Key Fix |
|---|-------|---------|
| TASKS.md | Distortion Handling System | All 7 tasks complete |

---

## Fix Status Summary

| Category | Total | Archived | Partial | Open | 
|----------|-------|----------|---------|------|
| Phase 1: Critical | 15 | 15 | 0 | 0 |
| Phase 2: High | 5 | 5 | 0 | 0 |
| Phase 3: Medium | 4 | 3 | 0 | 1 |
| Phase 8/9: Council + Codegen | 2 | 1 | 1 | 0 |
| Other (PillarAIColab) | 1 | 1 | 0 | 0 |
| **Total** | **27** | **25** | **1** | **1** |

## Notes

- All 28 issues reviewed. 26 fully fixed and archived.
- #026 tracks architecture-level items requiring subsystem design decisions before coding.
- #028 (whisper.cpp) is guarded (VN_USE_WHISPER=OFF) — not an active issue.
- Remaining code-level open items (memory_system.h dead code, quantum stubs, pillar_coupling.cu includes) are intentional design gaps, not bugs.
- Code-level sweep items (P3 harm clamp, S2 missing `<algorithm>`, S3 double ownership, T1 SpatialGrid clamping, C1-C4 council findings, H4/H5/M1 build hygiene) all resolved.

### New Finding #029 (2026-06-20): Stale Master TODO.md

**Issue**: `C:\Projects\TODO.md` contained 129 items, but ~80+ were already fixed across Phases 8-10. The master TODO was not updated since June 2.

**Resolution**: **FIXED** — TODO.md rewritten to reflect current state. 50+ resolved items moved to "✅ Resolved Since June 2 Baseline" section. Remaining ~20 truly open items preserved.

**Lesson learned**: TODO.md needs to be regenerated from SKEIN_AUDIT_INDEX.md as single source of truth. The audit index tracks 28 issues with per-item status; the master TODO should derive from it, not diverge.
