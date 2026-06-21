#pragma once

#include "../vn/ScaledInt.h"
#include <cstdint>

constexpr uint32_t TRUNC_OCT_VERTICES = 24;
constexpr uint32_t TRUNC_OCT_FACES = 14;
constexpr uint32_t TRUNC_OCT_SQUARE_FACES = 6;
constexpr uint32_t TRUNC_OCT_HEX_FACES = 8;

struct TruncOctVertex {
    vn::fp20_t x, y, z;
    vn::fp20_t rest_x, rest_y, rest_z;
};

struct TruncOctFace {
    uint32_t vertex_count;
    uint32_t vertex_indices[6];
    uint32_t pyramid_vertex;
};

struct TruncatedOctahedron {
    TruncOctVertex vertices[TRUNC_OCT_VERTICES];
    TruncOctFace faces[TRUNC_OCT_FACES];
    uint32_t square_face_indices[TRUNC_OCT_SQUARE_FACES];
    uint32_t hex_face_indices[TRUNC_OCT_HEX_FACES];
    vn::fp20_t center_x, center_y, center_z;
    uint32_t material_id;
};

inline void init_truncated_octahedron(TruncatedOctahedron& cell, vn::fp20_t size) {
    vn::fp20_t a = size;
    vn::fp20_t b = size * vn::fp20_t(0.5f);

    vn::fp20_t verts[24][3] = {
        { a,  b, vn::fp20_t(0) }, { a, -b, vn::fp20_t(0) }, {-a,  b, vn::fp20_t(0) }, {-a, -b, vn::fp20_t(0) },
        { b, vn::fp20_t(0),  a }, { b, vn::fp20_t(0), -a }, {-b, vn::fp20_t(0),  a }, {-b, vn::fp20_t(0), -a },
        { vn::fp20_t(0),  a,  b }, { vn::fp20_t(0),  a, -b }, { vn::fp20_t(0), -a,  b }, { vn::fp20_t(0), -a, -b },
        { a,  a,  a }, { a,  a, -a }, { a, -a,  a }, { a, -a, -a },
        {-a,  a,  a }, {-a,  a, -a }, {-a, -a,  a }, {-a, -a, -a },
        { b,  b,  b }, { b,  b, -b }, { b, -b,  b }, { b, -b, -b },
    };

    for (uint32_t i = 0; i < 24; i++) {
        cell.vertices[i].x = verts[i][0];
        cell.vertices[i].y = verts[i][1];
        cell.vertices[i].z = verts[i][2];
        cell.vertices[i].rest_x = verts[i][0];
        cell.vertices[i].rest_y = verts[i][1];
        cell.vertices[i].rest_z = verts[i][2];
    }

    cell.center_x = vn::fp20_t(0);
    cell.center_y = vn::fp20_t(0);
    cell.center_z = vn::fp20_t(0);
    cell.material_id = 0;

    uint32_t sq[6][4] = {
        {0,1,3,2}, {4,5,7,6}, {8,9,11,10},
        {12,13,15,14}, {16,17,19,18}, {20,21,23,22}
    };
    uint32_t he[8][6] = {
        {0,12,4,14,1,13}, {2,16,6,18,3,17},
        {0,13,5,15,1,12}, {2,17,7,19,3,16},
        {4,12,2,16,6,14}, {5,13,3,17,7,15},
        {4,14,1,15,5,13}, {6,18,0,12,4,14}
    };

    for (uint32_t i = 0; i < 6; i++) {
        cell.square_face_indices[i] = i;
        cell.faces[i].vertex_count = 4;
        for (uint32_t j = 0; j < 4; j++) cell.faces[i].vertex_indices[j] = sq[i][j];
        cell.faces[i].pyramid_vertex = i;
    }
    for (uint32_t i = 0; i < 8; i++) {
        cell.hex_face_indices[i] = 6 + i;
        cell.faces[6 + i].vertex_count = 6;
        for (uint32_t j = 0; j < 6; j++) cell.faces[6 + i].vertex_indices[j] = he[i][j];
        cell.faces[6 + i].pyramid_vertex = 6 + i;
    }
}
