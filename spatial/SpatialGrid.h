#pragma once

#include "../include/Entity.h"
#include "../vn/ScaledInt.h"
#include <cstdint>
#include <vector>
#include <cstring>
#include <algorithm>

// ── GPU-Resident Spatial Hash Table (Linear Probing) ────────
// Eliminates the O(n) CPU bottleneck by keeping the grid
// synchronised with the GPU-resident SVO raymarcher data.
// Uses a flat, power-of-two hash table with linear probing —
// no per-cell dynamic allocations, cache-line friendly.
//
// Shared data structure for:
//   - Physics spatial queries (collision, deformation)
//   - SVO raymarcher traversal (render_vulkan.comp)
//   - Agent proximity detection (cognition, cords)

template <uint32_t TABLE_POW2 = 14>  // 16,384 entries ~ 2x coverage at 5K entities
struct SpatialGridGPU {
    static constexpr uint32_t TABLE_SIZE = 1u << TABLE_POW2;
    static constexpr uint32_t TABLE_MASK = TABLE_SIZE - 1;
    static constexpr uint32_t MAX_PROBES = 16;
    static constexpr uint32_t EMPTY_KEY = 0xFFFFFFFFu;

    // GPU-compatible packed cell entry (16 bytes)
    struct Cell {
        uint32_t key;            // packed grid coord (8+8+8 = 24 bits)
        uint32_t entity_id;      // entity index in SoA arrays
        uint32_t next;           // probe offset for linear probing
        uint32_t _pad;
    };

    Cell table[TABLE_SIZE];
    uint32_t count;

    SpatialGridGPU() { clear(); }

    void clear() {
        for (uint32_t i = 0; i < TABLE_SIZE; i++) {
            table[i].key = EMPTY_KEY;
            table[i].entity_id = 0;
            table[i].next = 0;
        }
        count = 0;
    }

    // Pack (gx, gy, gz) into 24-bit key (8 bits per axis)
    static uint32_t pack_key(int32_t gx, int32_t gy, int32_t gz) {
        return (static_cast<uint32_t>(gz & 0xFF) << 16) |
               (static_cast<uint32_t>(gy & 0xFF) << 8)  |
               static_cast<uint32_t>(gx & 0xFF);
    }

    static uint32_t hash_key(uint32_t k) {
        // Thomas Wang integer hash for good distribution
        k = (k ^ 61) ^ (k >> 16);
        k = k + (k << 3);
        k = k ^ (k >> 4);
        k = k * 0x27d4eb2d;
        return k ^ (k >> 15);
    }

    void insert(uint32_t entity_id, vn::fp20_t x, vn::fp20_t y, vn::fp20_t z) {
        int32_t gx = static_cast<int32_t>(x.to_float() * 128.0f);
        int32_t gy = static_cast<int32_t>(y.to_float() * 128.0f);
        int32_t gz = static_cast<int32_t>(z.to_float() * 128.0f);
        if (gx < 0) gx = 0; if (gx > 127) gx = 127;
        if (gy < 0) gy = 0; if (gy > 127) gy = 127;
        if (gz < 0) gz = 0; if (gz > 127) gz = 127;

        uint32_t key = pack_key(gx, gy, gz);
        uint32_t h = hash_key(key) & TABLE_MASK;
        uint32_t probes = 0;

        while (probes < MAX_PROBES) {
            uint32_t idx = (h + probes) & TABLE_MASK;
            if (table[idx].key == EMPTY_KEY) {
                table[idx].key = key;
                table[idx].entity_id = entity_id;
                table[idx].next = 0;
                count++;
                return;
            }
            if (table[idx].key == key && table[idx].entity_id == entity_id) {
                return;  // already inserted
            }
            probes++;
        }
        // Table full (extremely rare) — overwrite farthest probe
        table[(h + MAX_PROBES - 1) & TABLE_MASK] = {key, entity_id, 0, 0};
    }

    // Query all entities in cells within range of (x,y,z)
    void query(vn::fp20_t x, vn::fp20_t y, vn::fp20_t z,
               uint32_t range, std::vector<uint32_t>& out) const {
        out.clear();
        int32_t gx = static_cast<int32_t>(x.to_float() * 128.0f);
        int32_t gy = static_cast<int32_t>(y.to_float() * 128.0f);
        int32_t gz = static_cast<int32_t>(z.to_float() * 128.0f);

        int32_t min_x = std::max(0, gx - static_cast<int32_t>(range));
        int32_t max_x = std::min(127, gx + static_cast<int32_t>(range));
        int32_t min_y = std::max(0, gy - static_cast<int32_t>(range));
        int32_t max_y = std::min(127, gy + static_cast<int32_t>(range));
        int32_t min_z = std::max(0, gz - static_cast<int32_t>(range));
        int32_t max_z = std::min(127, gz + static_cast<int32_t>(range));

        for (int32_t iz = min_z; iz <= max_z; iz++) {
            for (int32_t iy = min_y; iy <= max_y; iy++) {
                for (int32_t ix = min_x; ix <= max_x; ix++) {
                    uint32_t key = pack_key(ix, iy, iz);
                    uint32_t h = hash_key(key) & TABLE_MASK;
                    uint32_t probes = 0;
                    while (probes < MAX_PROBES) {
                        uint32_t idx = (h + probes) & TABLE_MASK;
                        if (table[idx].key == key) {
                            out.push_back(table[idx].entity_id);
                            break;
                        }
                        if (table[idx].key == EMPTY_KEY) break;
                        probes++;
                    }
                }
            }
        }
        // Deduplicate
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
    }

    // Rebuild from SoA arrays (GPU-friendly: single pass, no allocations)
    void rebuild(const float* xs, const float* ys, const float* zs,
                 const uint8_t* active, uint32_t entity_count) {
        clear();
        for (uint32_t i = 0; i < entity_count; i++) {
            if (active && !active[i]) continue;
            insert(i, vn::fp20_t(xs[i]), vn::fp20_t(ys[i]), vn::fp20_t(zs[i]));
        }
    }

    // Migrate data to/from GPU-compatible buffer
    // Returns pointer to flat table array for Vulkan SSBO upload
    const Cell* gpu_data() const { return table; }
    uint32_t gpu_data_size() const { return TABLE_SIZE * sizeof(Cell); }
};

// Alias for default-size grid used by QuantumBackend interface.
// Template parameter (128) accepted for backward compatibility with
// quantum interface signatures; actual table size is the POW2 default.
template <uint32_t>
using SpatialGrid = SpatialGridGPU<>;
