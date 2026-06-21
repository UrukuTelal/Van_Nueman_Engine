#pragma once

#include "TruncatedOctahedron.h"
#include "TetradSubCell.h"
#include "BCCIndex.h"
#include "YieldMatrix.h"
#include <cstdint>

struct VoxelCell {
    TruncatedOctahedron geometry;
    TetradSubCell pyramids[TRUNC_OCT_FACES];
    YieldMatrix material;
    BCCCoord coord;
    uint32_t cell_id;
    bool active;
    bool dirty;

    static constexpr uint32_t FACE_COUNT = TRUNC_OCT_FACES;

    VoxelCell()
        : cell_id(0), active(false), dirty(false)
    {
        coord = {0, 0, 0, 0};
    }

    void init(const BCCCoord& bcc, vn::fp20_t size, uint32_t id, const YieldMatrix& mat) {
        coord = bcc;
        cell_id = id;
        material = mat;
        active = true;
        dirty = true;

        init_truncated_octahedron(geometry, size);

        vn::fp20_t sub_vol = size * size * size / vn::fp20_t(FACE_COUNT);
        for (uint32_t i = 0; i < FACE_COUNT; i++) {
            pyramids[i] = TetradSubCell();
            pyramids[i].face_index = i;
            pyramids[i].volume = sub_vol;
        }
    }

    void mark_dirty() { dirty = true; }
    bool is_dirty() const { return dirty; }
    void clear_dirty() { dirty = false; }
};
