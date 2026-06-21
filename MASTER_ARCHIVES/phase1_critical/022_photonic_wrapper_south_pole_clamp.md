# SKEIN AUDIT: Issue #022 — photonic_wrapper.comp Bloch Rotation Always Moves Toward South Pole

## Severity
**Critical** — All inward enchantment rotations move in only one direction

## Category
Mathematics — Sign error in Bloch rotation delta computation

---

## Summary

In `shaders/photonic_wrapper.comp:114-117`, the Bloch rotation delta is computed with a redundant sign correction that makes delta **always positive**, causing all rotations to move toward the south pole (theta → π, value → 0).

---

## Evidence

**File:** `shaders/photonic_wrapper.comp:109-117`

```glsl
float target_value = (direction > 0) ? 1.0 : 0.0;

float theta = pw_value_to_theta(current);
float target_theta = pw_value_to_theta(target_value);
float diff = target_theta - theta;
float delta = diff * abs(rotation_rate);
if (abs(delta) > 0.0001) {
    inout_entities.pillars[base + p] = pw_apply_bloch(
        current, delta * ((diff > 0) ? 1.0 : -1.0)
    );
}
```

**Analysis:**

| Variable | When direction > 0 (White) | When direction < 0 (Black) |
|---|---|---|
| `target_value` | 1.0 | 0.0 |
| `target_theta` | acos(2*1-1) = acos(1) = 0 | acos(2*0-1) = acos(-1) = π |
| `diff` | 0 - theta = -theta | π - theta |
| `delta = diff * abs(rotation_rate)` | -theta * r | (π - theta) * r |
| `delta * ((diff > 0) ? 1 : -1)` | -theta * r * (-1) = theta * r | (π - theta) * r * 1 = (π - theta) * r |

For White (direction > 0): `diff = -theta` (negative), so `diff > 0` is false → multiply by -1 → `delta * (-1) = theta * r`. Result is positive.

For Black (direction < 0): `diff = π - theta` (positive when theta < π), so `diff > 0` is true → multiply by 1 → `delta = (π - theta) * r`. Result is positive.

**In both cases the delta passed to `pw_apply_bloch` is positive**, which moves theta toward π (south pole, value = 0).

---

## Root Cause

The code computes `delta = diff * abs(rotation_rate)` — the sign of `diff` is already baked into `delta`. Then it multiplies by `sign(diff)` again: `delta * ((diff > 0) ? 1.0 : -1.0) = diff * abs(rotation_rate) * sign(diff) = |diff| * abs(rotation_rate)`. This is always positive.

The correct code is:
```glsl
float delta = diff * abs(rotation_rate);
if (abs(delta) > 0.0001) {
    inout_entities.pillars[base + p] = pw_apply_bloch(current, delta);
}
```

No sign re-application needed — `diff` already has the correct sign.

---

## Failure Scenario

When direction is White (alpha = 0), the target is value = 1.0 (north pole, theta = 0). The intended behavior is to rotate theta toward 0 (decreasing theta). But the code always rotates toward π (increasing theta), so White enchantment pushes **away** from its target instead of toward it. Both White and Black enchantments move toward the south pole (value = 0).

---

## Recommended Fix

Remove the redundant sign correction at line 117:
```glsl
current, delta  // instead of delta * ((diff > 0) ? 1.0 : -1.0)
```

## Confidence
**100%** — Verified by tracing through the math for both directions.
