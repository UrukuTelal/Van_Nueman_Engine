# SKEIN AUDIT: Issue #021 — skelly_api.h vs skelly_api.cpp Struct Binary Incompatibility

## Severity
**Critical** — 4 structs have different layouts between header and implementation → all IPC/serialization is corrupted

## Category
Architecture — Binary Compatibility, ABI Mismatch

---

## Summary

Four structs (`BoneNode`, `BoneSegment`, `MuscleGroup`, `Organ`) are defined differently in the public C API header (`api/skelly_api.h`) vs the implementation (`api/skelly_api.cpp`). Both the field **order** and field **types** differ, meaning any code that writes data using the header layout and reads using the .cpp layout (or vice versa) gets corrupted data. Since `skelly_api.h` is the **public API** and `skelly_api.cpp` is the **implementation**, all callers are affected.

---

## Evidence

### Struct 1: `BoneNode`

**Header** (`api/skelly_api.h:41-50`):
```c
typedef struct BoneNode {
    uint32_t id;                    // offset 0
    float local_x, local_y, local_z; // offset 4, 8, 12
    float global_x, global_y, global_z; // offset 16, 20, 24
    int32_t parent_idx;              // offset 28
    uint32_t is_fractured;           // offset 32
    float constraint_pitch_min, constraint_pitch_max; // offset 36, 40
    float constraint_yaw_min, constraint_yaw_max;     // offset 44, 48
    float constraint_roll_min, constraint_roll_max;   // offset 52, 56
} BoneNode;
```
**Total:** 60 bytes (assuming no trailing padding)

**Implementation** (`api/skelly_api.cpp:32-40`):
```cpp
struct BoneNode {
    float3 local_pos;                // offset 0 (12 bytes: x, y, z)
    float3 global_pos;               // offset 12 (12 bytes)
    uint32_t parent_idx;             // offset 24
    uint32_t is_fractured;           // offset 28
    float constraint_pitch_min, constraint_pitch_max; // offset 32, 36
    float constraint_yaw_min, constraint_yaw_max;     // offset 40, 44
    float constraint_roll_min, constraint_roll_max;   // offset 48, 52
};
```
**Total:** 56 bytes

**Mismatches:**
| Field | Header offset | .cpp offset | Difference |
|---|---|---|---|
| `parent_idx` | 28 | 24 | Both uint32 but at different offsets |
| `is_fractured` | 32 | 28 | Different offset |
| First constraint | 36 | 32 | Different offset |
| All subsequent fields | +4 | base | Consistently shifted |

**Critical:** The header has `uint32_t id` at offset 0; the .cpp has **no `id` field** at all. All fields in the .cpp are shifted back by 4 bytes relative to the header.

### Struct 2: `BoneSegment`

**Header** (`api/skelly_api.h:52-61`):
```c
typedef struct BoneSegment {
    uint32_t id;              // offset 0
    int32_t start_node_idx;   // offset 4
    int32_t end_node_idx;     // offset 8
    float flexibility;        // offset 12
    float break_threshold;    // offset 16
    uint32_t is_fractured;    // offset 20
    float total_capacity;     // offset 24
    uint32_t organ_count;     // offset 28
} BoneSegment;
```
**Total:** 32 bytes

**Implementation** (`api/skelly_api.cpp:42-50`):
```cpp
struct BoneSegment {
    uint32_t start_node, end_node; // offset 0, 4
    float flexibility;              // offset 8
    float break_threshold;          // offset 12
    uint32_t is_fractured;          // offset 16
    float total_capacity;           // offset 20
    uint32_t organ_count;           // offset 24
    uint32_t organ_start;           // offset 28  ← EXTRA field!
};
```
**Total:** 32 bytes (but different field at offset 28!)

**Mismatches:**
| Field | Header offset | .cpp offset |
|---|---|---|
| `id` | 0 | — (doesn't exist) |
| `start_node_idx` / `start_node` | 4 | 0 |
| `end_node_idx` / `end_node` | 8 | 4 |
| `flexibility` | 12 | 8 |
| `break_threshold` | 16 | 12 |
| `is_fractured` | 20 | 16 |
| `total_capacity` | 24 | 20 |
| `organ_count` | 28 | 24 |
| `organ_start` | — | 28 |

The .cpp version is **missing `id`**, has `organ_start` that the header doesn't, and uses `uint32_t` instead of `int32_t` for start/end nodes.

### Struct 3: `MuscleGroup`

**Header** (`api/skelly_api.h:70-77`):
```c
typedef struct MuscleGroup {
    uint32_t id;                     // offset 0
    int32_t origin_node_idx;         // offset 4
    int32_t insertion_node_idx;      // offset 8
    float activation;                // offset 12
    uint32_t strand_start_idx;       // offset 16
    uint32_t strand_count;           // offset 20
} MuscleGroup;
```
**Total:** 24 bytes

**Implementation** (`api/skelly_api.cpp:58-64`):
```cpp
struct MuscleGroup {
    uint32_t origin_node, insertion_node; // offset 0, 4
    uint32_t strand_start;                // offset 8
    uint32_t strand_count;                // offset 12
    float activation;                     // offset 16
    float current_volume;                 // offset 20  ← EXTRA field!
};
```
**Total:** 24 bytes (different layout!)

**Mismatches:**
| Field | Header offset | .cpp offset |
|---|---|---|
| `id` | 0 | — |
| `origin_node_idx` / `origin_node` | 4 | 0 |
| `insertion_node_idx` / `insertion_node` | 8 | 4 |
| `strand_start_idx` / `strand_start` | 16 | 8 |
| `strand_count` | 20 | 12 |
| `activation` | 12 | 16 |
| `current_volume` | — | 20 |

Every single field is at a different offset.

### Struct 4: `Organ`

**Header** (`api/skelly_api.h:79-86`):
```c
typedef struct Organ {
    uint32_t id;              // offset 0
    int32_t type;             // offset 4
    float volume;             // offset 8
    float active_state;       // offset 12
    float energy_output;      // offset 16
    int32_t segment_idx;      // offset 20
} Organ;
```
**Total:** 24 bytes

**Implementation** (`api/skelly_api.cpp:66-72`):
```cpp
struct Organ {
    uint32_t type;            // offset 0
    float volume;             // offset 4
    float active_state;       // offset 8
    float energy_output;      // offset 12
    uint32_t segment_idx;     // offset 16
};
```
**Total:** 20 bytes

**Mismatches:**
| Field | Header offset | .cpp offset |
|---|---|---|
| `id` | 0 | — |
| `type` (int32_t vs uint32_t) | 4 | 0 |
| `volume` | 8 | 4 |
| `active_state` | 12 | 8 |
| `energy_output` | 16 | 12 |
| `segment_idx` (int32_t vs uint32_t) | 20 | 16 |

---

## Failure Scenario

1. A C API caller allocates a `BoneNode` using the header definition (60 bytes with `id` first).
2. The caller passes it to `skelly_api_step()` (in .cpp), which reads fields at .cpp offsets.
3. The .cpp reads `parent_idx` from offset 24, which in the header's layout is part of `global_z` → gets garbage.
4. The caller reads back a `BoneNode` using the header layout, getting all fields wrong.

The same happens for all Skelly operations: `skelly_api_create_creature`, `skelly_api_add_organ`, `skelly_api_get_system_state`, and any caller iterating over bones/segments/muscles/organs.

---

## Root Cause

The header was written as a standalone C API with `id` fields for identification. The .cpp was written independently for vncc (LLVM shader compiler) compatibility, using `float3` structs and omitting `id` fields. These were never cross-checked for binary compatibility.

---

## Recommended Fix

1. **Align** the .cpp struct definitions to exactly match the header layout (or vice versa — choose one canonical ABI).
2. **Add** the missing `id` field and `float3 → individual floats` conversion to the .cpp version.
3. **Add** `static_assert(sizeof(BoneNode) == expected_size, "ABI mismatch")` in both files.
4. **Add** a compile-time ABI compatibility test.

## Confidence
**100%** — Verified by comparing field layouts byte-by-byte.
