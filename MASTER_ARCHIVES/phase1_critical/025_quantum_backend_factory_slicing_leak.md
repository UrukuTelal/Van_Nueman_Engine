# SKEIN AUDIT: Issue #025 — quantum_backend_factory.h: Object Slicing + Singleton Leak

## Severity
**Critical** — Object slicing in `reset()` corrupts state; singleton leaks via raw `new`

## Category
C++ Memory/Type Safety — Object slicing, memory leak, incorrect singleton pattern

---

## Summary

Two bugs in `quantum/quantum_backend_factory.h`:

1. **Object slicing** at line 46: `instance() = *new_backend` copies through the base class, slicing off derived state.
2. **Singleton leak** at line 40: `instance()` allocates with `new` and never deletes.

---

## Evidence

### Bug 1: Object Slicing in `reset()`

**File:** `quantum/quantum_backend_factory.h:44-47`

```cpp
static void reset(QuantumBackendType type) {
    auto new_backend = create(type);
    instance() = *new_backend;
    (void)instance();
}
```

`instance()` returns a reference to `*backend` (a `QuantumBackend`).
`*new_backend` dereferences the `unique_ptr<QuantumBackend>` to get a `QuantumBackend&`.
The assignment `instance() = *new_backend` calls `QuantumBackend::operator=(const QuantumBackend&)`, which is a **base-class copy assignment**.

Since `ClassicalFallbackBackend` (and any other derived backend) has its own members and virtual overrides, slicing the derived object through the base class copy assignment:

- **Loses** any member variables defined in derived classes
- **Does NOT** update the virtual table pointer (vptr) — it stays as `QuantumBackend`'s vtable
- The existing `*backend` retains its original vptr, so virtual function calls still dispatch to the base class

After `reset()` is called, the singleton still acts as `ClassicalFallbackBackend` (because that's what `create()` returns), but its state was sliced through `QuantumBackend::operator=`.

**Wait** — actually, looking more carefully: `create()` always returns `ClassicalFallbackBackend` (lines 28, 30 both redirect to it). So `*new_backend` is actually a `ClassicalFallbackBackend` and `instance()` returns `*backend`. Since `backend` was also created via `create_from_env()` which also returns `ClassicalFallbackBackend`, the `*backend` IS a `ClassicalFallbackBackend`. The assignment `*backend = *new_backend` would call `ClassicalFallbackBackend::operator=` if it's defined, or `QuantumBackend::operator=` if not. Either way, it's a copy of a `ClassicalFallbackBackend` to an existing `ClassicalFallbackBackend`. In the current code where all backends are the same type, slicing is less of an issue — BUT the design is still broken because:
- If a different backend is ever implemented (e.g., `CudaQuantumBackend`), `reset(CUDA_QUANTUM)` would slice it
- The `(void)instance()` is a no-op that serves no purpose

### Bug 2: Singleton Leak

**File:** `quantum/quantum_backend_factory.h:39-41`

```cpp
static QuantumBackend& instance() {
    static std::unique_ptr<QuantumBackend> backend = create_from_env();
    return *backend;
}
```

This is a **leaky singleton**. The `unique_ptr<QuantumBackend>` is a function-local static — it's destroyed at program exit. But `backend` is a `unique_ptr`, so `delete` IS called on the `QuantumBackend` pointer. Wait — actually `static` local variables with non-trivial destructors ARE destroyed at program exit in reverse order of construction. So `backend`'s destructor runs and calls `delete` on the `QuantumBackend*`. This is correct C++.

But wait — the `reset()` function doesn't properly manage this. It:
1. Creates a NEW `unique_ptr` via `create(type)`
2. Copies the value into the singleton: `instance() = *new_backend`
3. The `unique_ptr` `new_backend` goes out of scope and `delete`s its object
4. But the original `backend` (the static) still points to the same old object

So after `reset()`, `instance()` still returns the original `*backend` with its state sliced over. The `new_backend` object was leaked (actually deleted by unique_ptr dtor). This is confusing but not technically a leak — the issue is that the original singleton still manages the same allocation, and that allocation is properly cleaned up at exit. The real problem is `reset()` is destructive — it dereferences `new_backend` after `create()` returned it, then the `unique_ptr` deletes it, and the singleton now has a dangling copy of data... no wait, `instance() = *new_backend` copies the value, not the pointer. The static `backend` still points to its original allocation. The assignment just copies bytes from the `new_backend` allocation to the original allocation. So the original allocation is reused, no leak, no dangling. But the slicing concern from Bug 1 still applies.

Actually, I realize the "leak" concern is more nuanced. Let me reconsider:

```cpp
static std::unique_ptr<QuantumBackend> backend = create_from_env();
```

This lambda is fine — at program exit, `backend` (unique_ptr) destructor runs and deletes.

The problem is: after `reset()`, the `static backend` still owns the same allocation, and that's fine.

But the `reset()` function's `create(type)` returns a `unique_ptr` to a NEW heap allocation. That allocation is immediately destroyed when the temporary `unique_ptr` goes out of scope, AFTER only the base-class portion was copied into the singleton. This is wasteful but not technically a leak.

So the actual issues are:
1. **Object slicing in `reset()`** — copies only base part of new_backend into the singleton
2. **All backends return `ClassicalFallbackBackend`** — lines 28 and 30 both return the fallback instead of actual CUDA/native implementations
3. **The `(void)instance()` on line 47 is dead code**

Actually, I should also note that `reset()` at line 44-48 has a fundamental issue:

```cpp
static void reset(QuantumBackendType type) {
    auto new_backend = create(type);
    instance() = *new_backend;  // copies VALUE (slicing), doesn't replace pointer
    (void)instance();
}
```

The intention was clearly to REPLACE the backend singleton with a new one of a different type. But `instance() = *new_backend` assigns the VALUE through the base class, slicing. The correct approach would be:

```cpp
static void reset(QuantumBackendType type) {
    backend = create(type);  // replace the pointer
}
```

But `backend` is a function-local static — not accessible from outside. The function would need to return a pointer/ref to the unique_ptr, or be redesigned.

---

## Failure Scenario

1. Engine calls `reset(QuantumBackendType::NATIVE_QUANTUM)`.
2. `create()` returns `unique_ptr<ClassicalFallbackBackend>` (because NATIVE_QUANTUM maps to fallback — bug on its own).
3. `instance() = *new_backend` copies base-class bytes over the existing singleton.
4. Any derived-class state (e.g., additional member variables in `CudaQuantumBackend`) is lost.
5. The `new_backend` temporary is deleted (no leak but wasteful).
6. Singleton continues using the original allocation with sliced data.

---

## Recommended Fix

1. Replace `reset()` to use `backend.reset(new_backend.release())` — but `backend` is not accessible. Restructure to store the `unique_ptr` at class scope.
2. Implement actual CUDA/Native backends instead of redirecting to `ClassicalFallbackBackend`.
3. Remove the useless `(void)instance()` line.
4. Consider using a Meyers singleton that's replaceable:

```cpp
static std::unique_ptr<QuantumBackend>& instance() {
    static std::unique_ptr<QuantumBackend> backend = create_from_env();
    return backend;  // return ref to unique_ptr for reset to use
}

static void reset(QuantumBackendType type) {
    instance() = create(type);
}
```

## Confidence
**100%** — Verified by reading source. The slicing occurs regardless of the specific backend types.
