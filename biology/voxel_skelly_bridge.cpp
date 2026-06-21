#include "voxel_skelly_bridge.h"
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <cmath>

namespace Van_Nueman {

namespace {

// ── Pillar helpers: average pillar value across all 24 vertices ──

vn::fp20_t cell_pillar_average(const VoxelCell& cell, int pillar) {
    vn::fp20_t sum(0);
    for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
        sum = sum + cell.material.pillar_to_vertex[pillar][v];
    }
    return sum / vn::fp20_t(static_cast<float>(YIELD_VERTICES));
}

vn::fp20_t cell_integrity(const VoxelCell& cell) {
    return cell_pillar_average(cell, Integrity);
}

vn::fp20_t cell_force(const VoxelCell& cell) {
    return cell_pillar_average(cell, Force);
}

vn::fp20_t cell_warmth(const VoxelCell& cell) {
    return cell_pillar_average(cell, Warmth);
}

vn::fp20_t cell_distortion(const VoxelCell& cell) {
    return cell_pillar_average(cell, Distortion);
}

// ── Build coord→cell pointer lookup ──

std::unordered_map<uint64_t, const VoxelCell*> build_cell_map(
    const std::vector<VoxelCell>& cells)
{
    std::unordered_map<uint64_t, const VoxelCell*> m;
    m.reserve(cells.size());
    for (const auto& c : cells) {
        if (c.active) {
            m[bcc_hash(c.coord)] = &c;
        }
    }
    return m;
}

// ── Find endpoint cells in a chain (degree ≤ 1 within the chain) ──

void find_chain_endpoints(
    const std::vector<BCCCoord>& chain,
    std::vector<BCCCoord>& endpoints)
{
    std::unordered_set<uint64_t> chain_set;
    chain_set.reserve(chain.size());
    for (const auto& c : chain) {
        chain_set.insert(bcc_hash(c));
    }

    for (const auto& c : chain) {
        int nbr = 0;
        for (uint32_t f = 0; f < 14; f++) {
            if (chain_set.count(bcc_hash(bcc_face_neighbor(c, f)))) {
                nbr++;
            }
        }
        if (nbr <= 1) {
            endpoints.push_back(c);
        }
    }

    if (endpoints.empty() && !chain.empty()) {
        endpoints.push_back(chain.front());
        if (chain.size() > 1) {
            endpoints.push_back(chain.back());
        }
    }
}

// ── BCCCoord hasher for unordered_map keys ──

struct BCCKey {
    uint64_t operator()(const BCCCoord& c) const {
        return bcc_hash(c);
    }
};

struct BCCEqual {
    bool operator()(const BCCCoord& a, const BCCCoord& b) const {
        return bcc_equal(a, b);
    }
};

} // anonymous namespace

// ===================================================================
// Constructor
// ===================================================================

VoxelSkellyBridge::VoxelSkellyBridge()
    : next_bone_id_(0)
    , next_joint_id_(0)
    , next_muscle_id_(0)
    , next_organ_id_(0)
{
}

// ===================================================================
// cells_are_adjacent
// ===================================================================

bool VoxelSkellyBridge::cells_are_adjacent(const BCCCoord& a, const BCCCoord& b) {
    for (uint32_t f = 0; f < 14; f++) {
        if (bcc_equal(bcc_face_neighbor(a, f), b)) {
            return true;
        }
    }
    return false;
}

// ===================================================================
// find_shared_face
// ===================================================================

int VoxelSkellyBridge::find_shared_face(const BCCCoord& a, const BCCCoord& b) {
    for (uint32_t f = 0; f < 14; f++) {
        if (bcc_equal(bcc_face_neighbor(a, f), b)) {
            return static_cast<int>(f);
        }
    }
    return -1;
}

// ===================================================================
// compute_chain_flexibility
// ===================================================================

vn::fp20_t VoxelSkellyBridge::compute_chain_flexibility(
    const std::vector<BCCCoord>& chain_coords,
    const std::vector<VoxelCell>& cells)
{
    auto cell_map = build_cell_map(cells);
    vn::fp20_t sum(0);
    uint32_t count = 0;

    for (const auto& coord : chain_coords) {
        auto it = cell_map.find(bcc_hash(coord));
        if (it == cell_map.end()) continue;

        const VoxelCell* cp = it->second;
        vn::fp20_t face_sum(0);
        for (uint32_t f = 0; f < TRUNC_OCT_FACES; f++) {
            face_sum = face_sum + (vn::fp20_t(1.0f) - cp->pyramids[f].structural_integrity);
        }
        sum = sum + (face_sum / vn::fp20_t(static_cast<float>(TRUNC_OCT_FACES)));
        count++;
    }

    if (count == 0) return vn::fp20_t(0.1f);
    return sum / vn::fp20_t(static_cast<float>(count));
}

// ===================================================================
// compute_break_threshold
// ===================================================================

vn::fp20_t VoxelSkellyBridge::compute_break_threshold(
    const std::vector<BCCCoord>& chain_coords,
    const std::vector<VoxelCell>& cells)
{
    auto cell_map = build_cell_map(cells);
    vn::fp20_t sum(0);
    uint32_t count = 0;

    for (const auto& coord : chain_coords) {
        auto it = cell_map.find(bcc_hash(coord));
        if (it == cell_map.end()) continue;

        const VoxelCell* cp = it->second;
        vn::fp20_t vert_sum(0);
        for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
            vert_sum = vert_sum + cp->material.pillar_to_vertex[Integrity][v];
        }
        sum = sum + (vert_sum / vn::fp20_t(static_cast<float>(YIELD_VERTICES)));
        count++;
    }

    if (count == 0) return vn::fp20_t(100.0f);
    return sum / vn::fp20_t(static_cast<float>(count));
}

// ===================================================================
// apply_joint_torsion
// ===================================================================

vn::fp20_t VoxelSkellyBridge::apply_joint_torsion(
    VoxelJointMapping& joint,
    vn::fp20_t applied_angular_shift,
    vn::fp20_t willpower,
    vn::fp20_t depth)
{
    ConstraintState cs;
    cs.init(0.0f);

    AccumulationResult result = cs.absorb_overflow(
        applied_angular_shift.to_float(),
        willpower.to_float(),
        depth.to_float());

    joint.angular_displacement = joint.angular_displacement +
        vn::fp20_t(result.delta_theta_applied);

    if (joint.angular_displacement < joint.pitch_min)
        joint.angular_displacement = joint.pitch_min;
    if (joint.angular_displacement > joint.pitch_max)
        joint.angular_displacement = joint.pitch_max;

    return vn::fp20_t(result.delta_theta_applied);
}

// ===================================================================
// trace_cell_chain (private) — BFS flood-fill of high-integrity cells
// ===================================================================

void VoxelSkellyBridge::trace_cell_chain(
    BCCCoord seed,
    const std::vector<VoxelCell>& cells,
    std::vector<BCCCoord>& out_chain)
{
    auto cell_map = build_cell_map(cells);

    std::unordered_set<uint64_t> visited;
    std::queue<BCCCoord> q;

    q.push(seed);
    visited.insert(bcc_hash(seed));

    while (!q.empty()) {
        BCCCoord cur = q.front();
        q.pop();
        out_chain.push_back(cur);

        for (uint32_t f = 0; f < 14; f++) {
            BCCCoord n = bcc_face_neighbor(cur, f);
            uint64_t nh = bcc_hash(n);
            if (visited.count(nh)) continue;

            auto it = cell_map.find(nh);
            if (it == cell_map.end()) continue;
            if (!it->second->active) continue;

            if (cell_integrity(*it->second) > vn::fp20_t(0.6f)) {
                visited.insert(nh);
                q.push(n);
            }
        }
    }
}

// ===================================================================
// detect_bone_chains
// ===================================================================

uint32_t VoxelSkellyBridge::detect_bone_chains(
    const std::vector<VoxelCell>& cells,
    std::vector<VoxelBoneMapping>& out_bones)
{
    auto cell_map = build_cell_map(cells);

    // Pre-cache which coords have high integrity
    std::unordered_set<uint64_t> hi_cells;
    for (const auto& kv : cell_map) {
        if (cell_integrity(*kv.second) > vn::fp20_t(0.6f)) {
            hi_cells.insert(kv.first);
        }
    }

    std::unordered_set<uint64_t> visited;
    uint32_t bones_created = 0;

    for (const auto& kv : cell_map) {
        uint64_t hash = kv.first;
        if (hi_cells.count(hash) == 0) continue;
        if (visited.count(hash)) continue;

        // Flood-fill the connected component
        std::vector<BCCCoord> chain;
        std::queue<BCCCoord> q;
        const BCCCoord& seed = kv.second->coord;

        q.push(seed);
        visited.insert(hash);

        while (!q.empty()) {
            BCCCoord cur = q.front();
            q.pop();
            chain.push_back(cur);

            for (uint32_t f = 0; f < 14; f++) {
                BCCCoord n = bcc_face_neighbor(cur, f);
                uint64_t nh = bcc_hash(n);
                if (visited.count(nh)) continue;
                if (hi_cells.count(nh) == 0) continue;

                auto it = cell_map.find(nh);
                if (it == cell_map.end()) continue;

                visited.insert(nh);
                q.push(n);
            }
        }

        // Bones require at least 2 cells
        if (chain.size() < 2) continue;

        VoxelBoneMapping bm;
        bm.bone_id = next_bone_id_++;
        bm.cell_coords = std::move(chain);
        bm.cell_count = static_cast<uint32_t>(bm.cell_coords.size());

        vn::fp20_t int_sum(0);
        for (const auto& c : bm.cell_coords) {
            auto it = cell_map.find(bcc_hash(c));
            if (it != cell_map.end()) {
                int_sum = int_sum + cell_integrity(*it->second);
            }
        }
        bm.avg_integrity = int_sum / vn::fp20_t(static_cast<float>(bm.cell_count));

        bm.flexibility = compute_chain_flexibility(bm.cell_coords, cells);
        bm.break_threshold = compute_break_threshold(bm.cell_coords, cells);

        if (bm.cell_coords.size() >= 2) {
            const BCCCoord& first = bm.cell_coords.front();
            const BCCCoord& last = bm.cell_coords.back();
            float dx = static_cast<float>(last.i - first.i);
            float dy = static_cast<float>(last.j - first.j);
            float dz = static_cast<float>(last.k - first.k);
            bm.length = vn::fp20_t(std::sqrt(dx * dx + dy * dy + dz * dz));
        } else {
            bm.length = vn::fp20_t(0);
        }

        bm.start_node = nullptr;
        bm.end_node = nullptr;

        out_bones.push_back(bm);
        bones_created++;
    }

    return bones_created;
}

// ===================================================================
// detect_joints
// ===================================================================

uint32_t VoxelSkellyBridge::detect_joints(
    const std::vector<VoxelBoneMapping>& bones,
    const std::vector<VoxelCell>& cells,
    std::vector<VoxelJointMapping>& out_joints,
    std::vector<VoxelMultiJointMapping>& out_multi_joints)
{
    auto cell_map = build_cell_map(cells);

    // ── Find endpoints of every bone ──
    struct EP {
        BCCCoord coord;
        uint32_t bone_id;
        uint32_t bone_idx;
    };
    std::vector<EP> all_eps;

    for (uint32_t bi = 0; bi < bones.size(); bi++) {
        std::vector<BCCCoord> eps;
        find_chain_endpoints(bones[bi].cell_coords, eps);
        for (const auto& c : eps) {
            all_eps.push_back({c, bones[bi].bone_id, bi});
        }
    }

    // ── Group endpoint coords by bone_id to find multi-joints ──
    std::unordered_map<uint64_t, std::vector<uint32_t>> coord_bones;
    for (const auto& ep : all_eps) {
        coord_bones[bcc_hash(ep.coord)].push_back(ep.bone_id);
    }

    // Deduplicate per-coord bone lists
    for (auto& kv : coord_bones) {
        auto& v = kv.second;
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
    }

    // Used endpoint cells (tracked by bone pair to avoid duplicate joints)
    std::unordered_set<uint64_t> used_endpoints;

    // ── Multi-joints: cells that terminate 3+ bones ──
    for (auto& kv : coord_bones) {
        if (kv.second.size() >= 3) {
            VoxelMultiJointMapping mj;
            mj.multi_joint_id = static_cast<uint32_t>(out_multi_joints.size());
            // Find the actual coord from the hash
            // We stored coords from all_eps; look up by hash through cell_map
            const VoxelCell* cell = nullptr;
            for (const auto& ep : all_eps) {
                if (bcc_hash(ep.coord) == kv.first) {
                    mj.center_cell = ep.coord;
                    cell = (cell_map.count(kv.first)) ? cell_map[kv.first] : nullptr;
                    break;
                }
            }
            mj.bone_ids = kv.second;
            mj.torsion_tension = vn::fp20_t(0);
            mj.constraint.init(0.0f);
            out_multi_joints.push_back(mj);

            used_endpoints.insert(kv.first);
        }
    }

    // ── Pairwise joints between two bones ──
    // Track processed bone pairs to avoid duplicates
    std::unordered_set<uint64_t> paired;

    for (size_t i = 0; i < all_eps.size(); i++) {
        uint64_t hi = bcc_hash(all_eps[i].coord);
        if (used_endpoints.count(hi)) continue;

        for (size_t j = i + 1; j < all_eps.size(); j++) {
            uint64_t hj = bcc_hash(all_eps[j].coord);
            if (used_endpoints.count(hj)) continue;

            if (all_eps[i].bone_id == all_eps[j].bone_id) continue;

            // Check if these endpoints are adjacent
            if (!cells_are_adjacent(all_eps[i].coord, all_eps[j].coord)) continue;

            // Create a sorted bone-pair key
            uint32_t ba = all_eps[i].bone_id;
            uint32_t bb = all_eps[j].bone_id;
            uint64_t pk = (ba < bb)
                ? ((uint64_t)ba << 32 | bb)
                : ((uint64_t)bb << 32 | ba);
            if (paired.count(pk)) continue;
            paired.insert(pk);

            VoxelJointMapping jm;
            jm.joint_id = next_joint_id_++;

            // The joint cell is the one that's at the shared boundary
            // Use the first endpoint as the joint cell
            jm.joint_cell = all_eps[i].coord;
            jm.bone_a_id = ba;
            jm.bone_b_id = bb;
            jm.angular_displacement = vn::fp20_t(0);

            jm.pitch_min = vn::fp20_t(-1.5f);
            jm.pitch_max = vn::fp20_t(1.5f);
            jm.yaw_min = vn::fp20_t(-1.5f);
            jm.yaw_max = vn::fp20_t(1.5f);
            jm.roll_min = vn::fp20_t(-0.5f);
            jm.roll_max = vn::fp20_t(0.5f);

            jm.node = nullptr;
            out_joints.push_back(jm);
        }
    }

    return static_cast<uint32_t>(out_joints.size() + out_multi_joints.size());
}

// ===================================================================
// detect_muscle_groups
// ===================================================================

uint32_t VoxelSkellyBridge::detect_muscle_groups(
    const std::vector<VoxelBoneMapping>& bones,
    const std::vector<VoxelJointMapping>& joints,
    const std::vector<VoxelCell>& cells,
    std::vector<VoxelMuscleMapping>& out_muscles)
{
    auto cell_map = build_cell_map(cells);

    // Build coord -> bone_id lookup
    std::unordered_map<uint64_t, uint32_t> coord_to_bone;
    for (const auto& bone : bones) {
        for (const auto& c : bone.cell_coords) {
            coord_to_bone[bcc_hash(c)] = bone.bone_id;
        }
    }

    std::unordered_set<uint64_t> processed_pairs;
    uint32_t muscle_count = 0;

    for (const auto& kv : cell_map) {
        uint64_t ch = kv.first;
        const VoxelCell* cp = kv.second;

        // Skip cells already in bones
        if (coord_to_bone.count(ch)) continue;

        // Must have FORCE > 0.6
        if (cell_force(*cp) <= vn::fp20_t(0.6f)) continue;

        // Find which bones this cell is adjacent to
        std::unordered_set<uint32_t> adj_bones;
        for (uint32_t f = 0; f < 14; f++) {
            BCCCoord n = bcc_face_neighbor(cp->coord, f);
            uint64_t nh = bcc_hash(n);
            auto it = coord_to_bone.find(nh);
            if (it != coord_to_bone.end()) {
                adj_bones.insert(it->second);
            }
        }

        // A muscle connects exactly 2 bones
        if (adj_bones.size() != 2) continue;

        auto it = adj_bones.begin();
        uint32_t ba = *it;
        ++it;
        uint32_t bb = *it;
        uint64_t pk = (ba < bb)
            ? ((uint64_t)ba << 32 | bb)
            : ((uint64_t)bb << 32 | ba);
        if (processed_pairs.count(pk)) continue;
        processed_pairs.insert(pk);

        // Gather ALL FORCE > 0.6 cells bridging these two bones
        std::vector<BCCCoord> muscle_cells;
        vn::fp20_t force_sum(0);

        for (const auto& kv2 : cell_map) {
            uint64_t h2 = kv2.first;
            const VoxelCell* c2 = kv2.second;

            if (coord_to_bone.count(h2)) continue;
            if (cell_force(*c2) <= vn::fp20_t(0.6f)) continue;

            bool adj_a = false, adj_b = false;
            for (uint32_t f = 0; f < 14; f++) {
                BCCCoord n = bcc_face_neighbor(c2->coord, f);
                uint64_t nh = bcc_hash(n);
                auto it = coord_to_bone.find(nh);
                if (it != coord_to_bone.end()) {
                    if (it->second == ba) adj_a = true;
                    if (it->second == bb) adj_b = true;
                }
            }

            if (adj_a && adj_b) {
                muscle_cells.push_back(c2->coord);
                force_sum = force_sum + cell_force(*c2);
            }
        }

        if (muscle_cells.empty()) continue;

        VoxelMuscleMapping mm;
        mm.muscle_id = next_muscle_id_++;
        mm.cell_coords = std::move(muscle_cells);
        mm.bone_a_id = ba;
        mm.bone_b_id = bb;
        mm.activation = vn::fp20_t(0);
        mm.max_force = force_sum / vn::fp20_t(static_cast<float>(mm.cell_coords.size()));
        mm.muscle = nullptr;

        out_muscles.push_back(mm);
        muscle_count++;
    }

    return muscle_count;
}

// ===================================================================
// detect_organs
// ===================================================================

uint32_t VoxelSkellyBridge::detect_organs(
    const std::vector<VoxelCell>& cells,
    std::vector<VoxelOrganMapping>& out_organs)
{
    auto cell_map = build_cell_map(cells);
    std::unordered_set<uint64_t> visited;
    uint32_t organ_count = 0;

    for (const auto& kv : cell_map) {
        uint64_t ch = kv.first;
        if (visited.count(ch)) continue;

        const VoxelCell* cp = kv.second;

        // Classify by pillar signature
        vn::fp20_t warmth = cell_warmth(*cp);
        vn::fp20_t distortion = cell_distortion(*cp);

        OrganType otype;
        bool is_organ = true;
        vn::fp20_t threshold;

        if (warmth > vn::fp20_t(0.8f)) {
            otype = ORGAN_PUMP;
            threshold = vn::fp20_t(0.8f);
        } else if (distortion > vn::fp20_t(0.7f)) {
            otype = ORGAN_FACTORY;
            threshold = vn::fp20_t(0.7f);
        } else {
            // Also check other signatures
            // WARMTH > 0.6 could be a valve
            if (warmth > vn::fp20_t(0.6f)) {
                otype = ORGAN_VALVE;
                threshold = vn::fp20_t(0.6f);
            } else {
                is_organ = false;
            }
        }

        if (!is_organ) continue;

        // Flood-fill connected cells with same type signature
        using BCCQ = std::pair<BCCCoord, uint64_t>;
        std::vector<BCCCoord> cluster;
        std::queue<BCCQ> q;
        q.push({cp->coord, ch});
        visited.insert(ch);

        auto matches_signature = [&](const VoxelCell& c) -> bool {
            switch (otype) {
            case ORGAN_PUMP:   return cell_warmth(c) > vn::fp20_t(0.8f);
            case ORGAN_VALVE:  return cell_warmth(c) > vn::fp20_t(0.6f);
            case ORGAN_FACTORY: return cell_distortion(c) > vn::fp20_t(0.7f);
            default:           return false;
            }
        };

        while (!q.empty()) {
            BCCCoord cur = q.front().first;
            q.pop();
            cluster.push_back(cur);

            for (uint32_t f = 0; f < 14; f++) {
                BCCCoord n = bcc_face_neighbor(cur, f);
                uint64_t nh = bcc_hash(n);
                if (visited.count(nh)) continue;

                auto it = cell_map.find(nh);
                if (it == cell_map.end()) continue;
                if (!it->second->active) continue;

                if (matches_signature(*it->second)) {
                    visited.insert(nh);
                    q.push({n, nh});
                }
            }
        }

        if (cluster.empty()) continue;

        VoxelOrganMapping om;
        om.organ_id = next_organ_id_++;
        om.center_cell = cp->coord;
        om.type = otype;
        om.volume = vn::fp20_t(static_cast<float>(cluster.size()));
        om.active_state = vn::fp20_t(0.5f);
        om.constituent_cells = std::move(cluster);
        om.organ = nullptr;

        out_organs.push_back(om);
        organ_count++;
    }

    return organ_count;
}

// ===================================================================
// map_cells_to_skeleton
// ===================================================================

uint32_t VoxelSkellyBridge::map_cells_to_skeleton(
    const std::vector<VoxelCell>& cells,
    ModularSkeleton& out_skeleton)
{
    std::vector<VoxelBoneMapping> bones;
    std::vector<VoxelJointMapping> joints;
    std::vector<VoxelMultiJointMapping> multi_joints;
    std::vector<VoxelMuscleMapping> muscles;
    std::vector<VoxelOrganMapping> organs;

    detect_bone_chains(cells, bones);
    detect_joints(bones, cells, joints, multi_joints);
    detect_muscle_groups(bones, joints, cells, muscles);
    detect_organs(cells, organs);

    if (bones.empty()) return 0;

    // ── Compute endpoint coords for each bone ──
    struct BoneEndpoints {
        BCCCoord start;
        BCCCoord end;
    };
    std::vector<BoneEndpoints> bone_eps(bones.size());

    for (size_t bi = 0; bi < bones.size(); bi++) {
        std::vector<BCCCoord> eps;
        find_chain_endpoints(bones[bi].cell_coords, eps);

        if (eps.size() >= 2) {
            bone_eps[bi].start = eps[0];
            bone_eps[bi].end = eps[1];
        } else if (eps.size() == 1) {
            bone_eps[bi].start = eps[0];
            bone_eps[bi].end = eps[0];
        } else {
            bone_eps[bi].start = bones[bi].cell_coords.front();
            bone_eps[bi].end = bones[bi].cell_coords.back();
        }
    }

    // ── Create SkeletonNodes for all unique bone endpoints ──
    uint32_t node_id = 1000;
    std::unordered_map<uint64_t, SkeletonNode*> node_map;

    for (size_t bi = 0; bi < bones.size(); bi++) {
        for (const auto& ep : {bone_eps[bi].start, bone_eps[bi].end}) {
            uint64_t h = bcc_hash(ep);
            if (node_map.count(h)) continue;
            Vec3 pos(
                static_cast<float>(ep.i),
                static_cast<float>(ep.j),
                static_cast<float>(ep.k));
            node_map[h] = new SkeletonNode(node_id++, pos, nullptr, "joint");
        }
    }

    // ── Set root and establish parent-child links ──
    uint64_t root_hash = bcc_hash(bone_eps[0].start);
    out_skeleton.root = node_map[root_hash];

    for (size_t bi = 0; bi < bones.size(); bi++) {
        SkeletonNode* sn = node_map[bcc_hash(bone_eps[bi].start)];
        SkeletonNode* en = node_map[bcc_hash(bone_eps[bi].end)];

        if (sn != en && en->parent == nullptr) {
            en->parent = sn;
            bool dup = false;
            for (auto* child : sn->children) {
                if (child == en) { dup = true; break; }
            }
            if (!dup) sn->children.push_back(en);
        }
    }

    // ── Create BoneSegments ──
    uint32_t seg_id = 2000;
    uint32_t transport_id = 3000;

    for (size_t bi = 0; bi < bones.size(); bi++) {
        SkeletonNode* sn = node_map[bcc_hash(bone_eps[bi].start)];
        SkeletonNode* en = node_map[bcc_hash(bone_eps[bi].end)];

        BoneSegment* seg = new BoneSegment(seg_id++, sn, en);
        seg->flexibility = bones[bi].flexibility.to_float();
        seg->break_threshold = bones[bi].break_threshold.to_float();
        out_skeleton.segments.push_back(seg);

        out_skeleton.transports.push_back(
            new InterstitialFluid(transport_id++, "bone_fluid", seg));

        bones[bi].start_node = sn;
        bones[bi].end_node = en;
    }

    // ── Link joints to SkeletonNodes ──
    for (auto& jm : joints) {
        BCCCoord cell = jm.joint_cell;
        uint64_t h = bcc_hash(cell);
        auto it = node_map.find(h);
        if (it != node_map.end()) {
            jm.node = it->second;
            // Sync constraint limits from SkeletonNode
            jm.pitch_min = vn::fp20_t(it->second->pitch_min);
            jm.pitch_max = vn::fp20_t(it->second->pitch_max);
            jm.yaw_min = vn::fp20_t(it->second->yaw_min);
            jm.yaw_max = vn::fp20_t(it->second->yaw_max);
            jm.roll_min = vn::fp20_t(it->second->roll_min);
            jm.roll_max = vn::fp20_t(it->second->roll_max);
        }
    }

    // ── Create MuscleGroups ──
    uint32_t muscle_id = 4000;

    for (auto& mm : muscles) {
        // Find origin = start_node of bone_a, insertion = end_node of bone_b
        SkeletonNode* origin = nullptr;
        SkeletonNode* insertion = nullptr;

        for (size_t bi = 0; bi < bones.size(); bi++) {
            if (bones[bi].bone_id == mm.bone_a_id) {
                origin = bones[bi].start_node;
            }
            if (bones[bi].bone_id == mm.bone_b_id) {
                insertion = bones[bi].end_node;
            }
        }

        if (origin && insertion) {
            MuscleGroup* mg = new MuscleGroup(
                muscle_id++, "voxel_muscle", origin, insertion);
            mg->activation = mm.activation.to_float();
            out_skeleton.muscles.push_back(mg);
            mm.muscle = mg;
        }
    }

    // ── Create Organs ──
    uint32_t organ_id = 5000;

    for (auto& om : organs) {
        // Attach to the first bone segment (or the one nearest the organ center)
        BoneSegment* target = out_skeleton.segments.empty()
            ? nullptr
            : out_skeleton.segments.front();

        // Try to find the closest bone segment
        if (!out_skeleton.segments.empty()) {
            vn::fp20_t best_dist(-1);
            for (auto* seg : out_skeleton.segments) {
                Vec3 spos = seg->start->local_pos;
                float dx = spos.x - static_cast<float>(om.center_cell.i);
                float dy = spos.y - static_cast<float>(om.center_cell.j);
                float dz = spos.z - static_cast<float>(om.center_cell.k);
                vn::fp20_t d = vn::fp20_t(dx * dx + dy * dy + dz * dz);
                if (best_dist < vn::fp20_t(0) || d < best_dist) {
                    best_dist = d;
                    target = seg;
                }
            }
        }

        const char* oname = "organ";
        switch (om.type) {
        case ORGAN_PUMP:   oname = "pump";   break;
        case ORGAN_VALVE:  oname = "valve";  break;
        case ORGAN_POWER_PLANT: oname = "power_plant"; break;
        case ORGAN_FACTORY: oname = "factory"; break;
        }

        Organ* organ = new Organ(
            organ_id++, oname, om.type, target, om.volume.to_float());
        organ->active_state = om.active_state.to_float();
        out_skeleton.organs.push_back(organ);

        if (target) {
            target->organs.push_back(organ);
        }
        om.organ = organ;
    }

    return static_cast<uint32_t>(
        1 + bones.size() + joints.size() + multi_joints.size() +
        muscles.size() + organs.size());
}

// ===================================================================
// update_skeleton_from_cells
// ===================================================================

void VoxelSkellyBridge::update_skeleton_from_cells(
    const std::vector<VoxelCell>& cells,
    ModularSkeleton& skeleton)
{
    auto cell_map = build_cell_map(cells);

    // Update bone flexibility and break_threshold from current cell state
    for (auto* seg : skeleton.segments) {
        if (!seg || seg->is_fractured) continue;

        // Gather all cells between the segment's start and end positions
        BCCCoord start_coord = {
            static_cast<int32_t>(seg->start->local_pos.x),
            static_cast<int32_t>(seg->start->local_pos.y),
            static_cast<int32_t>(seg->start->local_pos.z),
            0
        };
        start_coord.l = (start_coord.i + start_coord.j + start_coord.k) & 1;

        BCCCoord end_coord = {
            static_cast<int32_t>(seg->end->local_pos.x),
            static_cast<int32_t>(seg->end->local_pos.y),
            static_cast<int32_t>(seg->end->local_pos.z),
            0
        };
        end_coord.l = (end_coord.i + end_coord.j + end_coord.k) & 1;

        std::vector<BCCCoord> chain_coords;
        chain_coords.push_back(start_coord);
        if (bcc_hash(start_coord) != bcc_hash(end_coord)) {
            chain_coords.push_back(end_coord);
        }

        seg->flexibility = compute_chain_flexibility(chain_coords, cells).to_float();
        seg->break_threshold = compute_break_threshold(chain_coords, cells).to_float();
    }

    // Sync muscle activations
    for (auto* mg : skeleton.muscles) {
        if (mg) {
            mg->update_volume();
        }
    }
}

// ===================================================================
// apply_skelly_forces_to_cells
// ===================================================================

void VoxelSkellyBridge::apply_skelly_forces_to_cells(
    const ModularSkeleton& skeleton,
    std::vector<VoxelCell>& cells)
{
    auto cell_map = build_cell_map(cells);

    for (auto* mg : skeleton.muscles) {
        if (!mg || mg->activation < 0.01f) continue;

        // Compute force vector from muscle insertion - origin
        Vec3 force_dir = mg->insertion->get_global_position() -
                         mg->origin->get_global_position();
        float len = force_dir.norm();
        if (len < 0.001f) continue;

        // Normalize and scale by activation
        float inv_len = 1.0f / len;
        Vec3 force_vec(
            force_dir.x * inv_len * mg->activation,
            force_dir.y * inv_len * mg->activation,
            force_dir.z * inv_len * mg->activation);

        // Apply as a small perturbation to nearby cells
        BCCCoord origin_coord = {
            static_cast<int32_t>(mg->origin->local_pos.x),
            static_cast<int32_t>(mg->origin->local_pos.y),
            static_cast<int32_t>(mg->origin->local_pos.z),
            0
        };
        origin_coord.l = (origin_coord.i + origin_coord.j + origin_coord.k) & 1;

        BCCCoord insert_coord = {
            static_cast<int32_t>(mg->insertion->local_pos.x),
            static_cast<int32_t>(mg->insertion->local_pos.y),
            static_cast<int32_t>(mg->insertion->local_pos.z),
            0
        };
        insert_coord.l = (insert_coord.i + insert_coord.j + insert_coord.k) & 1;

        // Mark cells along the muscle path as perturbed
        for (auto& cell : cells) {
            if (!cell.active) continue;
            if (bcc_equal(cell.coord, origin_coord) ||
                bcc_equal(cell.coord, insert_coord)) {
                cell.mark_dirty();
            }
        }
    }

    // Apply organ forces
    for (auto* organ : skeleton.organs) {
        if (!organ || organ->active_state < 0.01f) continue;

        for (auto& cell : cells) {
            if (!cell.active) continue;
            BCCCoord cell_coord = cell.coord;
            uint32_t dist = static_cast<uint32_t>(
                std::abs(static_cast<float>(cell_coord.i - static_cast<int32_t>(organ->volume))));
            if (dist < 2) {
                cell.mark_dirty();
            }
        }
    }
}

} // namespace Van_Nueman
