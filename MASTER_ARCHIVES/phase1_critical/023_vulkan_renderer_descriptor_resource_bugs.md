# SKEIN AUDIT: Issue #023 — vulkan_renderer.cpp: Descriptor Writes to Non-Existent Bindings + Resource Leaks

## Severity
**Critical** — GPU writes to non-existent descriptor bindings → undefined behavior; resource leaks on repeated calls

## Category
API Misuse — Vulkan validation errors, resource management

---

## Summary

Three classes of bugs in `renderer/vulkan_renderer.cpp`:

1. **Descriptor writes to bindings 4 and 5** that are not defined in any descriptor set layout.
2. **Binding conflicts** between bone buffer (binding 3) and agent buffer (binding 3).
3. **Buffer/memory leaks** on repeated `uploadMuscles()`/`uploadOrgans()` calls.
4. **Array leaks** on window resize (swapchain arrays overwritten without `delete[]`).

---

## Evidence

### Bug 1: Writes to Non-Existent Bindings 4 and 5

The compute descriptor set layout is defined at lines 230-234 with **4 bindings (0-3)**:

```cpp
VkDescriptorSetLayoutBinding bindings[4] = {};
bindings[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
bindings[2] = {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT};
bindings[3] = {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
```

**But `uploadMuscles()` writes to binding 4** (line 664):
```cpp
wds.dstBinding = 4;
```

**And `uploadOrgans()` writes to binding 5** (line 705):
```cpp
wds.dstBinding = 5;
```

Neither binding 4 nor 5 exists in the descriptor set layout. This is **undefined behavior** in Vulkan — the driver may ignore the write, crash, or corrupt GPU memory. Vulkan validation layers will report `VK ERROR: descriptor binding #4 not found in descriptor set layout`.

### Bug 2: Binding 3 Conflict — Bone Buffer vs Agent Buffer

`uploadBones()` writes the bone buffer descriptor to binding 3 (line 612):
```cpp
wds.dstBinding = 3;
```

`uploadAgents()` also writes the agent buffer descriptor to binding 3 (line 752):
```cpp
wds.dstBinding = 3;
```

Only the **last written** descriptor survives. If both bones and agents are uploaded, the compute shader receives the wrong data for whichever was written first. Since `uploadAgents` doesn't check whether bones were uploaded first, calling `uploadAgents` after `uploadBones` silently replaces the bone buffer with the agent buffer.

### Bug 3: Resource Leaks in uploadMuscles/uploadOrgans

`uploadBones()` properly destroys old buffers before allocating new ones (lines 562-565):
```cpp
if (boneBuffer) {
    vkDestroyBuffer(device, boneBuffer, nullptr);
    vkFreeMemory(device, boneBufferMemory, nullptr);
}
```

But `uploadMuscles()` (lines 621-671) and `uploadOrgans()` (lines 673-712) **never check for or clean up** existing buffers. On each subsequent call:
- A new `VkBuffer` is created
- A new `VkDeviceMemory` is allocated
- The old `VkBuffer` and `VkDeviceMemory` are leaked

### Bug 4: Swapchain Array Leaks on Resize

`createSwapchain()` allocates arrays with `new[]` (lines 181, 183):
```cpp
swapchainImages = new VkImage[imageCount];
swapchainImageViews = new VkImageView[imageCount];
```

On window resize, `endFrame()` → `cleanupSwapchain()` + `createSwapchain()`. `cleanupSwapchain()` destroys the individual `VkImageView` objects and the swapchain, but **does not `delete[]` the arrays**. The next `createSwapchain()` call overwrites the pointers, leaking the old arrays.

---

## Failure Scenario

1. Engine starts, `uploadBones()` → writes bone descriptor to binding 3.
2. `uploadAgents()` → overwrites binding 3 with agent descriptor.
3. Compute shader reads "bones" from binding 3 → gets agent data → garbage physics.
4. `uploadMuscles()` → allocates new buffer+memory every frame → VRAM exhaustion.
5. Window resize → both `VkImage[]` and `VkImageView[]` arrays leak.

---

## Recommended Fix

1. **Extend** the descriptor set layout to include bindings 4 and 5 (or consolidate Skelly data into binding 1).
2. **Assign** distinct binding numbers to bones, muscles, organs, and agents, or use separate descriptor sets.
3. **Add** cleanup logic to `uploadMuscles()` and `uploadOrgans()` matching the pattern in `uploadBones()`.
4. **Add** `delete[]` for `swapchainImages` and `swapchainImageViews` before reallocation in `createSwapchain()`.

## Confidence
**100%** — Verified by reading source code. Each issue is trivially visible.
