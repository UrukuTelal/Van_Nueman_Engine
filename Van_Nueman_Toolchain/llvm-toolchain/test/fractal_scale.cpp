// Test: Fractal scale generation
// Tests that [[fractal]] attribute generates multiple scale versions

#include "../../include/vn/PillarTypes.h"
#include "../../include/vn/Fractal.h"

using namespace vn;

// Base pillar state for different scales
struct EntityState {
  PillarStateVector pillars;
  float3 position;
};

struct ServerState {
  PillarStateVector pillars;
  uint32_t player_count;
};

struct FederationState {
  PillarStateVector pillars;
  uint32_t server_count;
};

// This function should be generated for all 3 scales
[[fractal]]
void update_pillars(void* state, FractalScale scale) {
  switch (scale) {
    case FractalScale::Entity: {
      EntityState* es = static_cast<EntityState*>(state);
      // Update entity pillars
      es->pillars[vn::PillarIndex::Awareness] = vn::fp20_t(0.7f);
      break;
    }
    case FractalScale::Server: {
      ServerState* ss = static_cast<ServerState*>(state);
      // Aggregate entity pillars to server
      break;
    }
    case FractalScale::Federation: {
      FederationState* fs = static_cast<FederationState*>(state);
      // Aggregate server pillars to federation
      break;
    }
  }
}

int main() {
  EntityState es;
  ServerState ss;
  FederationState fs;

  update_pillars(&es, FractalScale::Entity);
  update_pillars(&ss, FractalScale::Server);
  update_pillars(&fs, FractalScale::Federation);

  return 0;
}
