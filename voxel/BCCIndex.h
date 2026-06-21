#pragma once

#include "../vn/ScaledInt.h"
#include <cstdint>

struct BCCCoord {
    int32_t i, j, k, l;
};

inline BCCCoord bcc_from_cartesian(int32_t x, int32_t y, int32_t z) {
    BCCCoord c;
    c.i = x;
    c.j = y;
    c.k = z;
    c.l = (x + y + z) & 1;
    return c;
}

inline BCCCoord bcc_parent(const BCCCoord& child, int32_t scale_delta) {
    int32_t shift = scale_delta;
    if (shift < 0) shift = 0;
    BCCCoord p;
    p.i = child.i >> shift;
    p.j = child.j >> shift;
    p.k = child.k >> shift;
    p.l = child.l;
    return p;
}

inline BCCCoord bcc_child(const BCCCoord& parent, int32_t scale_delta) {
    int32_t shift = scale_delta;
    if (shift < 0) shift = 0;
    BCCCoord c;
    c.i = parent.i << shift;
    c.j = parent.j << shift;
    c.k = parent.k << shift;
    c.l = parent.l;
    return c;
}

inline BCCCoord bcc_scale_down(const BCCCoord& coord, int32_t levels) {
    BCCCoord c;
    c.i = coord.i >> levels;
    c.j = coord.j >> levels;
    c.k = coord.k >> levels;
    c.l = coord.l;
    return c;
}

inline BCCCoord bcc_scale_up(const BCCCoord& coord, int32_t levels) {
    BCCCoord c;
    c.i = coord.i << levels;
    c.j = coord.j << levels;
    c.k = coord.k << levels;
    c.l = coord.l;
    return c;
}

inline BCCCoord bcc_face_neighbor(const BCCCoord& coord, uint32_t face_index) {
    BCCCoord n = coord;
    switch (face_index) {
        // 6 square faces (0-5): axial offsets
        case 0:  n.i += 1; n.l = (n.i + n.j + n.k) & 1; break;
        case 1:  n.i -= 1; n.l = (n.i + n.j + n.k) & 1; break;
        case 2:  n.j += 1; n.l = (n.i + n.j + n.k) & 1; break;
        case 3:  n.j -= 1; n.l = (n.i + n.j + n.k) & 1; break;
        case 4:  n.k += 1; n.l = (n.i + n.j + n.k) & 1; break;
        case 5:  n.k -= 1; n.l = (n.i + n.j + n.k) & 1; break;
        // 8 hexagonal faces (6-13): diagonal offsets
        case 6:  n.i += 1; n.j += 1; n.l = (n.i + n.j + n.k) & 1; break;
        case 7:  n.i += 1; n.j -= 1; n.l = (n.i + n.j + n.k) & 1; break;
        case 8:  n.i -= 1; n.j += 1; n.l = (n.i + n.j + n.k) & 1; break;
        case 9:  n.i -= 1; n.j -= 1; n.l = (n.i + n.j + n.k) & 1; break;
        case 10: n.i += 1; n.k += 1; n.l = (n.i + n.j + n.k) & 1; break;
        case 11: n.i += 1; n.k -= 1; n.l = (n.i + n.j + n.k) & 1; break;
        case 12: n.i -= 1; n.k += 1; n.l = (n.i + n.j + n.k) & 1; break;
        case 13: n.i -= 1; n.k -= 1; n.l = (n.i + n.j + n.k) & 1; break;
    }
    return n;
}

inline uint64_t bcc_hash(const BCCCoord& coord) {
    uint64_t h = 14695981039346656037ULL;
    h ^= static_cast<uint64_t>(coord.i);
    h *= 1099511628211ULL;
    h ^= static_cast<uint64_t>(coord.j);
    h *= 1099511628211ULL;
    h ^= static_cast<uint64_t>(coord.k);
    h *= 1099511628211ULL;
    h ^= static_cast<uint64_t>(coord.l);
    h *= 1099511628211ULL;
    return h;
}

inline uint64_t bcc_bitmask_key(const BCCCoord& coord, int32_t max_bits) {
    uint64_t key = 0;
    int32_t mask = (1 << max_bits) - 1;
    key |= (static_cast<uint64_t>(coord.i & mask));
    key |= (static_cast<uint64_t>(coord.j & mask)) << max_bits;
    key |= (static_cast<uint64_t>(coord.k & mask)) << (max_bits * 2);
    key |= (static_cast<uint64_t>(coord.l & 1)) << (max_bits * 3);
    return key;
}

inline bool bcc_equal(const BCCCoord& a, const BCCCoord& b) {
    return a.i == b.i && a.j == b.j && a.k == b.k && a.l == b.l;
}
