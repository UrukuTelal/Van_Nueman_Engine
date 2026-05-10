#ifndef VN_PILLAR_TYPES_H
#define VN_PILLAR_TYPES_H

#include <cstdint>
#include <cmath>

// Van Nueman custom types for vncc compiler
// These are simplified versions for regular compilation

typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
typedef float float32;
typedef double float64;

// Scaled integer types (used by vncc)
struct ScaledInt32 {
    int32 value;
    float to_float() const { return value / 4294967296.0f; }
    void from_float(float f) { value = (int32)(f * 4294967296.0f); }
};

// 3D vector types
struct float3 {
    float x, y, z;
    float3() : x(0), y(0), z(0) {}
    float3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    
    float3 operator+(const float3& o) const { return float3(x+o.x, y+o.y, z+o.z); }
    float3 operator-(const float3& o) const { return float3(x-o.x, y-o.y, z-o.z); }
    float3 operator*(float s) const { return float3(x*s, y*s, z*s); }
};

struct uint3 {
    uint32_t x, y, z;
    uint3() : x(0), y(0), z(0) {}
    uint3(uint32_t x_, uint32_t y_, uint32_t z_) : x(x_), y(y_), z(z_) {}
};

// Helper functions
inline float3 make_float3(float x, float y, float z) { return float3(x, y, z); }
inline float3 float3_normalize(float3 v) {
    float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len < 0.0001f) return v;
    return float3(v.x/len, v.y/len, v.z/len);
}
inline float fminf(float a, float b) { return a < b ? a : b; }
inline float fmaxf(float a, float b) { return a > b ? a : b; }

// Use std::fabs from <cmath> instead of defining fabsf
using std::fabs;  // Use std::fabs directly

// PillarVector is now defined in include/Entity.h (using std::array<float, 16>)
// This typedef removed to avoid redefinition conflict with Entity.h

// Note: ScaledInt template is defined in vn/ScaledInt.h
// Include that file separately if needed

#endif // VN_PILLAR_TYPES_H
