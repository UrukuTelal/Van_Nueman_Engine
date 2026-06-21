#pragma once

#include "BCCIndex.h"
#include <vector>
#include <cstdint>

inline std::vector<BCCCoord> bcc_interior_children(BCCCoord parent, int parent_size_exponent) {
    std::vector<BCCCoord> children;
    children.reserve(8);

    BCCCoord base = bcc_child(parent, 1);

    for (int di = 0; di <= 1; ++di) {
        for (int dj = 0; dj <= 1; ++dj) {
            for (int dk = 0; dk <= 1; ++dk) {
                BCCCoord child;
                child.i = base.i + di;
                child.j = base.j + dj;
                child.k = base.k + dk;
                child.l = (child.i + child.j + child.k) & 1;
                children.push_back(child);
            }
        }
    }

    return children;
}

inline std::vector<BCCCoord> bcc_interior_fill_depth(BCCCoord root, int depth) {
    std::vector<BCCCoord> result;
    if (depth <= 0) {
        result.push_back(root);
        return result;
    }

    auto children = bcc_interior_children(root, 0);
    for (const auto& child : children) {
        auto sub = bcc_interior_fill_depth(child, depth - 1);
        result.insert(result.end(), sub.begin(), sub.end());
    }

    return result;
}

inline int64_t bcc_estimate_interior_count(int levels) {
    if (levels <= 0) return 1;
    int64_t count = 1;
    for (int i = 0; i < levels; ++i) {
        count *= 8;
    }
    return count;
}

inline vn::fp20_t bcc_interior_volume_ratio(BCCCoord parent, int levels) {
    if (levels <= 0) return vn::fp20_t(0);
    vn::fp20_t inv = vn::fp20_t(1);
    for (int i = 0; i < levels; ++i) {
        inv = inv / vn::fp20_t(8);
    }
    return vn::fp20_t(1) - inv;
}
