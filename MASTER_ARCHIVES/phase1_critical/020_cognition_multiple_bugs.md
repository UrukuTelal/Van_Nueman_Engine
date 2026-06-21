# SKEIN AUDIT: Issue #020 — Cognition Subsystem: 3 Critical Bugs

## Severity
**Critical** — Dream shadow state corrupted, memory access UB, Bloch formula wrong

## Category
Logic Errors — Wrong value returned, UB on empty container, incorrect rotation math

---

## Summary

Three critical bugs in `agents/cognition.h` and `agents/cognition.cpp`:

1. **`get_shadow_state()`** returns waking-state pillars instead of dream shadow patterns.
2. **`recall_memory()`** calls `front()` on empty vector — undefined behavior.
3. **Bloch distortion** formula in two locations is mathematically incorrect, producing wrong Awareness values.

---

## Bug 1: `get_shadow_state()` Returns Waking State (Wrong Data)

**File:** `agents/cognition.h:132`

```cpp
const PillarVector& get_shadow_state() const { return current_state_.pillars; }
```

**Problem:** The method name implies it returns the dream shadow state (`dream_state_.shadow_patterns`), but it returns `current_state_.pillars` — the agent's **waking** pillar state. This means any code that calls `get_shadow_state()` to read dream shadow patterns (e.g., during dream consolidation, shadow integration, or wake-up processing) gets the wrong data.

The correct implementation should be:
```cpp
const PillarVector& get_shadow_state() const { return dream_state_.shadow_patterns; }
```

This is most likely a copy-paste error from `get_pillars()` (line 125).

---

## Bug 2: `recall_memory()` Calls `front()` on Empty Vector (UB)

**File:** `agents/cognition.cpp:242-246`

```cpp
CognitiveState AgentCognition::recall_memory(uint32_t ticks_ago) const {
    if (ticks_ago >= memory_.size()) {
        return memory_.front();
    }
    return memory_[memory_.size() - 1 - ticks_ago];
}
```

**Problem:** When `memory_` is empty (`memory_.size() == 0`), the condition `ticks_ago >= 0` is **always** true (since `ticks_ago` is `uint32_t` and unsigned >= 0 is always true). The function enters the `if` branch and calls `memory_.front()` on an **empty vector**, which is undefined behavior (typically crashes or returns garbage).

If `memory_` has 1 element and `ticks_ago == 1`, then `1 >= 1` is true, and `memory_.front()` returns the only element. This is actually correct behavior (the oldest available memory). But the crash occurs when `memory_` is empty and `ticks_ago` is any value.

---

## Bug 3: Bloch Distortion Formula Incorrect (Two Locations)

The Bloch sphere distortion formula used in two places is mathematically wrong, producing incorrect Awareness values.

### Location A: `perceive_environment` path — `cognition.cpp:141-143`

```cpp
float phi_twist = distortion * PI;
float z_projection = std::cos(theta) * std::cos(phi_twist);
current_state_.pillars[PILLAR_AWARENESS] = vn::fp20_t((z_projection + 1.0f) * 0.5f);
```

### Location B: `apply_bloch_distortion_torsion` — `cognition.cpp:356-359`

```cpp
float phi_twist = distortion * 3.14159265f;
float z_projection = std::cos(theta) * std::cos(phi_twist);
current_state_.pillars[PILLAR_AWARENESS] = vn::fp20_t((z_projection + 1.0f) * 0.5f);
```

**Problem:** The comment says "twist the phi (phase) angle" — a phase rotation around the Z-axis. But a Z-axis rotation **does not change the z-coordinate** of a Bloch sphere vector:
- z' = z = cos(theta)

The formula `z_projection = cos(theta) * cos(phi_twist)` would be correct for a **Y-axis rotation** ONLY if the x-component is zero:
- R_y(φ): z' = z*cos(φ) - x*sin(φ)

Since the state may have a non-zero x-component (`x = sin(theta)`), the correct formula is:
```
z' = cos(theta)*cos(phi_twist) - sin(theta)*sin(phi_twist)
   = cos(theta + phi_twist)
```

Which is exactly what `apply_bloch_rotation(value, phi_twist)` would compute. The current formula always produces `|z'| <= |cos(theta)|`, making Awareness move toward 0.5 regardless of the actual state.

---

## Failure Scenario

1. **Shadow state bug:** Any code integrating dream shadows into waking state reads waking pillars instead. Dreams have no effect on the agent's personality.

2. **Empty memory crash:** An agent that has never stored a memory (e.g., newly spawned, or memory cleared) crashes when anything calls `recall_memory()` — including `calculate_bloch_angular_shift()` which checks `memory_.empty()` first but other callers may not.

3. **Wrong Awareness:** The Bloch distortion formula underestimates Awareness changes, making the "phase twist" effect qualitatively wrong. High-distortion agents end up with Awareness values that don't match the intended physics.

---

## Recommended Fix

1. **Bug 1:** Change `cognition.h:132` to return `dream_state_.shadow_patterns`.
2. **Bug 2:** Add an early-return of a default CognitiveState when `memory_` is empty.
3. **Bug 3:** Replace both instances with the correct Bloch rotation formula or use the existing `apply_bloch_rotation` helper.

## Confidence
**100%** for bugs 1-2 (verifiable by reading source). **95%** for bug 3 (verified by Bloch sphere math; 5% uncertainty if the "phase twist" was intended to be a different non-standard operation, though the comment says it's a phase/phi twist).
