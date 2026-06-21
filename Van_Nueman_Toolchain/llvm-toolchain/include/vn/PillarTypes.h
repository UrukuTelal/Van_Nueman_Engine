#ifndef VN_PILLAR_TYPES_H
#define VN_PILLAR_TYPES_H

#include "ScaledInt.h"
#ifndef UGC_COMPILER
#include <cstdint>
#endif
#include <array>

namespace vn {

using fp20_t = vn::ScaledInt<int64_t, 20>;

// Auto-generated from pillars.yaml — DO NOT EDIT HERE
#include "PillarEnumUGC.h"

struct __attribute__((aligned(16))) PillarStateVector {
  fp20_t p[NumPillars];

  fp20_t& operator[](PillarIndex idx) { return p[static_cast<size_t>(idx)]; }
  const fp20_t& operator[](PillarIndex idx) const { return p[static_cast<size_t>(idx)]; }

  void set(PillarIndex idx, float value) { p[static_cast<size_t>(idx)] = fp20_t(value); }
  float get(PillarIndex idx) const { return p[static_cast<size_t>(idx)].to_float(); }
};

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
