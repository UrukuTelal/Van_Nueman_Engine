#ifndef VN_PILLAR_TYPES_H
#define VN_PILLAR_TYPES_H

#include <cstdint>
#include <array>

namespace vn {

// 14 Pillar indices (must match FULL_ARCHITECTURE.md)
enum PillarIndex : uint32_t {
  Awareness = 0,
  Willpower = 1,
  Force = 2,
  Influence = 3,
  Resistance = 4,
  Integrity = 5,
  Cohesion = 6,
  Relation = 7,
  Presence = 8,
  Warmth = 9,
  Memory = 10,
  Attraction = 11,
  Harm = 12,
  Distortion = 13,
  Flux = 14,
  Depth = 15,
  NumPillars = 16
};

// Scaled integer factor: 1 = 2^20
constexpr int32_t SCALE_FACTOR = 1048576;  // 2^20

// Convert float to scaled integer
inline int32_t to_scaled(float f) {
  return static_cast<int32_t>(f * SCALE_FACTOR);
}

// Convert scaled integer to float
inline float from_scaled(int32_t i) {
  return static_cast<float>(i) / SCALE_FACTOR;
}

// Pillar State Vector (14 values per entity)
// This is the core type that [[pillar_vector]] attribute applies to
struct __attribute__((aligned(16))) PillarStateVector {
  int32_t p[NumPillars];  // scaled integers

  int32_t& operator[](PillarIndex idx) { return p[static_cast<size_t>(idx)]; }
  const int32_t& operator[](PillarIndex idx) const { return p[static_cast<size_t>(idx)]; }

  // Helper methods
  void set(PillarIndex idx, float value) { p[static_cast<size_t>(idx)] = to_scaled(value); }
  float get(PillarIndex idx) const { return from_scaled(p[static_cast<size_t>(idx)]); }
};

// Entity types from architecture spec
enum class EntityType : uint32_t {
  Celestial = 0,
  LivingPlanet = 1,
  Drone = 2,
  Robot = 3,
  Creature = 4,
  Human = 5,
  Server = 6,
  Federation = 7
};

} // namespace vn

#endif // VN_PILLAR_TYPES_H
