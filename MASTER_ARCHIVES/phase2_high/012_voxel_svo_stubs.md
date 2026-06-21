# Issue #012: voxel_svo — svo_build is a no-op stub (both .cpp and .cu)

**Severity:** High (SVO tree never built)  
**Phase:** 2 (High)  
**Files:**
- `kernels/voxel_svo.cpp:90-98`
- `kernels/voxel_svo.cu:88-96`

## Description

Both the vncc kernel and CUDA kernel versions of `svo_build` contain no actual octree construction logic:

**voxel_svo.cpp:90-98:**
```cpp
void svo_build(SVO_Chunk* chunks, uint32_t num_chunks, uint32_t* dirty_voxels) {
    for (uint32_t chunk_idx = 0; chunk_idx < num_chunks; chunk_idx++) {
        if (!chunks[chunk_idx].dirty) continue;
        SVO_Chunk& chunk = chunks[chunk_idx];
        chunk.dirty = 0;  // Only clears dirty flag, no tree built
    }
}
```

**voxel_svo.cu:88-96** (same logic).

The SVO node pool (`g_node_pool`, `d_node_pool`) is allocated and initialized, nodes can be allocated via `svo_alloc_node()`, but **no function ever populates the tree**. The `children` pointers in `SVO_Node` are always 0.

## Impact

- Voxel data is stored in chunks but the SVO acceleration structure is never populated
- `svo_ray_intersect()` (voxel_svo.cpp:112-158, render_image.cu:55-84) traverses a **non-existent tree** — will always miss
- Any SVO-based rendering, physics collision, or query will produce empty results
- The `update_svo()` call in `tick_loop.cpp:699-703` calls into `SVO_Chunk::update_svo()` which presumably calls the kernel, which does nothing

## render_image.cu AABB Intersection

The AABB intersection in `render_image.cu:64-65` uses CUDA `float3` component-wise operators and appears **mathematically correct**:

```cpp
float3 t1 = (box_min - ray.origin) * inv_dir;  // element-wise, correct
float3 t2 = (box_max - ray.origin) * inv_dir;  // element-wise, correct
```

However, this function (`svo_ray_intersect`) will never return a valid hit because the SVO root node is always empty (all children are null due to the stub build).

## Recommended Fix

Implement voxelization into the SVO in `svo_build`: iterate over chunk voxels, insert non-empty voxels into the octree, subdivide based on LOD, and populate `children[]` pointers in `SVO_Node` entries in the node pool.
