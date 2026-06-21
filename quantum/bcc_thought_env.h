#pragma once

#include "../include/Entity.h"
#include "../include/HopfPID.h"
#include "../voxel/BCCIndex.h"
#include <cstring>
#include <cmath>
#include <vector>

namespace vn {
namespace quantum {

// ── BCCThoughtEnvironment ────────────────────────────────────
// A 3D spatial working memory for the cognitive pipeline.
// Uses the BCC lattice (i,j,k,l) as the coordinate system for
// an internal "thought environment" where the AI stores and
// retrieves pillar state patterns spatially.
//
// Key operations:
//   - store/load: read/write PSV at a BCC coordinate
//   - scatter: spread a PSV to neighboring cells (spatial
//     pattern diffusion)
//   - gather: collect nearby PSVs weighted by distance
//     (associative pattern completion)
//   - frame encode/decode: map between the 512D Hopf thought
//     vector and regions of BCC space

struct BCCThoughtEnvironment {
    static constexpr int ENV_SIZE  = 32;   // 32^3 grid
    static constexpr int NUM_CELLS = ENV_SIZE * ENV_SIZE * ENV_SIZE;

    // The cognitive map: PSV at each BCC coordinate (heap-allocated to avoid 2MB stack frame)
    std::vector<PillarStateVector> flat_grid;

    PillarStateVector& at(int i, int j, int k) { return flat_grid[(i * ENV_SIZE + j) * ENV_SIZE + k]; }
    const PillarStateVector& at(int i, int j, int k) const { return flat_grid[(i * ENV_SIZE + j) * ENV_SIZE + k]; }

    // Agent's current focus position in thought space
    BCCCoord agent_pos;

    BCCThoughtEnvironment() : flat_grid(NUM_CELLS) {
        init();
    }

    void init() {
        clear();
        agent_pos = {ENV_SIZE / 2, ENV_SIZE / 2, ENV_SIZE / 2, 0};
    }

    // ── Clear ────────────────────────────────────────────────
    void clear() {
        for (int i = 0; i < ENV_SIZE; i++)
            for (int j = 0; j < ENV_SIZE; j++)
                for (int k = 0; k < ENV_SIZE; k++)
                    at(i, j, k).fill(0.5f);
    }

    // ── Validation ───────────────────────────────────────────
    static bool valid(const BCCCoord& c) {
        return c.i >= 0 && c.i < ENV_SIZE &&
               c.j >= 0 && c.j < ENV_SIZE &&
               c.k >= 0 && c.k < ENV_SIZE &&
               c.l >= 0 && c.l <= 1;
    }

    // ── Store / Load at coordinate ───────────────────────────
    void store(const BCCCoord& coord, const PillarStateVector& psv) {
        if (!valid(coord)) return;
        at(coord.i, coord.j, coord.k) = psv;
    }

    PillarStateVector load(const BCCCoord& coord) const {
        if (!valid(coord)) {
            PillarStateVector fallback;
            fallback.fill(0.5f);
            return fallback;
        }
        return at(coord.i, coord.j, coord.k);
    }

    // ── Store / Load at agent position ───────────────────────
    void store_at_agent(const PillarStateVector& psv) {
        store(agent_pos, psv);
    }

    PillarStateVector load_at_agent() const {
        return load(agent_pos);
    }

    // ── Move agent ───────────────────────────────────────────
    void move_to(const BCCCoord& coord) {
        if (valid(coord)) agent_pos = coord;
    }

    void move_by(int di, int dj, int dk) {
        BCCCoord next = {agent_pos.i + di, agent_pos.j + dj, agent_pos.k + dk, 0};
        next.l = (next.i + next.j + next.k) & 1;
        if (valid(next)) agent_pos = next;
    }

    // ── Get 6 face-neighbors ─────────────────────────────────
    // Returns the 6 axis-aligned neighbors of agent_pos.
    // Each neighbor is a PSV that can be spatially blended with
    // the current cognitive state.
    void get_neighbors(PillarStateVector out[6]) const {
        static const int dirs[6][3] = {
            {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
        };
        for (int d = 0; d < 6; d++) {
            BCCCoord n = {agent_pos.i + dirs[d][0],
                          agent_pos.j + dirs[d][1],
                          agent_pos.k + dirs[d][2],
                          0};
            n.l = (n.i + n.j + n.k) & 1;
            out[d] = load(n);
        }
    }

    // ── Scatter ──────────────────────────────────────────────
    // Spread a PSV to all cells within Manhattan radius.
    // Each cell receives a distance-weighted blend of the input.
    // This is the spatial encoding operation: a thought pattern
    // diffuses into the BCC environment.

    void scatter(const PillarStateVector& psv, int radius) {
        int ci = agent_pos.i;
        int cj = agent_pos.j;
        int ck = agent_pos.k;

        for (int di = -radius; di <= radius; di++) {
            for (int dj = -radius; dj <= radius; dj++) {
                for (int dk = -radius; dk <= radius; dk++) {
                    int ni = ci + di;
                    int nj = cj + dj;
                    int nk = ck + dk;
                    if (ni < 0 || ni >= ENV_SIZE) continue;
                    if (nj < 0 || nj >= ENV_SIZE) continue;
                    if (nk < 0 || nk >= ENV_SIZE) continue;

                    int dist = std::abs(di) + std::abs(dj) + std::abs(dk);
                    if (dist > radius) continue;

                    // Distance weight: linear falloff
                    float weight = 1.0f - (float)dist / (float)(radius + 1);
                    // Ensure weight doesn't make the center cell vanish
                    weight = weight * 0.5f;

                    PillarStateVector& cell = at(ni, nj, nk);
                    for (int p = 0; p < NumPillars; p++) {
                        float blended = cell[p] * (1.0f - weight) + psv.pillars[p] * weight;
                        cell[p] = vn::fp20_t(blended);
                    }
                }
            }
        }
    }

    // ── Gather ───────────────────────────────────────────────
    // Collect nearby PSVs weighted by distance from agent_pos.
    // Returns a single PSV that is the spatial blend of all
    // cells within radius. This is the spatial decoding /
    // associative pattern completion operation.

    PillarStateVector gather(int radius) const {
        PillarStateVector result;
        result.fill(0.0f);

        int ci = agent_pos.i;
        int cj = agent_pos.j;
        int ck = agent_pos.k;
        float total_weight = 0.0f;

        for (int di = -radius; di <= radius; di++) {
            for (int dj = -radius; dj <= radius; dj++) {
                for (int dk = -radius; dk <= radius; dk++) {
                    int ni = ci + di;
                    int nj = cj + dj;
                    int nk = ck + dk;
                    if (ni < 0 || ni >= ENV_SIZE) continue;
                    if (nj < 0 || nj >= ENV_SIZE) continue;
                    if (nk < 0 || nk >= ENV_SIZE) continue;

                    int dist = std::abs(di) + std::abs(dj) + std::abs(dk);
                    if (dist > radius) continue;

                    float weight = 1.0f - (float)dist / (float)(radius + 1);
                    total_weight += weight;

                    const PillarStateVector& cell = at(ni, nj, nk);
                    for (int p = 0; p < NumPillars; p++) {
                        result[p] += cell[p] * weight;
                    }
                }
            }
        }

        if (total_weight > 1e-8f) {
            float inv = 1.0f / total_weight;
            for (int p = 0; p < NumPillars; p++) {
                result[p] = vn::fp20_t(result[p] * inv);
            }
        } else {
            result.fill(0.5f);
        }

        return result;
    }

    // ── Thought Frame Encode ─────────────────────────────────
    // Encode the 512D Hopf thought vector into the BCC grid.
    // Each Hopf frame (frame 0..31) maps to a layer of the grid
    // at z = frame.
    // frame 0  → z=0  (with i×j = pillar × block)
    // frame 31 → z=31
    //
    // This gives the thought vector a spatial representation in
    // the BCC environment: frames become layers, pillars become
    // x-axis positions, frame indices become y-axis positions.

    void encode_thought(const float thought[HOPF_FIBER_DIM]) {
        for (int f = 0; f < HOPF_FRAME_COUNT && f < ENV_SIZE; f++) {
            for (int b = 0; b < FIBER_BLOCK_COUNT && b < ENV_SIZE; b++) {
                for (int p = 0; p < NumPillars && p < ENV_SIZE; p++) {
                    int idx = f * NumPillars + p;
                    if (idx < HOPF_FIBER_DIM) {
                        at(p, b, f).pillars[p] = vn::fp20_t(thought[idx]);
                    }
                }
            }
        }
    }

    // ── Decode to Buffer ─────────────────────────────────────
    // Extract a region of the grid into a flat buffer of PSVs.
    // Useful for feeding spatial context into the ThoughtEngine.

    int decode_region(int ci, int cj, int ck,
                      int size, PillarStateVector* buffer, int max_count) const {
        int count = 0;
        for (int di = 0; di < size && count < max_count; di++) {
            for (int dj = 0; dj < size && count < max_count; dj++) {
                for (int dk = 0; dk < size && count < max_count; dk++) {
                    BCCCoord c = {ci + di, cj + dj, ck + dk, 0};
                    c.l = (c.i + c.j + c.k) & 1;
                    buffer[count++] = load(c);
                }
            }
        }
        return count;
    }
};

}} // namespace vn::quantum
