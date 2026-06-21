# SKEIN AUDIT: Issue #019 — Agent Perception Hardcoded to World Origin

## Severity
**Critical** — Multi-agent perception is completely broken

## Category
Logic Error — Wrong variable used in distance computation

---

## Summary

`detect_nearby_agents()` at `agents/perception.cpp:89-104` computes all inter-agent distances from the fixed point (0,0,0) instead of the observer agent's position. This means agents can only detect other agents if both are at the world origin.

---

## Evidence

### Bug 1: Hardcoded (0,0,0) agent position (line 97)

**File:** `agents/perception.cpp:97`

```cpp
float ax = 0.0f, ay = 0.0f, az = 0.0f;
```

The function receives the observer's position as `(x, y, z)` but never assigns it to `(ax, ay, az)`. The coordinates remain `(0,0,0)` for ALL agents. The distance is then computed from `(x, y, z)` to `(0,0,0)` at line 99:

```cpp
float dist = distance_3d(x, y, z, ax, ay, az);
```

The `(ax, ay, az)` variables were **intended** to hold each agent's position (obtained by dereferencing the iterator `it`), but the code never reads from `it`. The variables should be set from agent data, e.g.:

```cpp
// Hypothetical fix:
float ax = it->x, ay = it->y, az = it->z;
```

### Bug 2: `world_to_chunk` may compute wrong voxel coordinates for negative world positions (lines 225-227)

**File:** `agents/perception.cpp:225-227`

```cpp
voxel_x = static_cast<int>(x) - chunk_x * CHUNK_SIZE;
voxel_y = static_cast<int>(y) - chunk_y * CHUNK_SIZE;
voxel_z = static_cast<int>(z) - chunk_z * CHUNK_SIZE;
```

`static_cast<int>(x)` truncates toward zero, while `std::floor(x / CHUNK_SIZE)` floors toward negative infinity. For negative `x`:
- If `x = -1.0`, `chunk_x = floor(-1/16) = -1`, `voxel_x = int(-1) - (-1)*16 = -1 + 16 = 15`. This happens to be correct because of the corrective code at lines 229-234.
- If `x = -16.0`, `chunk_x = floor(-16/16) = -1`, `voxel_x = int(-16) - (-1)*16 = -16 + 16 = 0`. This is correct.
- But for `x = -16.5`, `chunk_x = floor(-16.5/16) = -2`, `voxel_x = int(-16.5) - (-2)*16 = -16 + 32 = 16`. Then line 233 corrects: `voxel_x >= 16` → `chunk_x = -1, voxel_x = 0`.

The corrective code at lines 229-234 papers over the truncation-vs-floor mismatch, but it's fragile and wastes cycles on every call.

---

## Failure Scenario

An agent at position (100, 0, 0) calls `detect_nearby_agents(100, 0, 0, 50.0, ...)`. It expects to detect agents within 50 units. Instead, it computes `distance_3d(100, 0, 0, 0, 0, 0)` = 100 for EVERY agent. Since 100 > 50, no agents are ever detected unless the observer is near the origin **and** the other agents are also near the origin.

In a typical gameplay scenario where agents spawn at distributed positions, **no agent ever perceives any other agent**, breaking all multi-agent interaction: flocking, combat, communication, resource sharing.

---

## Root Cause

The `detect_nearby_agents` template was written with placeholder variables `(ax, ay, az)` initialized to zero, and the code to populate them from the iterator was never implemented. This is a cut-and-paste or stub-completion error.

---

## Recommended Fix

1. Replace line 97 with code that reads each agent's position from the iterator.
2. Replace `static_cast<int>(x)` with `std::floor(x)` in `world_to_chunk` for consistency.

## Confidence
**100%** — Verified by reading the source code. The zero-initialization is unambiguous.
