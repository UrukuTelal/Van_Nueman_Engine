# Issue #006: Constraint.h — Illegal char array assignment (compilation error)

**Severity:** High (compilation error on non-MSVC compilers)  
**Phase:** 2 (High)  
**File:** `include/Constraint.h:96`  
**Cross-reference:** `include/GodNode.h:256` (correct implementation)

## Description

`ConstraintState::record_event()` at line 96 performs a direct array assignment:

```cpp
events[event_count].event_type = type;  // line 96
```

`event_type` is declared as `char event_type[24]` (line 25). C++ does not allow array-to-array assignment — this is illegal in standard C++. The right-hand side `type` is `const char*`, making this an attempt to assign a pointer to an array lvalue, which is ill-formed.

## Evidence

```cpp
// Constraint.h:24-26
struct ConstraintEvent {
    char event_type[24];   // "accumulation", "recovery", "overflow", "full_constraint"
    ...
    // line 96:
    events[event_count].event_type = type;  // ILLEGAL: char[24] = const char*
};
```

## Why This Compiles on MSVC (Default)

MSVC in default `/Ze` mode allows non-standard array assignment as a Microsoft extension. However:
- MSVC `/permissive-` (standard conformance mode) **rejects** this
- GCC and Clang **always reject** this
- CI includes Linux GCC builds (`.github/workflows/ci.yml`) — this would fail there

## Correct Pattern (Existing in Same Codebase)

`GodNode.h:256` handles the identical pattern correctly:

```cpp
snprintf(events[event_count].event_type, sizeof(events[event_count].event_type), "%s", type);
```

## Additional Issues in Same File

### 1. Unbounded event_count in record_event parameter (line 94)

The method accepts `event_count` up to 32 (line 95 check), but `ConstraintEvent` struct stores events in arrays of different sizes:
- `AccumulationResult::events` has max 8 entries (line 50)
- `ConstraintState::events` has max 32 entries (line 68)

The `record_event` method on `ConstraintState` (line 94-105) correctly caps at 32. However, `absord_overflow` (line 140-158) writes directly into `AccumulationResult::events[8]` with **no bounds check** — if `result.event_count` exceeds 7, it will overflow the 8-element array.

### 2. strncpy not used for event_type in aggregate initialization

Lines 140-158 use C++ aggregate initialization with string literals for `event_type`:

```cpp
result.events[result.event_count++] = {
    "overflow", constraint_before, level,
    depth_before, 0.0f, remaining, -1
};
```

This works syntactically (aggregate init assigns chars, not array), but will truncate if a future string exceeds 23 chars. Current strings are short, so low risk.

## Impact

- **Build failure** on GCC/Clang (Linux CI, macOS)
- **Potential runtime truncation** in `accumulation` path (AggregateResult::events[8] unbounded writes)
- **Prevents cross-platform development**

## Recommended Fix

Replace line 96:

```cpp
// Before (line 96):
events[event_count].event_type = type;

// After:
snprintf(events[event_count].event_type, sizeof(events[event_count].event_type), "%s", type);
```

Also add bounds check in `absorb_overflow` for `result.event_count < 8` before writing to `result.events[]`.
