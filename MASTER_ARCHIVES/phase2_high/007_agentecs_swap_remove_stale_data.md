# Issue #007: AgentECS swap-remove leaves stale data, memory bloat, and PillarArrays UB

**Severity:** High  
**Phase:** 2 (High)  
**File:** `include/simulation/AgentECS.h`  
**Related:** `include/simulation/PillarArrays.h`

## Bug 1: Vector size never shrinks after remove

`AgentECS::remove_agent()` (lines 98-125) uses swap-remove: swaps the target element with the last element, then decrements `size_`. However, the underlying `std::vector`s are **never popped**:

```cpp
void remove_agent(Index idx) {
    // ... swap logic ...
    size_--;            // logical size decremented
    // vector sizes unchanged!
}
```

After removal, `std::vector::size()` remains at the old max value while `AgentECS::size_` is smaller. This means:

- **Memory bloat**: Vectors grow monotonically with peak agent count, never shrinking when agents die
- **Stale data accessible**: `pillar_array(p).size()` returns inflated count. Any caller iterating via vector `.size()` instead of ECS `.size_` will process stale/removed agents

## Bug 2: add_agent does not resize PillarArrays (potential UB)

`AgentECS::add_agent()` resizes float/int/uint8 vectors via `ensure_size` lambdas (lines 51-62), but **never resizes `pillars_`** before calling `pillars_.set_pillars(idx, initial_pillars)` at line 92:

```cpp
// Lines 64-75: vectors sized up
ensure_size(x_);     // x_.resize(idx+1) if needed
// ... 9 more ensure_size calls ...

// Line 92: direct array access — NO ensure_size equivalent!
pillars_.set_pillars(idx, initial_pillars);
```

`PillarArrays::set_pillars` (PillarArrays.h:25-28) does:
```cpp
arrays_[p][idx] = psv[p];  // UB if arrays_[p].size() <= idx
```

Since `AgentECS::reserve()` calls `pillars_.reserve()` (not `resize`), the PillarArrays vectors have `size() == 0` after construction. Writing via `operator[]` on a vector with `size() <= idx` is **undefined behavior**, even if `capacity() > idx`. This works accidentally on most MSVC implementations due to lack of bounds checking, but is technically UB and will crash on checked/debug iterators.

This UB occurs **every time** `add_agent` is called after construction (all agents after the reserved capacity fails too, since `reserve` doesn't change `size()`).

## Bug 3: Duplicated swap-remove logic

`AgentECS::remove_agent` lines 118-121 manually swap each pillar array element:

```cpp
for (size_t p = 0; p < NUM_PILLARS; ++p) {
    std::swap(pillars_.pillar_array(static_cast<PillarIndex>(p))[idx],
              pillars_.pillar_array(static_cast<PillarIndex>(p))[last_idx]);
}
```

`PillarArrays` already provides an identical `swap_remove` method (PillarArrays.h:51-55):

```cpp
void swap_remove(size_t idx, size_t last_idx) {
    for (int p = 0; p < NUM_PILLARS; ++p) {
        std::swap(arrays_[p][idx], arrays_[p][last_idx]);
    }
}
```

The ECS should call `pillars_.swap_remove(idx, last_idx)` instead of duplicating the loop.

## Impact

| Bug | Severity | Reproducibility |
|-----|----------|-----------------|
| Memory bloat | Medium | Always — every remove() leaks the vector capacity |
| Stale data via vector::size() | Medium | Any external iteration via pillar_array(p).size() processes ghosts |
| PillarArrays UB on add | High (crash on debug builds) | Every add_agent() call writes beyond vector size |
| Duplicated swap_remove | Low (code quality) | Always |

## Recommended Fix

```cpp
void remove_agent(Index idx) {
    if (!is_valid(idx)) return;
    if (size_ == 0) return;

    const Index last_idx = size_ - 1;
    if (idx != last_idx) {
        std::swap(x_[idx], x_[last_idx]);
        std::swap(y_[idx], y_[last_idx]);
        std::swap(z_[idx], z_[last_idx]);
        std::swap(vx_[idx], vx_[last_idx]);
        std::swap(vy_[idx], vy_[last_idx]);
        std::swap(vz_[idx], vz_[last_idx]);
        std::swap(active_[idx], active_[last_idx]);
        std::swap(resources_[idx], resources_[last_idx]);
        std::swap(last_hash_x_[idx], last_hash_x_[last_idx]);
        std::swap(last_hash_y_[idx], last_hash_y_[last_idx]);
        std::swap(last_hash_z_[idx], last_hash_z_[last_idx]);
        std::swap(cognitions_[idx], cognitions_[last_idx]);
        pillars_.swap_remove(idx, last_idx);  // use PillarArrays method
    }

    // Pop back all vectors to match logical size
    x_.pop_back();
    y_.pop_back();
    z_.pop_back();
    vx_.pop_back();
    vy_.pop_back();
    vz_.pop_back();
    active_.pop_back();
    resources_.pop_back();
    last_hash_x_.pop_back();
    last_hash_y_.pop_back();
    last_hash_z_.pop_back();
    cognitions_.pop_back();
    pillars_.resize(size_ - 1);  // shrink PillarArrays too

    size_--;
}
```

Alternatively, for performance-critical paths where pop_back overhead is undesirable, document the invariant that `size_` is the authoritative count and all vector `size()` values are always >= `size_`. Then add an explicit `shrink_to_fit` or `compact()` method.

Also add a `pillars_.resize(idx + 1)` in `add_agent` before calling `set_pillars` to fix the UB.
