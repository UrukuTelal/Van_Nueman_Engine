#pragma once
#include "FractalNode.h"
#include "QuantumSeed.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <mutex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef __CUDACC__
#include <cuda_runtime.h>
#endif

namespace vn {
namespace fll {

// ── Unified Virtual Memory Arena Allocator ──────────────────────────
// Deterministic, fragmentation-free GPU-backed allocator for
// scale-invariant FractalNode graphs.
//
// Strategy: single contiguous UVM allocation partitioned into:
//   - Node slab: linear bump allocator for FractalNode objects
//   - Seed slab: linear bump allocator for QuantumSeed objects
//   - Scratch: ring-buffer for temporary resonance computations
//
// Thread-safe (lock-free fast path, mutex on pool exhaustion).

class ArenaAllocator {
public:
    // Default capacities tuned for ~1.5 GB total arena
    static constexpr uint64_t DEFAULT_NODE_CAPACITY = 1u << 19;  // 524k nodes × 2560B ≈ 1.28 GB
    static constexpr uint64_t DEFAULT_SEED_CAPACITY = 1u << 16; // 65536 seeds × ~2300B ≈ 150 MB
    static constexpr uint64_t DEFAULT_SCRATCH_SIZE   = 1u << 24; // 16 MB scratch

    static constexpr uint64_t NODE_SLAB_SIZE =
        DEFAULT_NODE_CAPACITY * sizeof(FractalNode);
    static constexpr uint64_t SEED_SLAB_SIZE =
        DEFAULT_SEED_CAPACITY * sizeof(QuantumSeed);
    static constexpr uint64_t TOTAL_ARENA_SIZE =
        NODE_SLAB_SIZE + SEED_SLAB_SIZE + DEFAULT_SCRATCH_SIZE;

    ArenaAllocator() = default;
    ~ArenaAllocator();

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    // ── Lifecycle ─────────────────────────────────────────────
    bool init(uint64_t node_capacity = DEFAULT_NODE_CAPACITY,
              uint64_t seed_capacity = DEFAULT_SEED_CAPACITY,
              uint64_t scratch_size  = DEFAULT_SCRATCH_SIZE);

    void shutdown();

    bool is_initialized() const { return arena_ != nullptr; }

    // ── Allocation (deterministic bump, no free) ──────────────
    FractalNode* alloc_node();
    QuantumSeed* alloc_seed();

    // ── Reset (reuse arena without deallocation) ──────────────
    // Resets all allocation counters to zero, allowing the arena
    // to be reused without freeing/reallocating memory.
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        node_count_     = 0;
        seed_count_     = 0;
        scratch_offset_ = 0;
    }

    // ── Scratch buffer (temporary, ring-buffer semantics) ─────
    void* scratch_alloc(uint64_t size);
    void scratch_reset();

    // ── Access ────────────────────────────────────────────────
    FractalNode* get_node(uint64_t index) const;
    QuantumSeed* get_seed(uint64_t index) const;
    FractalNode* get_node_by_uid(uint64_t uid) const;

    uint64_t node_count() const { return node_count_; }
    uint64_t seed_count() const { return seed_count_; }
    uint64_t node_capacity() const { return node_capacity_; }
    uint64_t seed_capacity() const { return seed_capacity_; }

    // ── Batch operations ──────────────────────────────────────
    // Prefetch node graph into GPU memory
    void prefetch_to_device(uint64_t node_index, uint64_t count);
    void prefetch_all_to_device();

    // ── Stats ─────────────────────────────────────────────────
    struct Stats {
        uint64_t arena_bytes;
        uint64_t node_bytes_used;
        uint64_t seed_bytes_used;
        uint64_t scratch_bytes_used;
        uint64_t node_utilization_pct;
        uint64_t seed_utilization_pct;
    };

    Stats get_stats() const;

private:
    // ── Memory layout ─────────────────────────────────────────
    uint8_t* arena_           = nullptr;
    uint64_t arena_size_      = 0;

    FractalNode* node_slab_   = nullptr;
    uint64_t node_capacity_   = 0;
    uint64_t node_count_      = 0;

    QuantumSeed* seed_slab_   = nullptr;
    uint64_t seed_capacity_   = 0;
    uint64_t seed_count_      = 0;

    uint8_t* scratch_base_    = nullptr;
    uint64_t scratch_size_    = 0;
    uint64_t scratch_offset_  = 0;

    std::mutex mutex_;

#if defined(VN_USE_CUDA_QUANTUM) && defined(__CUDACC__)
    int gpu_device_ = 0;
#endif
};

// ── Inline Implementation ──────────────────────────────────────────

inline ArenaAllocator::~ArenaAllocator() {
    shutdown();
}

inline bool ArenaAllocator::init(uint64_t node_capacity,
                                  uint64_t seed_capacity,
                                  uint64_t scratch_size) {
    if (arena_) return false;

    node_capacity_ = node_capacity;
    seed_capacity_ = seed_capacity;
    scratch_size_  = scratch_size;

    const uint64_t node_bytes  = node_capacity_ * sizeof(FractalNode);
    const uint64_t seed_bytes  = seed_capacity_ * sizeof(QuantumSeed);
    const uint64_t total_bytes = node_bytes + seed_bytes + scratch_size_;

    node_slab_   = nullptr;
    seed_slab_   = nullptr;
    scratch_base_ = nullptr;

#if defined(VN_USE_CUDA_QUANTUM) && defined(__CUDACC__)
    // CUDA UVM allocation
    cudaError_t err = cudaMallocManaged(&arena_, total_bytes);
    if (err == cudaSuccess) {
        // Prefer the highest-performance GPU device
        cudaGetDevice(&gpu_device_);
        // Set UVM migration policy for this allocation
        cudaMemAdvise(arena_, total_bytes, cudaMemAdviseSetPreferredLocation,
                      gpu_device_);
        cudaMemAdvise(arena_, total_bytes, cudaMemAdviseSetAccessedBy,
                      cudaCpuDeviceId);
    } else {
        // Fall back to CPU allocation
        arena_ = nullptr;
    }
#endif

    if (!arena_) {
#ifdef _WIN32
        arena_ = static_cast<uint8_t*>(
            VirtualAlloc(NULL, total_bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
#else
        if (posix_memalign(reinterpret_cast<void**>(&arena_),
                           4096, total_bytes) != 0) {
            arena_ = nullptr;
        }
#endif
        if (!arena_) return false;
    }

    std::memset(arena_, 0, total_bytes);
    arena_size_ = total_bytes;

    // Partition arena
    node_slab_    = reinterpret_cast<FractalNode*>(arena_);
    seed_slab_    = reinterpret_cast<QuantumSeed*>(arena_ + node_bytes);
    scratch_base_ = arena_ + node_bytes + seed_bytes;

    node_count_     = 0;
    seed_count_     = 0;
    scratch_offset_ = 0;

    return true;
}

inline void ArenaAllocator::shutdown() {
    if (!arena_) return;

#if defined(VN_USE_CUDA_QUANTUM) && defined(__CUDACC__)
    cudaError_t err = cudaFree(arena_);
    if (err == cudaSuccess) {
        arena_ = nullptr;
        node_slab_ = nullptr;
        seed_slab_ = nullptr;
        scratch_base_ = nullptr;
        return;
    }
#endif

#ifdef _WIN32
    fflush(stdout);
    VirtualFree(arena_, 0, MEM_RELEASE);
#else
    std::free(arena_);
#endif
    arena_ = nullptr;
    node_slab_   = nullptr;
    seed_slab_   = nullptr;
    scratch_base_ = nullptr;
}

inline FractalNode* ArenaAllocator::alloc_node() {
    if (!arena_) return nullptr;

    std::lock_guard<std::mutex> lock(mutex_);
    if (node_count_ >= node_capacity_) return nullptr;

    FractalNode* node = &node_slab_[node_count_++];
    std::memset(node, 0, sizeof(FractalNode));
    node->uid = reinterpret_cast<uint64_t>(node);
    node->depth = 0;
    return node;
}

inline QuantumSeed* ArenaAllocator::alloc_seed() {
    if (!arena_) return nullptr;

    std::lock_guard<std::mutex> lock(mutex_);
    if (seed_count_ >= seed_capacity_) return nullptr;

    QuantumSeed* seed = &seed_slab_[seed_count_++];
    std::memset(seed, 0, sizeof(QuantumSeed));
    return seed;
}

inline void* ArenaAllocator::scratch_alloc(uint64_t size) {
    if (!arena_ || size == 0) return nullptr;

    std::lock_guard<std::mutex> lock(mutex_);
    // Ring-buffer wrap-around
    if (scratch_offset_ + size > scratch_size_) {
        scratch_offset_ = 0;
    }
    void* ptr = scratch_base_ + scratch_offset_;
    scratch_offset_ += size;
    return ptr;
}

inline void ArenaAllocator::scratch_reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    scratch_offset_ = 0;
}

inline FractalNode* ArenaAllocator::get_node(uint64_t index) const {
    if (!node_slab_ || index >= node_count_) return nullptr;
    return &node_slab_[index];
}

inline QuantumSeed* ArenaAllocator::get_seed(uint64_t index) const {
    if (!seed_slab_ || index >= seed_count_) return nullptr;
    return &seed_slab_[index];
}

inline FractalNode* ArenaAllocator::get_node_by_uid(uint64_t uid) const {
    if (!node_slab_) return nullptr;
    uintptr_t base = reinterpret_cast<uintptr_t>(node_slab_);
    uintptr_t target = static_cast<uintptr_t>(uid);
    if (target < base) return nullptr;
    uint64_t byte_offset = target - base;
    if (byte_offset % sizeof(FractalNode) != 0) return nullptr;
    uint64_t index = byte_offset / sizeof(FractalNode);
    if (index >= node_count_) return nullptr;
    return &node_slab_[index];
}

inline void ArenaAllocator::prefetch_to_device(uint64_t node_index, uint64_t count) {
#if defined(VN_USE_CUDA_QUANTUM) && defined(__CUDACC__)
    if (!arena_) return;
    if (node_index + count > node_capacity_) return;
    uint64_t offset = node_index * sizeof(FractalNode);
    uint64_t size   = count * sizeof(FractalNode);
    cudaMemPrefetchAsync(arena_ + offset, size, gpu_device_, nullptr);
#endif
}

inline void ArenaAllocator::prefetch_all_to_device() {
#if defined(VN_USE_CUDA_QUANTUM) && defined(__CUDACC__)
    if (!arena_) return;
    cudaMemPrefetchAsync(arena_, arena_size_, gpu_device_, nullptr);
#endif
}

inline ArenaAllocator::Stats ArenaAllocator::get_stats() const {
    Stats s{};
    const uint64_t node_bytes = node_capacity_ * sizeof(FractalNode);
    const uint64_t seed_bytes = seed_capacity_ * sizeof(QuantumSeed);
    s.arena_bytes        = arena_size_;
    s.node_bytes_used    = node_count_ * sizeof(FractalNode);
    s.seed_bytes_used    = seed_count_ * sizeof(QuantumSeed);
    s.scratch_bytes_used = scratch_offset_;
    s.node_utilization_pct = node_capacity_ > 0
        ? (node_count_ * 100) / node_capacity_ : 0;
    s.seed_utilization_pct = seed_capacity_ > 0
        ? (seed_count_ * 100) / seed_capacity_ : 0;
    return s;
}

} // namespace fll
} // namespace vn
