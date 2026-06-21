# Issue #008: Per-frame staging buffer allocation in GPU upload/download

**Severity:** Medium (performance)  
**Phase:** 3 (Medium)  
**Files:**
- `simulation/transform_compute.cpp` (upload: lines 286-341, download: lines 392-455)
- `simulation/hopf_compute.cpp` (upload: lines 240-327, download: lines 369-454)

## Description

Both `TransformCompute` and `HopfCompute` allocate temporary `std::vector` staging buffers **every** upload/download cycle. These heap allocations occur once per tick (potentially 60+ times/second) and are completely redundant: the GPU buffers are already `HOST_VISIBLE | HOST_COHERENT`, meaning the CPU can write to them directly.

### TransformCompute::upload() allocations per tick

| Line | Type | Size |
|------|------|------|
| 298 | `vector<float>(count)` | pos/vel reuse — 3+3 fields × count |
| 320 | `vector<uint32_t>(count)` | active mask |
| 327 | `vector<int32_t>(count)` | resources |
| 334 | `vector<int64_t>(count)` × 16 pillars | pillar arrays |

### TransformCompute::download() allocations per tick

| Line | Type | Size |
|------|------|------|
| 411 | `vector<float>(count)` | positions |
| 422 | `vector<float>(count)` | velocities |
| 433 | `vector<uint32_t>(count)` | active |
| 440 | `vector<int32_t>(count)` | resources |
| 447 | `vector<int64_t>(count)` × 16 pillars | pillar arrays |

### HopfCompute::upload() allocations per tick

| Line | Type | Size |
|------|------|------|
| 248 | `vector<int64_t>(count × 512)` | thought state (4 MB @ 1000 entities) |
| 263 | `vector<float>(count × 1024)` | WHT coefficients (4 MB) |
| 277 | `vector<float>(count × 32)` | membrane |
| 289 | `vector<float>(count × 256)` | fiber coherence (1 MB) |
| 302-306 | 5 separate vectors | scalar metadata |

### HopfCompute::download() allocations per tick

Mirrors upload — same staging vectors re-allocated.

## Impact

- **Heap thrashing**: ~40+ heap allocations/deallocations per tick, each with O(n) memset in debug
- **Cache pollution**: Staging buffers evict hot ECS data from cache during copy
- **Redundant memcpy**: Data is copied ECS→staging→GPU (or GPU→staging→ECS) instead of ECS↔GPU directly
- **At 60 FPS**: 2400+ heap allocation cycles per second

Since all GPU buffers use `HOST_VISIBLE | HOST_COHERENT` memory, staging vectors provide zero benefit — the mapped pointer can be written directly.

## Recommended Fix

Pre-map all GPU buffers persistently at init time, then write/read directly in upload/download:

```cpp
// In init(), after createBuffers:
vkMapMemory(device_, memory, 0, VK_WHOLE_SIZE, 0, &mappedPtr_);

// In upload():
int64_t* dst = static_cast<int64_t*>(mappedPtr_);
for (uint32_t i = 0; i < count; i++)
    dst[i] = ecs.pillar(i, PILLAR_WILLPOWER).raw();
// No staging vector, no vkMapMemory/vkUnmapMemory per frame

// In cleanup():
vkUnmapMemory(device_, memory);
```

This eliminates all per-frame staging allocations and the `mapAndUpload`/`mapAndRead` helper overhead. For SoA layouts where elements are scattered in memory, write directly into the mapped pointer indexed by `i` rather than gathering into a contiguous staging vector first.
