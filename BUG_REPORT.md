# Final Bug Report — Van Nueman Engine

Last updated: 2026-06-20

---

## CRITICAL

### C1. CUDA shared-memory buffer overflow (4 bytes)

**File:** `api/pillars_api.cu:175`
**Severity:** CRITICAL — GPU memory corruption

```cpp
size_t shared_bytes = (256 * NumPillars * sizeof(float)) + (1 * sizeof(uint32_t));
```

Allocates 16388 bytes (`256*16*4 + 4`). The kernel in `kernels/pillars_compute.cu:114-117` lays out shared memory as:

```cuda
float* s_theta = (float*)shared_mem;                          // offset 0,     size 16384
uint32_t* s_servers_in_block = (uint32_t*)&s_theta[...];       // offset 16384, size 4
uint32_t* s_count = (uint32_t*)&s_servers_in_block[1];         // offset 16388, size 4
```

`s_count[0]` writes to offsets 16388–16391, but only 16388 bytes were allocated (offsets 0–16387). This is a **4-byte overflow** into adjacent shared memory, corrupting subsequent kernel launches or causing silent data corruption.

**Fix:** `shared_bytes = (256 * NumPillars * sizeof(float)) + (2 * sizeof(uint32_t))` (add one more `uint32_t` for `s_count`).

---

### C2. CUDA `__syncthreads()` divergence deadlock

**File:** `kernels/pillars_compute.cu:128-129,148`
**Severity:** CRITICAL — GPU hang / driver reset

```cuda
if (idx >= num_entities) return;    // line 128
if (!entities[idx].alive) return;   // line 129
// ...
__syncthreads();                    // line 148
```

When `num_entities` is not a multiple of `blockDim.x` (256), or any entity has `alive == false`, threads in the last block return early at line 128-129, while other threads in the same block reach `__syncthreads` at line 148 and **wait forever**. This is a well-known CUDA divergence deadlock pattern.

Also affects the second `__syncthreads()` at line 158 and the one inside the `if (uniform)` block at line 167.

**Fix:** Replace early return with masked computation. Use a shared-memory flag and conditionally skip work, but always let all threads reach every `__syncthreads()`:

```cuda
bool valid = (idx < num_entities) && entities[idx].alive;
// ... guard all work with `if (valid)` ...
__syncthreads();  // all threads reach this
if (valid) { /* ... */ }
```

---

### C3. `realloc` NULL return — memory leak + NULL dereference

**File:** `server_registry/approval_system.cpp:77-78`
**Severity:** CRITICAL — crash / memory leak on allocation failure

```cpp
impl->servers = (ServerInfoImpl*)realloc(impl->servers,
                                          sizeof(ServerInfoImpl) * impl->server_capacity);
```

If `realloc` fails (returns NULL), the original pointer (`impl->servers`) is **overwritten with NULL**, leaking the original allocation. The next line then writes through `impl->servers[impl->server_count++]`, which is a **NULL dereference crash**.

**Fix:**
```cpp
ServerInfoImpl* new_servers = (ServerInfoImpl*)realloc(impl->servers,
                              sizeof(ServerInfoImpl) * impl->server_capacity);
if (!new_servers) return -1;  // keep original allocation intact
impl->servers = new_servers;
```

---

### C4. Vulkan swapchain out-of-date → semaphore deadlock

**File:** `renderer/vulkan_renderer.cpp:1028-1029`
**Severity:** CRITICAL — GPU deadlock, application freeze

```cpp
vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame],
                         VK_NULL_HANDLE, &imageIndex);
```

The return value of `vkAcquireNextImageKHR` is **not checked**. If the swapchain is out-of-date (window resize, monitor change), the semaphore is **not signaled**. The subsequent `vkQueueSubmit` waits forever on an unsignaled semaphore, deadlocking the GPU pipeline.

**Fix:**
```cpp
VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                      imageAvailableSemaphores[currentFrame],
                                      VK_NULL_HANDLE, &imageIndex);
if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
    recreate_swapchain();
    return false;
}
```

---

## HIGH

### H1. Negative `max_len` → UB in `new char[max_len]`

**File:** `audio/voice_system.cpp:80`
**Severity:** HIGH — UB / crash if caller passes negative `max_len`

```cpp
char* text_buffer = new char[max_len];
```

`max_len` is `int`. If a caller passes a negative value (e.g., error code), `new char[-1]` is undefined behavior (array new with negative size is ill-formed; the allocation wraps to a huge value on most implementations, causing `std::bad_alloc` or crash).

**Fix:**
```cpp
if (max_len <= 0) return nullptr;
```

---

## LOW

### L1. Missing `malloc` null check (auditorium)

**File:** `audio/audio_system.cpp:262`
**Severity:** LOW — unlikely to fail (small allocation), but defensively incorrect

```cpp
char* result = (char*)malloc(strlen(dummy) + 1);
strcpy(result, dummy);
```

If `malloc` returns NULL, `strcpy` crashes.

**Fix:** `if (!result) return nullptr;`

---

## Previously Fixed (per AGENTS.md)

All 28/28 issues from the June 19 Council audit have been resolved:
- C1–C5: GPU Entity struct, Vulkan UBO (false positive), cudaMalloc checks, stack allocation, VoiceSystem stub
- W1–W6: strncpy termination, uninitialized wht_weights, hardcoded multipliers, ImGui cleanup, buffer size, unused params
- P-05, S-02, 8.5, 8.6, H-01–H-03, SV-01: Voxel dirty flag, REST API limits, ScaleExponent agents, ScaledInt bridge, GPU/CPU tests, FLL test, SVO pass

163 tests pass, 0 failures.

---

## Summary of Unfixed Bugs

| # | File:Line | Severity | Description |
|---|-----------|----------|-------------|
| C1 | `api/pillars_api.cu:175` | CRITICAL | Shared memory 4-byte overflow (16388 alloc vs 16392 needed) |
| C2 | `kernels/pillars_compute.cu:128` | CRITICAL | `__syncthreads` divergence deadlock on early return |
| C3 | `server_registry/approval_system.cpp:77` | CRITICAL | `realloc` null → memory leak + NULL deref |
| C4 | `renderer/vulkan_renderer.cpp:1028` | CRITICAL | Swapchain out-of-date → unsignaled semaphore → deadlock |
| H1 | `audio/voice_system.cpp:80` | HIGH | Negative `max_len` → UB in `new char[]` |
| L1 | `audio/audio_system.cpp:262` | LOW | Missing `malloc` null check |
