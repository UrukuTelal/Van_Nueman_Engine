#include "voxel_organism.h"
#include "../voxel/BCCIndex.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

using namespace Van_Nueman;

uint32_t VoxelOrganism::next_organism_id_ = 1;

VoxelOrganismGenome::VoxelOrganismGenome() {
    for (int i = 0; i < 256; i++) {
        nn_weights[i] = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 0.1f;
    }
    lineage_hash = 0;
    metabolism_rate = 0.05f;
    photosynthesis_efficiency = 0.1f;
    mutation_rate = 0.02f;
    max_cells = 64;
    memory_retention = 0.0f;
}

VoxelOrganism::VoxelOrganism()
    : organism_id(next_organism_id_++),
      age_(vn::fp20_t(0)),
      energy_reserve_(vn::fp20_t(100.0f)),
      overall_health_(vn::fp20_t(1.0f)),
      body_temperature_(vn::fp20_t(37.0f)),
      growth_progress_(vn::fp20_t(0)),
      accumulated_harm_(vn::fp20_t(0)),
      alive_(false),
      skelly_dirty_(false),
      reproduction_count_(0),
      env_light_level_(vn::fp20_t(0)),
      env_nutrient_level_(vn::fp20_t(0)),
      env_temperature_(vn::fp20_t(0))
{
    pillar_state_.fill(vn::fp20_t(0.5f));
    baseline_pillars_.fill(vn::fp20_t(0.5f));
    constraint_.init();
}

void VoxelOrganism::init_zygote(const BCCCoord& position, const YieldMatrix& cell_material,
                                 const VoxelOrganismGenome& genome,
                                 const PillarStateVector& baseline) {
    VoxelCell zygote;
    vn::fp20_t default_size(0.5f);
    zygote.init(position, default_size, 0, cell_material);

    body_cells_.clear();
    body_cells_.push_back(zygote);

    genome_ = genome;
    baseline_pillars_ = baseline;
    pillar_state_ = baseline;
    constraint_.init();

    alive_ = true;
    energy_reserve_ = vn::fp20_t(100.0f);
    overall_health_ = vn::fp20_t(1.0f);
    body_temperature_ = vn::fp20_t(37.0f);

    apply_body_axes();
}

bool VoxelOrganism::tick(vn::fp20_t dt, const EnvironmentalSample* env) {
    sense_environment(env);
    harvest_energy(dt);
    metabolize(dt);

    if (alive_) {
        diffuse_pillars(dt);
        circulate_warmth(dt);
        process_harm(dt);
        age_ += dt;
        try_grow(dt);
        repair_damage(dt);
        apply_skelly_physics(dt);
    }

    if (overall_health_ <= vn::fp20_t(0) || constraint_.is_fully_constrained()) {
        die();
    }

    return alive_;
}

void VoxelOrganism::sense_environment(const EnvironmentalSample* env) {
    if (!env) return;

    env_light_level_ = env->light_level;
    env_nutrient_level_ = env->nutrient_level;
    env_temperature_ = env->temperature;

    if (body_cells_.empty()) return;

    auto boundary = find_boundary_cells();
    for (auto bi : boundary) {
        if (bi >= body_cells_.size()) continue;
        auto& cell = body_cells_[bi];
        if (!cell.active) continue;

        vn::fp20_t light = env_light_level_;
        vn::fp20_t nutrients = env_nutrient_level_;
        vn::fp20_t temp = env_temperature_;

        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            if (light > vn::fp20_t(0.5f)) {
                vn::fp20_t boost = (light - vn::fp20_t(0.5f)) * vn::fp20_t(0.01f);
                cell.pyramids[f].material_composition[Flux] =
                    vn::clamp(cell.pyramids[f].material_composition[Flux] + boost,
                              vn::fp20_t(0), vn::fp20_t(1));
            }
            if (nutrients > vn::fp20_t(0.5f)) {
                vn::fp20_t boost = (nutrients - vn::fp20_t(0.5f)) * vn::fp20_t(0.01f);
                cell.pyramids[f].material_composition[Attraction] =
                    vn::clamp(cell.pyramids[f].material_composition[Attraction] + boost,
                              vn::fp20_t(0), vn::fp20_t(1));
            }
            if (temp > vn::fp20_t(0.5f)) {
                vn::fp20_t boost = (temp - vn::fp20_t(0.5f)) * vn::fp20_t(0.01f);
                cell.pyramids[f].material_composition[Warmth] =
                    vn::clamp(cell.pyramids[f].material_composition[Warmth] + boost,
                              vn::fp20_t(0), vn::fp20_t(1));
            }
        }
    }
}

void VoxelOrganism::harvest_energy(vn::fp20_t dt) {
    float bcc = static_cast<float>(boundary_cell_count());
    energy_reserve_ = energy_reserve_ + vn::fp20_t(bcc * 0.01f * dt.to_float());
}

bool VoxelOrganism::metabolize(vn::fp20_t dt) {
    float dt_f = dt.to_float();
    float cc = static_cast<float>(cell_count());
    float bc = static_cast<float>(boundary_cell_count());
    float light = env_light_level_.to_float();

    energy_reserve_ = energy_reserve_ - vn::fp20_t(genome_.metabolism_rate * dt_f * cc);
    energy_reserve_ = energy_reserve_ + vn::fp20_t(genome_.photosynthesis_efficiency * light * bc * dt_f);

    if (energy_reserve_ <= vn::fp20_t(0)) {
        overall_health_ = overall_health_ - vn::fp20_t(0.1f * dt_f);
    }

    float metabolism_heat = cc * 0.01f;
    float radiation = body_temperature_.to_float() * 0.005f;
    body_temperature_ = body_temperature_ + vn::fp20_t((metabolism_heat - radiation) * dt_f);

    float temp = body_temperature_.to_float();
    if (temp < 0.0f || temp > 50.0f) {
        overall_health_ = overall_health_ - vn::fp20_t(0.05f * dt_f);
    }

    if (overall_health_ <= vn::fp20_t(0)) {
        alive_ = false;
    }

    return alive_;
}

void VoxelOrganism::diffuse_pillars(vn::fp20_t dt) {
    vn::fp20_t rate(0.05f);
    vn::fp20_t dt_rate = rate * dt;

    for (size_t i = 0; i < body_cells_.size(); i++) {
        if (!body_cells_[i].active) continue;
        for (size_t j = i + 1; j < body_cells_.size(); j++) {
            if (!body_cells_[j].active) continue;
            if (!VoxelSkellyBridge::cells_are_adjacent(body_cells_[i].coord, body_cells_[j].coord))
                continue;

            for (uint32_t p = 0; p < NumPillars; p++) {
                vn::fp20_t src_val(0), dst_val(0);
                for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                    src_val += body_cells_[i].pyramids[f].material_composition[p];
                    dst_val += body_cells_[j].pyramids[f].material_composition[p];
                }
                src_val = src_val / vn::fp20_t(static_cast<float>(VoxelCell::FACE_COUNT));
                dst_val = dst_val / vn::fp20_t(static_cast<float>(VoxelCell::FACE_COUNT));

                if (src_val == dst_val) continue;

                vn::fp20_t diff = src_val - dst_val;
                vn::fp20_t transfer = diff * dt_rate;

                if (transfer < vn::fp20_t(0))
                    transfer = vn::fp20_t() - transfer;

                if (src_val > dst_val) {
                    for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                        body_cells_[i].pyramids[f].material_composition[p] =
                            vn::clamp(body_cells_[i].pyramids[f].material_composition[p] - transfer,
                                      vn::fp20_t(0), vn::fp20_t(1));
                        body_cells_[j].pyramids[f].material_composition[p] =
                            vn::clamp(body_cells_[j].pyramids[f].material_composition[p] + transfer,
                                      vn::fp20_t(0), vn::fp20_t(1));
                    }
                } else {
                    for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                        body_cells_[j].pyramids[f].material_composition[p] =
                            vn::clamp(body_cells_[j].pyramids[f].material_composition[p] - transfer,
                                      vn::fp20_t(0), vn::fp20_t(1));
                        body_cells_[i].pyramids[f].material_composition[p] =
                            vn::clamp(body_cells_[i].pyramids[f].material_composition[p] + transfer,
                                      vn::fp20_t(0), vn::fp20_t(1));
                    }
                }
            }
        }
    }
}

void VoxelOrganism::circulate_warmth(vn::fp20_t dt) {
    for (size_t i = 0; i < body_cells_.size(); i++) {
        if (!body_cells_[i].active) continue;

        vn::fp20_t src_warmth(0);
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            src_warmth += body_cells_[i].pyramids[f].material_composition[Warmth];
        }
        src_warmth = src_warmth / vn::fp20_t(static_cast<float>(VoxelCell::FACE_COUNT));

        if (src_warmth <= vn::fp20_t(0.7f)) continue;

        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            BCCCoord ncoord = bcc_face_neighbor(body_cells_[i].coord, f);
            for (size_t j = 0; j < body_cells_.size(); j++) {
                if (i == j || !body_cells_[j].active) continue;
                if (!bcc_equal(body_cells_[j].coord, ncoord)) continue;

                vn::fp20_t dst_warmth(0);
                for (uint32_t pf = 0; pf < VoxelCell::FACE_COUNT; pf++) {
                    dst_warmth += body_cells_[j].pyramids[pf].material_composition[Warmth];
                }
                dst_warmth = dst_warmth / vn::fp20_t(static_cast<float>(VoxelCell::FACE_COUNT));

                if (dst_warmth >= src_warmth) continue;

                vn::fp20_t transfer = (src_warmth - dst_warmth) * vn::fp20_t(0.1f) * dt;
                for (uint32_t pf = 0; pf < VoxelCell::FACE_COUNT; pf++) {
                    body_cells_[j].pyramids[pf].material_composition[Warmth] =
                        vn::clamp(body_cells_[j].pyramids[pf].material_composition[Warmth] + transfer,
                                  vn::fp20_t(0), vn::fp20_t(1));
                    body_cells_[i].pyramids[pf].material_composition[Warmth] =
                        vn::clamp(body_cells_[i].pyramids[pf].material_composition[Warmth] - transfer,
                                  vn::fp20_t(0), vn::fp20_t(1));
                }
            }
        }
    }
}

void VoxelOrganism::process_harm(vn::fp20_t dt) {
    float dt_f = dt.to_float();
    accumulated_harm_ = accumulated_harm_ + vn::fp20_t(genome_.metabolism_rate * 0.1f * dt_f * cell_count());

    auto boundary = find_boundary_cells();
    if (boundary.empty()) return;

    for (size_t i = 0; i < body_cells_.size(); i++) {
        if (!body_cells_[i].active) continue;

        vn::fp20_t harm_level(0);
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            harm_level += body_cells_[i].pyramids[f].material_composition[Harm];
        }
        harm_level = harm_level / vn::fp20_t(static_cast<float>(VoxelCell::FACE_COUNT));

        bool is_boundary = false;
        for (auto bi : boundary) {
            if (bi == i) { is_boundary = true; break; }
        }

        if (is_boundary) {
            for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                vn::fp20_t shed = body_cells_[i].pyramids[f].material_composition[Harm] * vn::fp20_t(0.1f) * dt;
                body_cells_[i].pyramids[f].material_composition[Harm] =
                    vn::clamp(body_cells_[i].pyramids[f].material_composition[Harm] - shed,
                              vn::fp20_t(0), vn::fp20_t(1));
            }
        } else {
            uint32_t nearest_boundary = boundary[0];
            vn::fp20_t min_dist(1e10f);
            for (auto bi : boundary) {
                int32_t di = body_cells_[i].coord.i - body_cells_[bi].coord.i;
                int32_t dj = body_cells_[i].coord.j - body_cells_[bi].coord.j;
                int32_t dk = body_cells_[i].coord.k - body_cells_[bi].coord.k;
                vn::fp20_t dist(static_cast<float>(di * di + dj * dj + dk * dk));
                if (dist < min_dist) {
                    min_dist = dist;
                    nearest_boundary = bi;
                }
            }

            vn::fp20_t transfer = harm_level * vn::fp20_t(0.05f) * dt;
            for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                vn::fp20_t removed = body_cells_[i].pyramids[f].material_composition[Harm] * transfer;
                body_cells_[i].pyramids[f].material_composition[Harm] =
                    vn::clamp(body_cells_[i].pyramids[f].material_composition[Harm] - removed,
                              vn::fp20_t(0), vn::fp20_t(1));
                body_cells_[nearest_boundary].pyramids[f].material_composition[Harm] =
                    vn::clamp(body_cells_[nearest_boundary].pyramids[f].material_composition[Harm] + removed,
                              vn::fp20_t(0), vn::fp20_t(1));
            }
        }

        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            body_cells_[i].pyramids[f].strain_accumulator =
                body_cells_[i].pyramids[f].strain_accumulator + harm_level * dt;

            if (body_cells_[i].pyramids[f].strain_accumulator >
                body_cells_[i].pyramids[f].structural_integrity * vn::fp20_t(2)) {
                body_cells_[i].pyramids[f].fractured = true;
            }
        }

        bool all_fractured = true;
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            if (!body_cells_[i].pyramids[f].fractured) {
                all_fractured = false;
                break;
            }
        }
        if (all_fractured) {
            body_cells_[i].active = false;
            if (i == 0) {
                die();
                return;
            }
        }
    }
}

uint32_t VoxelOrganism::try_grow(vn::fp20_t dt) {
    if (cell_count() >= genome_.max_cells) return 0;
    (void)dt;

    uint32_t divisions = 0;

    std::vector<uint32_t> active_indices;
    for (size_t i = 0; i < body_cells_.size(); i++) {
        if (body_cells_[i].active) active_indices.push_back(static_cast<uint32_t>(i));
    }

    for (auto idx : active_indices) {
        if (cell_count() >= genome_.max_cells) break;

        auto& cell = body_cells_[idx];

        vn::fp20_t avg_warmth(0), avg_flux(0), avg_attraction(0);
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            avg_warmth += cell.pyramids[f].material_composition[Warmth];
            avg_flux += cell.pyramids[f].material_composition[Flux];
            avg_attraction += cell.pyramids[f].material_composition[Attraction];
        }
        float inv_faces = 1.0f / static_cast<float>(VoxelCell::FACE_COUNT);
        avg_warmth = avg_warmth * vn::fp20_t(inv_faces);
        avg_flux = avg_flux * vn::fp20_t(inv_faces);
        avg_attraction = avg_attraction * vn::fp20_t(inv_faces);

        if (!GrowthPipeline::check_division_ready(cell, avg_warmth, avg_flux, avg_attraction,
                                                   GrowthPipeline::DEFAULT_DIVISION_ENERGY_THRESHOLD)) {
            continue;
        }

        vn::fp20_t stresses[VoxelCell::FACE_COUNT];
        compute_face_stresses(cell, stresses);

        uint32_t div_face = GrowthPipeline::determine_division_face(cell, stresses);

        BCCCoord neighbor_coord = bcc_face_neighbor(cell.coord, div_face);
        if (is_occupied(neighbor_coord)) continue;

        VoxelCell daughter;
        std::vector<GrowthEvent> events;
        if (!GrowthPipeline::subdivide_cell(cell, vn::fp20_t(0.1f), div_face, daughter, events)) {
            continue;
        }

        body_cells_.push_back(daughter);

        energy_reserve_ = energy_reserve_ - vn::fp20_t(0.1f);
        if (energy_reserve_ < vn::fp20_t(0)) energy_reserve_ = vn::fp20_t(0);

        float progress = static_cast<float>(cell_count()) / static_cast<float>(genome_.max_cells);
        growth_progress_ = vn::fp20_t(progress);

        skelly_dirty_ = true;
        divisions++;
    }

    return divisions;
}

void VoxelOrganism::apply_body_axes() {
    if (body_cells_.empty()) return;

    BCCCoord root_coord = body_cells_[0].coord;

    vn::fp20_t ap_gradient[NumPillars];
    vn::fp20_t dv_gradient[NumPillars];
    vn::fp20_t lr_gradient[NumPillars];

    for (uint32_t p = 0; p < NumPillars; p++) {
        ap_gradient[p] = vn::fp20_t(0.5f);
        dv_gradient[p] = vn::fp20_t(0.5f);
        lr_gradient[p] = vn::fp20_t(0.5f);
    }

    ap_gradient[Awareness] = vn::fp20_t(0.8f);
    ap_gradient[Willpower] = vn::fp20_t(0.6f);
    ap_gradient[Resistance] = vn::fp20_t(0.4f);

    dv_gradient[Force] = vn::fp20_t(0.8f);
    dv_gradient[Integrity] = vn::fp20_t(0.6f);
    dv_gradient[Cohesion] = vn::fp20_t(0.7f);

    lr_gradient[Attraction] = vn::fp20_t(0.8f);
    lr_gradient[Relation] = vn::fp20_t(0.6f);
    lr_gradient[Memory] = vn::fp20_t(0.5f);

    BCCCoord ap_origin = root_coord;
    BCCCoord dv_origin = root_coord;
    BCCCoord lr_origin = root_coord;

    ap_origin.i += 3;
    dv_origin.j += 3;
    lr_origin.k += 3;

    for (auto& cell : body_cells_) {
        if (!cell.active) continue;
        GrowthPipeline::apply_morphogen_gradient(cell, ap_origin, ap_gradient);
        GrowthPipeline::apply_morphogen_gradient(cell, dv_origin, dv_gradient);
        GrowthPipeline::apply_morphogen_gradient(cell, lr_origin, lr_gradient);
    }
}

void VoxelOrganism::repair_damage(vn::fp20_t dt) {
    float dt_f = dt.to_float();

    for (auto& cell : body_cells_) {
        if (!cell.active) continue;

        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            auto& pyramid = cell.pyramids[f];
            if (pyramid.structural_integrity >= vn::fp20_t(1.0f)) continue;

            vn::fp20_t deficit = vn::fp20_t(1.0f) - pyramid.structural_integrity;
            vn::fp20_t cost = deficit * vn::fp20_t(0.01f * dt_f);

            if (energy_reserve_ >= cost) {
                energy_reserve_ = energy_reserve_ - cost;
                pyramid.structural_integrity =
                    vn::clamp(pyramid.structural_integrity + vn::fp20_t(0.05f * dt_f),
                              vn::fp20_t(0), vn::fp20_t(1.0f));
            }
        }
    }
}

void VoxelOrganism::rebuild_skelly_mapping() {
    skelly_bridge_.detect_bone_chains(body_cells_, bone_mappings_);
    skelly_bridge_.detect_joints(bone_mappings_, body_cells_, joint_mappings_, multi_joint_mappings_);
    skelly_bridge_.detect_muscle_groups(bone_mappings_, joint_mappings_, body_cells_, muscle_mappings_);
    skelly_bridge_.detect_organs(body_cells_, organ_mappings_);
    skelly_dirty_ = false;
}

void VoxelOrganism::apply_skelly_physics(vn::fp20_t dt) {
    (void)dt;
    rebuild_if_dirty();
    skelly_bridge_.update_skeleton_from_cells(body_cells_, skeleton_);
    skelly_bridge_.apply_skelly_forces_to_cells(skeleton_, body_cells_);
}

VoxelOrganism* VoxelOrganism::reproduce() {
    VoxelOrganismGenome child_genome = genome_;
    mutate_genome(child_genome);

    VoxelOrganism* child = new VoxelOrganism();

    uint32_t bud_index = 0;
    if (cell_count() > 1) {
        auto boundary = find_boundary_cells();
        if (!boundary.empty()) {
            vn::fp20_t min_stress(1e10f);
            for (auto bi : boundary) {
                vn::fp20_t stresses[VoxelCell::FACE_COUNT];
                compute_face_stresses(body_cells_[bi], stresses);
                for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                    if (stresses[f] < min_stress) {
                        min_stress = stresses[f];
                        bud_index = bi;
                    }
                }
            }
        }
    }

    PillarStateVector child_baseline = baseline_pillars_;
    for (int p = 0; p < NumPillars; p++) {
        float pert = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.1f;
        float val = child_baseline[p] + pert;
        if (val < 0.0f) val = 0.0f;
        if (val > 1.0f) val = 1.0f;
        child_baseline[p] = vn::fp20_t(val);
    }

    child->energy_reserve_ = energy_reserve_ * vn::fp20_t(0.2f);

    VoxelCell bud = body_cells_[bud_index];
    child->body_cells_.push_back(bud);
    child->genome_ = child_genome;
    child->baseline_pillars_ = child_baseline;
    child->pillar_state_ = child_baseline;
    child->constraint_.init();
    child->alive_ = true;

    reproduction_count_++;
    return child;
}

void VoxelOrganism::mutate_genome(VoxelOrganismGenome& base) {
    float r;

    r = static_cast<float>(rand()) / RAND_MAX;
    if (r < 0.05f) {
        base.metabolism_rate *= (1.0f + (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.2f);
    }
    if (base.metabolism_rate < 0.001f) base.metabolism_rate = 0.001f;
    if (base.metabolism_rate > 1.0f) base.metabolism_rate = 1.0f;

    r = static_cast<float>(rand()) / RAND_MAX;
    if (r < 0.05f) {
        base.photosynthesis_efficiency *= (1.0f + (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.2f);
    }
    if (base.photosynthesis_efficiency < 0.0f) base.photosynthesis_efficiency = 0.0f;
    if (base.photosynthesis_efficiency > 1.0f) base.photosynthesis_efficiency = 1.0f;

    r = static_cast<float>(rand()) / RAND_MAX;
    if (r < 0.02f) {
        base.mutation_rate += (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.02f;
    }
    if (base.mutation_rate < 0.001f) base.mutation_rate = 0.001f;
    if (base.mutation_rate > 0.5f) base.mutation_rate = 0.5f;

    r = static_cast<float>(rand()) / RAND_MAX;
    if (r < 0.05f) {
        float delta = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.2f;
        int new_max = static_cast<int>(static_cast<float>(base.max_cells) * (1.0f + delta));
        if (new_max < 1) new_max = 1;
        if (new_max > 4096) new_max = 4096;
        base.max_cells = static_cast<uint32_t>(new_max);
    }

    r = static_cast<float>(rand()) / RAND_MAX;
    if (r < 0.05f) {
        base.memory_retention += (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.1f;
    }
    if (base.memory_retention < 0.0f) base.memory_retention = 0.0f;
    if (base.memory_retention > 1.0f) base.memory_retention = 1.0f;

    for (int i = 0; i < 256; i++) {
        r = static_cast<float>(rand()) / RAND_MAX;
        if (r < 0.02f) {
            base.nn_weights[i] += (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.2f;
            if (base.nn_weights[i] < -1.0f) base.nn_weights[i] = -1.0f;
            if (base.nn_weights[i] > 1.0f) base.nn_weights[i] = 1.0f;
        }
    }

    if (base.memory_retention > 0.0f && !base.ancestor_hashes.empty()) {
        float bias = base.memory_retention * 0.05f;
        for (int i = 0; i < 256; i++) {
            r = static_cast<float>(rand()) / RAND_MAX;
            if (r < bias) {
                uint64_t hash = base.ancestor_hashes.back();
                float direction = ((hash >> (i % 64)) & 1ULL) ? 0.01f : -0.01f;
                base.nn_weights[i] += direction;
                if (base.nn_weights[i] < -1.0f) base.nn_weights[i] = -1.0f;
                if (base.nn_weights[i] > 1.0f) base.nn_weights[i] = 1.0f;
            }
        }
    }
}

void VoxelOrganism::die() {
    alive_ = false;
    for (auto& cell : body_cells_) {
        cell.active = false;
    }
    energy_reserve_ = vn::fp20_t(0);
}

void VoxelOrganism::aggregate_pillar_state() {
    for (uint32_t p = 0; p < NumPillars; p++)
        pillar_state_[p] = vn::fp20_t(0);

    uint32_t count = 0;
    for (const auto& cell : body_cells_) {
        if (!cell.active) continue;
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            for (uint32_t p = 0; p < NumPillars; p++) {
                pillar_state_[p] = pillar_state_[p] + cell.pyramids[f].material_composition[p];
            }
            count++;
        }
    }

    if (count > 0) {
        vn::fp20_t inv_count = vn::fp20_t(1.0f) / vn::fp20_t(static_cast<float>(count));
        for (uint32_t p = 0; p < NumPillars; p++)
            pillar_state_[p] = pillar_state_[p] * inv_count;
    }
}

std::vector<uint32_t> VoxelOrganism::find_boundary_cells() const {
    std::vector<uint32_t> result;

    for (size_t i = 0; i < body_cells_.size(); i++) {
        if (!body_cells_[i].active) continue;

        bool is_boundary = false;
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            BCCCoord ncoord = bcc_face_neighbor(body_cells_[i].coord, f);

            bool occupied_by_self = false;
            for (size_t j = 0; j < body_cells_.size(); j++) {
                if (i == j) continue;
                if (!body_cells_[j].active) continue;
                if (bcc_equal(body_cells_[j].coord, ncoord)) {
                    occupied_by_self = true;
                    break;
                }
            }

            if (!occupied_by_self) {
                is_boundary = true;
                break;
            }
        }

        if (is_boundary) {
            result.push_back(static_cast<uint32_t>(i));
        }
    }

    return result;
}

std::vector<BCCCoord> VoxelOrganism::get_neighbors(const BCCCoord& coord) const {
    std::vector<BCCCoord> result;
    result.reserve(VoxelCell::FACE_COUNT);
    for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
        result.push_back(bcc_face_neighbor(coord, f));
    }
    return result;
}

bool VoxelOrganism::is_occupied(const BCCCoord& coord) const {
    for (const auto& cell : body_cells_) {
        if (!cell.active) continue;
        if (bcc_equal(cell.coord, coord)) return true;
    }
    return false;
}

void VoxelOrganism::compute_face_stresses(const VoxelCell& cell,
                                           vn::fp20_t out_stresses[VoxelCell::FACE_COUNT]) const {
    for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
        BCCCoord ncoord = bcc_face_neighbor(cell.coord, f);

        bool found = false;
        for (const auto& other : body_cells_) {
            if (!other.active) continue;
            if (bcc_equal(other.coord, ncoord)) {
                vn::fp20_t total_diff(0);
                for (uint32_t p = 0; p < NumPillars; p++) {
                    vn::fp20_t cell_avg(0), other_avg(0);
                    for (uint32_t pf = 0; pf < VoxelCell::FACE_COUNT; pf++) {
                        cell_avg += cell.pyramids[pf].material_composition[p];
                        other_avg += other.pyramids[pf].material_composition[p];
                    }
                    cell_avg = cell_avg / vn::fp20_t(static_cast<float>(VoxelCell::FACE_COUNT));
                    other_avg = other_avg / vn::fp20_t(static_cast<float>(VoxelCell::FACE_COUNT));

                    vn::fp20_t diff = cell_avg - other_avg;
                    if (diff < vn::fp20_t(0)) diff = vn::fp20_t() - diff;
                    total_diff += diff;
                }
                out_stresses[f] = total_diff;
                found = true;
                break;
            }
        }

        if (!found) {
            out_stresses[f] = vn::fp20_t(0);
        }
    }
}

void VoxelOrganism::rebuild_if_dirty() {
    if (skelly_dirty_) rebuild_skelly_mapping();
}

GrowthStage VoxelOrganism::growth_stage() const {
    return get_growth_stage_from_ratio(growth_progress_.to_float());
}

vn::fp20_t VoxelOrganism::body_size() const {
    if (body_cells_.empty()) return vn::fp20_t(0);

    int32_t min_i = body_cells_[0].coord.i, max_i = body_cells_[0].coord.i;
    int32_t min_j = body_cells_[0].coord.j, max_j = body_cells_[0].coord.j;
    int32_t min_k = body_cells_[0].coord.k, max_k = body_cells_[0].coord.k;

    for (const auto& cell : body_cells_) {
        if (!cell.active) continue;
        if (cell.coord.i < min_i) min_i = cell.coord.i;
        if (cell.coord.i > max_i) max_i = cell.coord.i;
        if (cell.coord.j < min_j) min_j = cell.coord.j;
        if (cell.coord.j > max_j) max_j = cell.coord.j;
        if (cell.coord.k < min_k) min_k = cell.coord.k;
        if (cell.coord.k > max_k) max_k = cell.coord.k;
    }

    int32_t di = max_i - min_i;
    int32_t dj = max_j - min_j;
    int32_t dk = max_k - min_k;

    vn::fp20_t dx(static_cast<float>(di));
    vn::fp20_t dy(static_cast<float>(dj));
    vn::fp20_t dz(static_cast<float>(dk));

    return vn::fp_sqrt(dx * dx + dy * dy + dz * dz);
}

uint32_t VoxelOrganism::boundary_cell_count() const {
    return static_cast<uint32_t>(find_boundary_cells().size());
}

float VoxelOrganism::calculate_fitness() const {
    float fitness = 0.0f;
    fitness += energy_reserve_.to_float() / 1000.0f * 0.3f;
    fitness += age_.to_float() / 1000.0f * 0.3f;
    fitness += static_cast<float>(reproduction_count_) * 0.15f;
    fitness += static_cast<float>(cell_count()) / static_cast<float>(genome_.max_cells) * 0.15f;
    fitness += overall_health_.to_float() * 0.1f;
    return fitness;
}

VoxelOrganism* create_voxel_organism(const BCCCoord& position,
                                      const VoxelOrganismGenome& dna,
                                      vn::fp20_t scale) {
    (void)scale;
    VoxelOrganism* org = new VoxelOrganism();
    PillarStateVector baseline = create_default_pillar_state(vn::fp20_t(0.5f));
    org->init_zygote(position, YieldMatrix::default_stem(), dna, baseline);
    return org;
}
