# SKEIN AUDIT: Issue #005 — Cord.h `residue` Undefined Behavior

## Severity
**Critical** — Build / Undefined Behavior

## Category
Build — Compilation Error / Runtime UB

---

## Summary

`Cord::print()` references `residue` as a `printf` argument, but the `Cord` struct has **no member named `residue`**. This is either a compilation error (if compiled with certain warning levels or in C++20 mode which disallows implicit member access) or silent undefined behavior (reading garbage from the stack).

---

## Evidence

**File:** `include/Cord.h:27-102`

The `Cord` struct declares these members:
```cpp
struct Cord {
    uint32_t entity_a;
    uint32_t entity_b;
    float strength;
    float decay_rate;
    float age;
    float last_reinforced_at;
    bool is_active;
    // ⚠ NO `residue` field
};
```

But `print()` at line 97-101:
```cpp
void print() const {
    printf("CORD %u<->%u: strength=%.3f residue=%.3f age=%.1f %s\n",
           entity_a, entity_b, strength, residue, age,
           is_alive() ? "ALIVE" : "RESIDUAL");
    //                           ^^^^^^^
    //                           This field does NOT exist!
}
```

The `printf` format expects `double` for the `%.3f` of `residue` (floats are promoted to double in varargs). Since `residue` is not a member, this code:
1. If the compiler allows it: reads garbage from the C++ object representation (the `bool is_active` plus padding bytes will be misinterpreted as a float)
2. If the compiler doesn't allow it: compilation error

### Root Cause Context

The file header describes the cord's decay behavior:
```
// Cords are bidirectional iT links between entities that enable
// tau/phi synchronization. They decay exponentially toward a
// permanent residue. Re-entanglement is cheaper than initial bonding.
```

The `residue` field was described in the design but **never added to the struct**. The `print()` function was written as if it existed. The `strength` field decays toward `CORD_MIN_STRENGTH` as a makeshift residue, but the actual `residue` member is missing.

### Other Cord.h Issues

While investigating, three additional issues were found in the same file:

1. **Line 37**: `entity_a = (a < b) ? a : b;` — `init()` forces a canonical ordering. But `find()` at line 124 also canonicalizes. If any code path creates cords without canonicalization, `find()` will miss them. This is a fragile invariant.

2. **Line 166**: `affect_entity` is marked `const` (after the previous fix at line 165 commented "FIXED: Made const") but this is technically correct now since it only modifies the `psv` reference parameter.

3. **Line 58**: `reinforce(float amount)` adds to `strength` without checking if the cord is active. Dead cords can be reinforced back to life, bypassing the `sever()` mechanism.

---

## Failure Scenario

1. **Compilation failure:** A developer enables `/W4` or `-Wall -Werror`, or upgrades to a stricter compiler. The compilation fails on `residue` being undeclared. The `print()` function is only used in debug/logging paths, so this may go unnoticed until someone tries to debug cord behavior.

2. **Silent corruption:** If the compiler allows it (MSVC with default settings typically would), `residue` reads garbage that is printed as a floating-point number. The debug output is misleading, causing developers to believe cords have a "residue" that is actually noise.

3. **The `%s` format specifier reads `is_alive() ? "ALIVE" : "RESIDUAL"`** — which is correct, BUT if someone rearranges the arguments, this could also be a crash vector.

---

## Recommended Fix

### Fix 1: Add the `residue` field

```cpp
struct Cord {
    uint32_t entity_a;
    uint32_t entity_b;
    float strength;
    float decay_rate;
    float age;
    float last_reinforced_at;
    float residue;           // ← ADD: permanent residue (residual thread)
    bool is_active;
    
    void init(uint32_t a, uint32_t b, float initial_strength = 0.3f) {
        // ... existing init ...
        residue = CORD_MIN_STRENGTH;  // initialize residue
    }
};
```

### Fix 2: Update decay logic to settle at residue

Currently `tick()` decays `strength` toward `CORD_MIN_STRENGTH`. The proper behavior should be decay toward `residue`:
```cpp
void tick(float delta_time = 1.0f) {
    if (!is_alive()) return;
    age += delta_time;
    float target = residue > CORD_MIN_STRENGTH ? residue : CORD_MIN_STRENGTH;
    float decay = (strength - target) * (1.0f - std::exp(-decay_rate * delta_time));
    strength -= decay;
    if (strength < target) strength = target;
}
```

### Fix 3: Prevent dead cord reinforcement

```cpp
void reinforce(float amount = CORD_REINFORCE_BOOST) {
    if (!is_active) return;  // dead cords stay dead
    strength += amount;
    if (strength > 1.0f) strength = 1.0f;
    last_reinforced_at = age;
}
```

---

## Confidence

**100%** — The missing member is trivially verifiable. The code references `residue` and no such member exists in the struct declaration at lines 27-36.
