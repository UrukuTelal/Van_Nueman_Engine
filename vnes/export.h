#pragma once

#include "mesh_cleanup.h"
#include <cstdio>
#include <cstring>

// ── STL (binary) export ───────────────────────────────────
// Writes a binary STL file. Format:
//   80-byte header
//   uint32_t triangle_count
//   foreach triangle: vec3 normal, vec3 v0, vec3 v1, vec3 v2, uint16_t attribute

inline bool export_stl_binary(const VNESMesh& mesh, const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) return false;

    uint32_t tri_count = mesh.triangle_count();

    // Header (80 bytes, padded with zeros)
    char header[80] = {};
    snprintf(header, 80, "VNES Van Neumann Ecosystem Substrate");
    fwrite(header, 1, 80, f);

    // Triangle count
    fwrite(&tri_count, sizeof(uint32_t), 1, f);

    // Triangles
    for (uint32_t i = 0; i < tri_count; i++) {
        uint32_t i0 = mesh.indices[i * 3];
        uint32_t i1 = mesh.indices[i * 3 + 1];
        uint32_t i2 = mesh.indices[i * 3 + 2];

        // Face normal (average of vertex normals, or recompute)
        vnes_vec3 fn = mesh.normals[i0] + mesh.normals[i1] + mesh.normals[i2];
        float len = length(fn);
        if (len > 1e-8f) fn = fn * (1.0f / len);
        else fn = {0, 1, 0};

        float tri_data[12] = {
            fn.x, fn.y, fn.z,
            mesh.vertices[i0].x, mesh.vertices[i0].y, mesh.vertices[i0].z,
            mesh.vertices[i1].x, mesh.vertices[i1].y, mesh.vertices[i1].z,
            mesh.vertices[i2].x, mesh.vertices[i2].y, mesh.vertices[i2].z,
        };

        fwrite(tri_data, sizeof(float), 12, f);

        // Attribute byte count (unused)
        uint16_t attr = 0;
        fwrite(&attr, sizeof(uint16_t), 1, f);
    }

    fclose(f);
    return true;
}

// ── STL (ASCII) export ────────────────────────────────────
// Human-readable STL format. Useful for debugging.

inline bool export_stl_ascii(const VNESMesh& mesh, const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) return false;

    fprintf(f, "solid vnes\n");

    for (uint32_t i = 0; i < mesh.triangle_count(); i++) {
        uint32_t i0 = mesh.indices[i * 3];
        uint32_t i1 = mesh.indices[i * 3 + 1];
        uint32_t i2 = mesh.indices[i * 3 + 2];

        vnes_vec3 fn = mesh.normals[i0] + mesh.normals[i1] + mesh.normals[i2];
        float len = length(fn);
        if (len > 1e-8f) fn = fn * (1.0f / len);
        else fn = {0, 1, 0};

        fprintf(f, "  facet normal %e %e %e\n", fn.x, fn.y, fn.z);
        fprintf(f, "    outer loop\n");
        fprintf(f, "      vertex %e %e %e\n",
                mesh.vertices[i0].x, mesh.vertices[i0].y, mesh.vertices[i0].z);
        fprintf(f, "      vertex %e %e %e\n",
                mesh.vertices[i1].x, mesh.vertices[i1].y, mesh.vertices[i1].z);
        fprintf(f, "      vertex %e %e %e\n",
                mesh.vertices[i2].x, mesh.vertices[i2].y, mesh.vertices[i2].z);
        fprintf(f, "    endloop\n");
        fprintf(f, "  endfacet\n");
    }

    fprintf(f, "endsolid vnes\n");
    fclose(f);
    return true;
}

// ── PLY (ASCII) export ────────────────────────────────────
// Vertex colors are not populated by marching cubes (no
// texture data), but the format supports them.

inline bool export_ply(const VNESMesh& mesh, const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) return false;

    uint32_t vert_count = (uint32_t)mesh.vertices.size();
    uint32_t tri_count = mesh.triangle_count();

    fprintf(f, "ply\n");
    fprintf(f, "format ascii 1.0\n");
    fprintf(f, "comment VNES Van Neumann Ecosystem Substrate\n");
    fprintf(f, "element vertex %u\n", vert_count);
    fprintf(f, "property float x\n");
    fprintf(f, "property float y\n");
    fprintf(f, "property float z\n");
    fprintf(f, "property float nx\n");
    fprintf(f, "property float ny\n");
    fprintf(f, "property float nz\n");
    fprintf(f, "element face %u\n", tri_count);
    fprintf(f, "property list uchar int vertex_indices\n");
    fprintf(f, "end_header\n");

    for (uint32_t i = 0; i < vert_count; i++) {
        fprintf(f, "%e %e %e %e %e %e\n",
                mesh.vertices[i].x, mesh.vertices[i].y, mesh.vertices[i].z,
                mesh.normals[i].x, mesh.normals[i].y, mesh.normals[i].z);
    }

    for (uint32_t i = 0; i < tri_count; i++) {
        uint32_t i0 = mesh.indices[i * 3];
        uint32_t i1 = mesh.indices[i * 3 + 1];
        uint32_t i2 = mesh.indices[i * 3 + 2];
        fprintf(f, "3 %u %u %u\n", i0, i1, i2);
    }

    fclose(f);
    return true;
}

// ── Format auto-detect ────────────────────────────────────
// Chooses writer based on filename extension.
// Supported: .stl (binary), .ascii.stl (ASCII), .ply

inline bool export_mesh(const VNESMesh& mesh, const char* filename) {
    size_t len = strlen(filename);

    // Check for .ascii.stl (ASCII override)
    if (len > 10 && strcmp(filename + len - 10, ".ascii.stl") == 0) {
        // Strip .ascii for actual filename, write ASCII
        char ascii_name[1024];
        if (len >= 1024) return false;
        memcpy(ascii_name, filename, len - 10);
        ascii_name[len - 10] = '\0';
        strcat(ascii_name, ".stl");
        return export_stl_ascii(mesh, ascii_name);
    }

    if (len > 4) {
        const char* ext = filename + len - 4;
        if (strcmp(ext, ".stl") == 0)
            return export_stl_binary(mesh, filename);
        if (strcmp(ext, ".ply") == 0)
            return export_ply(mesh, filename);
    }

    // Default: binary STL
    return export_stl_binary(mesh, filename);
}
