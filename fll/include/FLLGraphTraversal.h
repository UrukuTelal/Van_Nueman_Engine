#pragma once
#include "FractalNode.h"
#include "ArenaAllocator.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>
#include <thread>
#include <algorithm>

namespace vn {
namespace fll {

// ── Traversal Order ──────────────────────────────────────────────────
enum class TraversalOrder : uint8_t {
    BFS,             // breadth-first, level by level
    DFS_PREORDER,    // parent before children
    DFS_POSTORDER,   // children before parent
    DEPTH_SORTED,    // ascending depth (stable for render order)
};

// ── Traversal Filter ─────────────────────────────────────────────────
struct TraversalFilter {
    uint32_t min_depth       = 0;
    uint32_t max_depth       = UINT32_MAX;
    uint32_t require_flags   = 0;   // all bits must be set
    uint32_t exclude_flags   = 0;   // any bit set → skip
    bool     cull_by_zoom    = false;
    float    zoom_level      = 1.0f;

    bool passes(const FractalNode& node) const {
        if (node.depth < min_depth || node.depth > max_depth)
            return false;
        if (require_flags && (node.flags & require_flags) != require_flags)
            return false;
        if (exclude_flags && (node.flags & exclude_flags))
            return false;
        if (cull_by_zoom) {
            float s = depth_to_scale(node.depth);
            float zs = zoom_level * s;
            if (zs < 0.1f || zs > 10.0f)
                return false;
            if (node.zoom_threshold_min > 0.0f && zoom_level < node.zoom_threshold_min)
                return false;
            if (node.zoom_threshold_max > 0.0f && zoom_level > node.zoom_threshold_max)
                return false;
        }
        return true;
    }
};

// ── Make a zoom-based filter ─────────────────────────────────────────
inline TraversalFilter make_zoom_filter(float zoom_level,
                                         uint32_t range = 4) {
    float depth_f = std::log(zoom_level) / std::log(FLL_PHI);
    int32_t active = static_cast<int32_t>(std::round(depth_f));
    if (active < 0) active = 0;

    TraversalFilter f;
    f.cull_by_zoom = true;
    f.zoom_level   = zoom_level;
    f.min_depth    = (active > static_cast<int32_t>(range))
                     ? static_cast<uint32_t>(active - range) : 0;
    f.max_depth    = static_cast<uint32_t>(active + range);
    if (f.max_depth > FLL_MAX_DEPTH) f.max_depth = FLL_MAX_DEPTH;
    return f;
}

// ── Visitor-based Traversal ─────────────────────────────────────────
// Visitor signature: bool(ArenaAllocator&, FractalNode&)
// Return false to stop traversal early.
// Uses heap-allocated vectors to avoid stack overflow with deep trees.
template<typename Visitor>
void traverse(ArenaAllocator& arena, FractalNode* root,
              TraversalOrder order, Visitor&& visitor,
              const TraversalFilter& filter = TraversalFilter{})
{
    if (!root) return;

    std::vector<FractalNode*> stack;
    stack.reserve(4096);
    uint32_t head = 0;

    switch (order) {

    case TraversalOrder::BFS: {
        stack.push_back(root);
        while (head < stack.size()) {
            FractalNode* node = stack[head++];
            for (uint32_t i = 0; i < FractalNode::MAX_CHILDREN; i++) {
                uint64_t cuid = node->child_uids[i];
                if (cuid == 0) continue;
                FractalNode* child = arena.get_node_by_uid(cuid);
                if (child) stack.push_back(child);
            }
            if (!filter.passes(*node)) continue;
            if (!visitor(arena, *node)) return;
        }
        break;
    }

    case TraversalOrder::DFS_PREORDER: {
        stack.push_back(root);
        while (!stack.empty()) {
            FractalNode* node = stack.back();
            stack.pop_back();
            for (int32_t i = FractalNode::MAX_CHILDREN - 1; i >= 0; i--) {
                uint64_t cuid = node->child_uids[i];
                if (cuid == 0) continue;
                FractalNode* child = arena.get_node_by_uid(cuid);
                if (child) stack.push_back(child);
            }
            if (!filter.passes(*node)) continue;
            if (!visitor(arena, *node)) return;
        }
        break;
    }

    case TraversalOrder::DFS_POSTORDER: {
        std::vector<FractalNode*> result;
        result.reserve(4096);
        stack.push_back(root);
        while (!stack.empty()) {
            FractalNode* node = stack.back();
            stack.pop_back();
            result.push_back(node);
            for (uint32_t i = 0; i < FractalNode::MAX_CHILDREN; i++) {
                uint64_t cuid = node->child_uids[i];
                if (cuid == 0) continue;
                FractalNode* child = arena.get_node_by_uid(cuid);
                if (child) stack.push_back(child);
            }
        }
        for (int32_t i = static_cast<int32_t>(result.size()) - 1; i >= 0; i--) {
            if (!filter.passes(*result[i])) continue;
            if (!visitor(arena, *result[i])) return;
        }
        break;
    }

    case TraversalOrder::DEPTH_SORTED: {
        std::vector<FractalNode*> all;
        all.reserve(4096);
        stack.push_back(root);
        head = 0;
        while (head < stack.size()) {
            FractalNode* node = stack[head++];
            if (filter.passes(*node))
                all.push_back(node);
            for (uint32_t i = 0; i < FractalNode::MAX_CHILDREN; i++) {
                uint64_t cuid = node->child_uids[i];
                if (cuid == 0) continue;
                FractalNode* child = arena.get_node_by_uid(cuid);
                if (child) stack.push_back(child);
            }
        }
        for (uint32_t i = 1; i < all.size(); i++) {
            FractalNode* key = all[i];
            int32_t j = static_cast<int32_t>(i) - 1;
            while (j >= 0 && all[j]->depth > key->depth) {
                all[j + 1] = all[j];
                j--;
            }
            all[j + 1] = key;
        }
        for (uint32_t i = 0; i < all.size(); i++) {
            if (!visitor(arena, *all[i])) return;
        }
        break;
    }
    }
}

// ── Count nodes matching filter ─────────────────────────────────────
inline uint32_t count_nodes(ArenaAllocator& arena, FractalNode* root,
                             const TraversalFilter& filter = TraversalFilter{})
{
    uint32_t count = 0;
    traverse(arena, root, TraversalOrder::BFS,
             [&count](ArenaAllocator&, FractalNode&) { count++; return true; },
             filter);
    return count;
}

// ── Collect nodes into array ─────────────────────────────────────────
inline uint32_t collect_nodes(ArenaAllocator& arena, FractalNode* root,
                               FractalNode** output, uint32_t max_output,
                               TraversalOrder order = TraversalOrder::BFS,
                               const TraversalFilter& filter = TraversalFilter{})
{
    uint32_t count = 0;
    traverse(arena, root, order,
             [&](ArenaAllocator&, FractalNode& node) {
                 if (count < max_output) {
                     output[count++] = &node;
                     return true;
                 }
                 return false;
             },
             filter);
    return count;
}

// ── Find first node matching predicate ───────────────────────────────
template<typename Pred>
inline FractalNode* find_node(ArenaAllocator& arena, FractalNode* root,
                               Pred&& pred,
                               TraversalOrder order = TraversalOrder::BFS)
{
    FractalNode* found = nullptr;
    traverse(arena, root, order,
             [&](ArenaAllocator&, FractalNode& node) {
                 if (pred(node)) {
                     found = &node;
                     return false; // stop
                 }
                 return true;
             });
    return found;
}

// ── Check if any node satisfies predicate ────────────────────────────
template<typename Pred>
inline bool any_node(ArenaAllocator& arena, FractalNode* root, Pred&& pred) {
    return find_node(arena, root, std::forward<Pred>(pred)) != nullptr;
}

// ── Check if all nodes satisfy predicate ─────────────────────────────
template<typename Pred>
inline bool all_nodes(ArenaAllocator& arena, FractalNode* root, Pred&& pred) {
    bool ok = true;
    traverse(arena, root, TraversalOrder::BFS,
             [&](ArenaAllocator&, FractalNode& node) {
                 if (!pred(node)) { ok = false; return false; }
                 return true;
             });
    return ok;
}

// ── Build a manually-controlled test tree ────────────────────────────
// Creates a tree of depth `depth` with `branching` children per node,
// each labelled with metadata[0] = sequential index for test verification.
__declspec(noinline) FractalNode* build_test_tree(ArenaAllocator& arena,
                                     uint32_t depth,
                                     uint32_t branching)
{
    if (depth == 0 || branching == 0) return nullptr;
    if (branching > FractalNode::MAX_CHILDREN) branching = FractalNode::MAX_CHILDREN;

    FractalNode* root = arena.alloc_node();
    if (!root) return nullptr;
    root->depth = 0;
    root->flags = NODE_ACTIVE | NODE_RENDERABLE;

    uint64_t counter = 1;
    FractalNode* queue[4096];
    uint32_t head = 0, tail = 0;
    queue[tail++] = root;

    while (head < tail) {
        FractalNode* parent = queue[head++];
        if (parent->depth + 1 >= depth) continue;

        for (uint32_t i = 0; i < branching; i++) {
            FractalNode* child = arena.alloc_node();
            if (!child) break;
            child->uid = reinterpret_cast<uint64_t>(child);
            child->depth = parent->depth + 1;
            child->flags = NODE_ACTIVE | NODE_RENDERABLE;
            child->parent_uid = parent->uid;
            child->metadata[0] = static_cast<float>(counter++);
            parent->add_child(child->uid);

            if (tail < 4096)
                queue[tail++] = child;
        }
    }
    return root;
}

// ── Parallel Traversal (Multi-threaded) ─────────────────────────────
// Distributes root's immediate child subtrees across threads.
// Each thread runs a serial traverse on its assigned subtree(s).
// Useful for wide trees where root-level children are independent.
//
// The visitor is called with (arena, node) and should return true to
// continue or false to stop that thread's traversal early.
// Thread safety: the visitor must be thread-safe for concurrent calls.

template<typename Visitor>
void traverse_parallel(ArenaAllocator& arena, FractalNode* root,
                       TraversalOrder order, Visitor&& visitor,
                       const TraversalFilter& filter = TraversalFilter{},
                       uint32_t num_threads = 0)
{
    if (!root) return;

    if (num_threads == 0)
        num_threads = std::thread::hardware_concurrency();
    if (num_threads < 2) {
        traverse(arena, root, order, std::forward<Visitor>(visitor), filter);
        return;
    }

    // Collect root's immediate children as independent subtrees
    std::vector<FractalNode*> subtrees;
    for (uint32_t i = 0; i < FractalNode::MAX_CHILDREN; i++) {
        uint64_t cuid = root->child_uids[i];
        if (cuid == 0) continue;
        FractalNode* child = arena.get_node_by_uid(cuid);
        if (child) subtrees.push_back(child);
    }

    if (subtrees.empty()) {
        traverse(arena, root, order, std::forward<Visitor>(visitor), filter);
        return;
    }

    // Distribute subtrees across threads
    struct ThreadRange { uint32_t start; uint32_t end; };
    std::vector<ThreadRange> ranges(num_threads);
    uint32_t per_thread = (static_cast<uint32_t>(subtrees.size()) + num_threads - 1) / num_threads;
    for (uint32_t t = 0; t < num_threads; t++) {
        ranges[t].start = t * per_thread;
        ranges[t].end = std::min(ranges[t].start + per_thread,
                                 static_cast<uint32_t>(subtrees.size()));
    }

    std::vector<std::thread> threads;
    std::atomic<bool> any_stopped{false};
    for (uint32_t t = 0; t < num_threads; t++) {
        if (ranges[t].start >= ranges[t].end) break;
        threads.emplace_back([&, t]() {
            for (uint32_t i = ranges[t].start; i < ranges[t].end; i++) {
                if (any_stopped.load()) return;
                traverse(arena, subtrees[i], order,
                         [&](ArenaAllocator& a, FractalNode& node) {
                             if (any_stopped.load()) return false;
                             if (!visitor(a, node)) {
                                 any_stopped.store(true);
                                 return false;
                             }
                             return true;
                         }, filter);
            }
        });
    }

    for (auto& th : threads) th.join();

    // Visit root last (after all children processed)
    if (!any_stopped.load() && filter.passes(*root)) {
        visitor(arena, *root);
    }
}

// ── Parallel collect_nodes (Multi-threaded) ────────────────────────
// Distributes root's child subtrees across threads for parallel
// node collection. Nodes from subtrees are merged into output array.
inline uint32_t collect_nodes_parallel(ArenaAllocator& arena,
                                        FractalNode* root,
                                        FractalNode** output,
                                        uint32_t max_output,
                                        TraversalOrder order = TraversalOrder::BFS,
                                        const TraversalFilter& filter = TraversalFilter{},
                                        uint32_t num_threads = 0)
{
    if (!root) return 0;

    if (num_threads == 0)
        num_threads = std::thread::hardware_concurrency();
    if (num_threads < 2) {
        return collect_nodes(arena, root, output, max_output, order, filter);
    }

    // Collect root's immediate children as independent subtrees
    std::vector<FractalNode*> subtrees;
    for (uint32_t i = 0; i < FractalNode::MAX_CHILDREN; i++) {
        uint64_t cuid = root->child_uids[i];
        if (cuid == 0) continue;
        FractalNode* child = arena.get_node_by_uid(cuid);
        if (child) subtrees.push_back(child);
    }

    if (subtrees.empty()) {
        return collect_nodes(arena, root, output, max_output, order, filter);
    }

    // Allocate per-thread output buffers
    uint32_t per_thread_max = max_output / num_threads + 1;
    struct ThreadResult {
        std::vector<FractalNode*> nodes;
        std::mutex mutex;
    };
    std::vector<ThreadResult> results(num_threads);
    for (auto& r : results) r.nodes.reserve(per_thread_max);

    // Distribute subtrees across threads
    uint32_t per_subtree = (static_cast<uint32_t>(subtrees.size()) + num_threads - 1) / num_threads;
    std::vector<std::thread> threads;
    for (uint32_t t = 0; t < num_threads; t++) {
        uint32_t start = t * per_subtree;
        uint32_t end = std::min(start + per_subtree, static_cast<uint32_t>(subtrees.size()));
        if (start >= end) break;
        threads.emplace_back([&, t, start, end]() {
            for (uint32_t i = start; i < end; i++) {
                uint32_t remaining = per_thread_max - static_cast<uint32_t>(results[t].nodes.size());
                if (remaining == 0) break;
                FractalNode** buf = results[t].nodes.data() + results[t].nodes.size();
                uint32_t n = collect_nodes(arena, subtrees[i], buf, remaining, order, filter);
                results[t].nodes.resize(results[t].nodes.size() + n);
            }
        });
    }

    for (auto& th : threads) th.join();

    // Merge results into output
    uint32_t total = 0;
    if (filter.passes(*root)) {
        if (total < max_output) output[total++] = root;
    }
    for (uint32_t t = 0; t < num_threads; t++) {
        for (uint32_t i = 0; i < static_cast<uint32_t>(results[t].nodes.size()) && total < max_output; i++) {
            output[total++] = results[t].nodes[i];
        }
    }

    return total;
}

} // namespace fll
} // namespace vn
