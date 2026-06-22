#pragma once

#include "marching_cubes.h"
#include <algorithm>
#include <unordered_map>
#include <cfloat>

// ── Vertex hash for deduplication ─────────────────────────
// Two vertices are considered equal if their positions
// are within VNES_MERGE_EPSILON of each other.

constexpr float VNES_MERGE_EPSILON = 0.001f;

struct VNESVertexHash {
    size_t operator()(const vnes_vec3& v) const {
        // Quantize to merge-epsilon grid
        int ix = (int)(v.x / VNES_MERGE_EPSILON + 0.5f);
        int iy = (int)(v.y / VNES_MERGE_EPSILON + 0.5f);
        int iz = (int)(v.z / VNES_MERGE_EPSILON + 0.5f);

        size_t h1 = std::hash<int>{}(ix);
        size_t h2 = std::hash<int>{}(iy);
        size_t h3 = std::hash<int>{}(iz);

        // Combine hashes (xor + shift)
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

struct VNESVertexEqual {
    bool operator()(const vnes_vec3& a, const vnes_vec3& b) const {
        return std::fabs(a.x - b.x) < VNES_MERGE_EPSILON &&
               std::fabs(a.y - b.y) < VNES_MERGE_EPSILON &&
               std::fabs(a.z - b.z) < VNES_MERGE_EPSILON;
    }
};

// ── Vertex deduplication ──────────────────────────────────
// Merges vertices that are within epsilon of each other and
// rebuilds index buffer.

inline void mesh_deduplicate(VNESMesh& mesh) {
    std::unordered_map<vnes_vec3, uint32_t, VNESVertexHash, VNESVertexEqual> vert_map;
    std::vector<vnes_vec3> new_verts;
    std::vector<vnes_vec3> new_normals;
    std::vector<uint32_t> new_indices;

    // Remap old vertices to new compacted indices
    for (uint32_t i = 0; i < (uint32_t)mesh.vertices.size(); i++) {
        auto it = vert_map.find(mesh.vertices[i]);
        if (it != vert_map.end()) {
            // Already have this vertex — accumulate normal
            uint32_t idx = it->second;
            new_normals[idx] = new_normals[idx] + mesh.normals[i];
        } else {
            uint32_t idx = (uint32_t)new_verts.size();
            vert_map[mesh.vertices[i]] = idx;
            new_verts.push_back(mesh.vertices[i]);
            new_normals.push_back(mesh.normals[i]);
        }
    }

    // Remap indices
    for (uint32_t i = 0; i < (uint32_t)mesh.indices.size(); i++) {
        auto it = vert_map.find(mesh.vertices[mesh.indices[i]]);
        new_indices.push_back(it->second);
    }

    // Normalize accumulated normals
    for (uint32_t i = 0; i < (uint32_t)new_normals.size(); i++) {
        float len = length(new_normals[i]);
        if (len > 1e-8f)
            new_normals[i] = new_normals[i] * (1.0f / len);
    }

    mesh.vertices = std::move(new_verts);
    mesh.normals = std::move(new_normals);
    mesh.indices = std::move(new_indices);
}

// ── Remove degenerate triangles ───────────────────────────
// Triangles with near-zero area (colinear vertices).

inline void mesh_remove_degenerate(VNESMesh& mesh) {
    std::vector<uint32_t> clean_indices;
    clean_indices.reserve(mesh.indices.size());

    for (uint32_t i = 0; i < mesh.triangle_count(); i++) {
        uint32_t i0 = mesh.indices[i * 3];
        uint32_t i1 = mesh.indices[i * 3 + 1];
        uint32_t i2 = mesh.indices[i * 3 + 2];

        vnes_vec3 e1 = mesh.vertices[i1] - mesh.vertices[i0];
        vnes_vec3 e2 = mesh.vertices[i2] - mesh.vertices[i0];
        float area = length(cross(e1, e2)) * 0.5f;

        // Keep triangle if area > 0 and no duplicated indices
        if (area > 1e-10f && i0 != i1 && i1 != i2 && i0 != i2) {
            clean_indices.push_back(i0);
            clean_indices.push_back(i1);
            clean_indices.push_back(i2);
        }
    }

    mesh.indices = std::move(clean_indices);
}

// ── Full cleanup pipeline ─────────────────────────────────

inline void mesh_cleanup(VNESMesh& mesh) {
    mesh_deduplicate(mesh);
    mesh_remove_degenerate(mesh);
}
