# Issues #009-#010: Entity.h — Signed right-shift fix status & EntityUID::combined() data loss

**Severity:** Medium  
**Phase:** 2 (High) / 3 (Medium)  
**File:** `include/Entity.h:55-92`

## Issue #009: Signed Right-Shift (Previously Fixed)

Line 58-61 shows the fix already applied:

```cpp
int64_t raw = psv.pillars[i].raw();
uint32_t low = static_cast<uint32_t>(raw & 0xFFFFFFFF);
uint32_t high = static_cast<uint32_t>((static_cast<uint64_t>(raw) >> 32) & 0xFFFFFFFF);
```

Raw extraction and left-shift on line 58 is fine (`int64_t raw = ...`). The right-shift on line 61 correctly casts to `uint64_t` before shifting, avoiding implementation-defined signed right-shift behavior. **This issue is resolved.** Retained for audit completeness.

## Issue #010: EntityUID::combined() — Upper 16 bits of sid/hid silently dropped

Line 82-85:

```cpp
uint64_t combined() const {
    return ((uint64_t)psv_hash << 32) | ((uint64_t)(sid & 0xFFFF) << 16) | (hid & 0xFFFF);
}
```

The "fix" masks `sid` and `hid` to 16 bits each to prevent overlap with `psv_hash`. However:

- `sid` and `hid` are declared as `uint32_t` (lines 79-80)
- The mask `sid & 0xFFFF` silently drops the upper 16 bits
- `operator==` (lines 87-91) compares full `uint32_t sid` and `uint32_t hid` values

This means two entities with `sid = 0xABCD0001` and `sid = 0x12340001` would have:
- Different `operator==` results (correctly unequal)
- **Same `combined()` value** (both become `0x0001` after masking)

If `combined()` is used as a hash or unique key (e.g., in `std::unordered_map<EntityUID>` that uses `combined()` for hashing), this produces **collisions** between entities that differ only in the upper 16 bits of `sid` or `hid`.

### Impact

| Scenario | Consequence |
|----------|-------------|
| `combined()` used as hash key | Collisions for entities differing in `sid[31:16]` or `hid[31:16]` |
| `combined()` used as unique ID | False duplicates, data loss |
| `operator==` vs `combined()` mismatch | Subtle correctness bugs |

### Fix

Either widen `combined()` to 96 bits, or shift `sid` by 32 and `hid` by 0 without masking (64 bits total for `psv_hash` + `sid`, with `psv_hash` validation):

```cpp
uint64_t combined() const {
    return ((uint64_t)psv_hash << 32) | sid;  // loses hid
}
// Or use a 96-bit or 128-bit combined structure
struct EntityUIDCombined {
    uint64_t high; // psv_hash
    uint32_t low;  // sid
    uint32_t hid;
};
```
