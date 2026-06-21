#include "tick_loop.h"
#include "../include/BlochSpace.h"
#include <iostream>
#include <cstring>

using Index = vn::simulation::AgentECS::Index;
#include <algorithm>
#include <cmath>
#include <unordered_map>

SimulationTickLoop::SimulationTickLoop()
    : micro_tick_count_(0), meso_tick_count_(0), macro_tick_count_(0) {
    world_state_.temperature = 0.5f;
    world_state_.hazard_level = 0.1f;
    world_state_.resource_density = 0.7f;
    world_state_.tick_count = 0;
    
    // Reserve space for agents
    agent_ecs_.reserve(1000);
    hopf_cord_field_.init();
}

SimulationTickLoop::~SimulationTickLoop() {
#ifndef VN_USE_NATIVE_REASONING
    if (llm_worker_) {
        llm_worker_->shutdown();
    }
#endif
}

bool SimulationTickLoop::initialize(int max_agents) {
    agent_ecs_.reserve(max_agents);
    groups_.clear();
    chunks_.clear();
    world_state_.temperature = 0.5f;
    world_state_.hazard_level = 0.1f;
    world_state_.resource_density = 0.7f;
    world_state_.tick_count = 0;
    micro_tick_count_ = meso_tick_count_ = macro_tick_count_ = 0;
    bloch_conductors_.resize(max_agents);
    for (auto& bc : bloch_conductors_) bc.init();
    return true;
}

bool SimulationTickLoop::init_native_reasoning(float creativity, float coherence,
                                                 const char* zmq_endpoint) {
    native_worker_ = std::make_unique<NativeReasoningWorker>(creativity, coherence);
    if (!native_worker_->initialize(zmq_endpoint)) {
        std::cerr << "SimulationTickLoop: Native reasoning worker initialization failed" << std::endl;
        native_worker_.reset();
        return false;
    }
    std::cout << "SimulationTickLoop: Native reasoning worker initialized (creativity="
              << creativity << ", coherence=" << coherence << ")" << std::endl;
    return true;
}

bool SimulationTickLoop::request_native_reasoning(vn::simulation::AgentECS::Index idx) {
    if (!native_worker_ || !agent_ecs_.active(idx)) return false;

    LLMWorkerRequest req;
    req.agent_id = idx;
    std::memset(req.wht_coefficients, 0, sizeof(req.wht_coefficients));
    std::strncpy(req.task, "native", LLM_WORKER_TASK_MAX - 1);
    req.task[LLM_WORKER_TASK_MAX - 1] = '\0';

    auto pillars = agent_ecs_.get_pillars(idx);
    float theta[NumPillars];
    for (int i = 0; i < NumPillars; i++) {
        theta[i] = pillars[i];
        req.pillars_theta[i] = theta[i];
    }

    LLMWorker::compress_to_wht(theta, req.wht_coefficients);
    return native_worker_->send_request(req);
}

void SimulationTickLoop::collect_native_responses() {
    if (!native_worker_) return;

    LLMWorkerResponse resp;
    while (native_worker_->poll_response(resp, 0)) {
        vn::simulation::AgentECS::Index idx = resp.agent_id;
        if (!agent_ecs_.active(idx)) continue;

        AgentCognition* cognition = agent_ecs_.cognition(idx);
        if (!cognition) continue;

        PillarVector pv;
        for (int i = 0; i < NumPillars; i++) {
            pv[i] = vn::fp20_t(resp.pillars_theta[i]);
        }

        if (resp.success) {
            cognition->get_pillars() = pv;
        }
    }
}

#ifndef VN_USE_NATIVE_REASONING
bool SimulationTickLoop::init_llm_worker(const char* zmq_endpoint) {
    llm_worker_ = std::make_unique<LLMWorker>();
    if (!llm_worker_->initialize(zmq_endpoint)) {
        std::cerr << "SimulationTickLoop: LLM worker initialization failed (Ollama may not be running)" << std::endl;
        llm_worker_.reset();
        return false;
    }
    std::cout << "SimulationTickLoop: LLM worker initialized on " << zmq_endpoint << std::endl;
    return true;
}

bool SimulationTickLoop::request_agent_reasoning(vn::simulation::AgentECS::Index idx,
                                                   const char* task) {
    if (!llm_worker_ || !agent_ecs_.active(idx)) return false;

    LLMWorkerRequest req;
    req.agent_id = idx;
    std::memset(req.wht_coefficients, 0, sizeof(req.wht_coefficients));
    std::strncpy(req.task, task, LLM_WORKER_TASK_MAX - 1);
    req.task[LLM_WORKER_TASK_MAX - 1] = '\0';

    auto pillars = agent_ecs_.get_pillars(idx);
    float theta[NumPillars];
    for (int i = 0; i < NumPillars; i++) {
        theta[i] = pillars[i];
        req.pillars_theta[i] = theta[i];
    }

    LLMWorker::compress_to_wht(theta, req.wht_coefficients);
    return llm_worker_->send_request(req);
}

void SimulationTickLoop::collect_llm_responses() {
    if (!llm_worker_) return;

    LLMWorkerResponse resp;
    while (llm_worker_->poll_response(resp, 0)) {
        vn::simulation::AgentECS::Index idx = resp.agent_id;
        if (!agent_ecs_.active(idx)) continue;

        AgentCognition* cognition = agent_ecs_.cognition(idx);
        if (!cognition) continue;

        PillarVector pv;
        for (int i = 0; i < NumPillars; i++) {
            pv[i] = vn::fp20_t(resp.pillars_theta[i]);
        }

        if (resp.success) {
            cognition->get_pillars() = pv;
            if (resp.reasoning[0]) {
                (void)resp.reasoning;
            }
        }
    }
}
#endif

bool SimulationTickLoop::init_gpu(VkDevice device, VkPhysicalDevice physicalDevice,
                                   VkQueue computeQueue, uint32_t computeFamily,
                                   uint32_t maxEntities) {
    transform_compute_ = std::make_unique<TransformCompute>();
    bool ok = transform_compute_->init(device, physicalDevice, computeQueue,
                                        computeFamily, maxEntities);
    if (!ok) {
        std::cerr << "WARNING: TransformCompute GPU init failed; using CPU fallback" << std::endl;
        transform_compute_.reset();
    } else {
        std::cout << "SimulationTickLoop: GPU TRANSFORM compute initialized (max " << maxEntities << " entities)" << std::endl;
    }

    hopf_compute_ = std::make_unique<HopfCompute>();
    ok = hopf_compute_->init(device, physicalDevice, computeQueue,
                              computeFamily, maxEntities);
    if (!ok) {
        std::cerr << "WARNING: HopfCompute GPU init failed; using CPU fallback" << std::endl;
        hopf_compute_.reset();
        return false;
    }
    std::cout << "SimulationTickLoop: GPU Hopf-PID compute initialized (max " << maxEntities << " entities)" << std::endl;

    cuda_compute_ = std::make_unique<CUDAPillarCompute>();
    ok = cuda_compute_->init(maxEntities);
    if (!ok) {
        std::cerr << "WARNING: CUDAPillarCompute GPU init failed; using CPU fallback" << std::endl;
        cuda_compute_.reset();
    } else {
        std::cout << "SimulationTickLoop: CUDA pillar compute initialized (max " << maxEntities << " entities)" << std::endl;
    }
    return true;
}

SimulationTickLoop::SpatialHashKey SimulationTickLoop::hash_cell(int cx, int cy, int cz) {
    return ((static_cast<uint64_t>(cx) & 0xFFFFF) << 40) |
           ((static_cast<uint64_t>(cy) & 0xFFFFF) << 20) |
           (static_cast<uint64_t>(cz) & 0xFFFFF);
}

void SimulationTickLoop::get_cell_coords(float x, float y, float z, int& cx, int& cy, int& cz) {
    cx = static_cast<int>(std::floor(x / SPATIAL_HASH_CELL_SIZE));
    cy = static_cast<int>(std::floor(y / SPATIAL_HASH_CELL_SIZE));
    cz = static_cast<int>(std::floor(z / SPATIAL_HASH_CELL_SIZE));
}

void SimulationTickLoop::build_spatial_hash() {
    spatial_hash_.clear();
    for (Index idx = 0; idx < agent_ecs_.size(); ++idx) {
        if (!agent_ecs_.active(idx)) continue;
        float x = agent_ecs_.x(idx);
        float y = agent_ecs_.y(idx);
        float z = agent_ecs_.z(idx);
        int cx, cy, cz;
        get_cell_coords(x, y, z, cx, cy, cz);
        SpatialHashKey key = hash_cell(cx, cy, cz);
        spatial_hash_[key].push_back(static_cast<int>(idx));
    }
}

void SimulationTickLoop::query_nearby_agents(vn::simulation::AgentECS::Index self_idx, int* nearby_indices, int max_nearby, int& count) const {
    count = 0;
    float sx = agent_ecs_.x(self_idx);
    float sy = agent_ecs_.y(self_idx);
    float sz = agent_ecs_.z(self_idx);
    int center_cx, center_cy, center_cz;
    get_cell_coords(sx, sy, sz, center_cx, center_cy, center_cz);
    
    const int radius_in_cells = static_cast<int>(std::ceil(INTERACTION_RADIUS / SPATIAL_HASH_CELL_SIZE));
    
    for (int dx = -radius_in_cells; dx <= radius_in_cells; dx++) {
        for (int dy = -radius_in_cells; dy <= radius_in_cells; dy++) {
            for (int dz = -radius_in_cells; dz <= radius_in_cells; dz++) {
                int cell_id = hash_cell(center_cx + dx, center_cy + dy, center_cz + dz);
                auto it = spatial_hash_.find(cell_id);
                if (it != spatial_hash_.end()) {
                    const std::vector<int>& neighbors = it->second;
                    for (int neighbor : neighbors) {
                        if (neighbor == static_cast<int>(self_idx)) continue;
                        if (count >= max_nearby) return;
                        nearby_indices[count] = neighbor;
                        count++;
                    }
                }
            }
        }
    }
}

void SimulationTickLoop::gather_resources(vn::simulation::AgentECS::Index idx) {
    float x = agent_ecs_.x(idx);
    float y = agent_ecs_.y(idx);
    float z = agent_ecs_.z(idx);
    int cx = static_cast<int>(std::floor(x / CHUNK_SIZE));
    int cy = static_cast<int>(std::floor(y / CHUNK_SIZE));
    int cz = static_cast<int>(std::floor(z / CHUNK_SIZE));

    for (auto& chunk : chunks_) {
        if (!chunk) continue;
        if (chunk->get_chunk_x() != cx || chunk->get_chunk_y() != cy || chunk->get_chunk_z() != cz) continue;

        int lx = static_cast<int>(x) - cx * CHUNK_SIZE;
        int ly = static_cast<int>(y) - cy * CHUNK_SIZE;
        int lz = static_cast<int>(z) - cz * CHUNK_SIZE;

        int gathered = 0;
        const int R = 2;
        for (int dz = -R; dz <= R; dz++) {
            for (int dy = -R; dy <= R; dy++) {
                for (int dx = -R; dx <= R; dx++) {
                    int vx = lx + dx;
                    int vy = ly + dy;
                    int vz = lz + dz;
                    if (!Chunk::is_valid_pos(vx, vy, vz)) continue;
                    Voxel& v = chunk->get_voxel(vx, vy, vz);
                    if (v.material == MATERIAL_RESOURCE) {
                        v.material = 0;
                        gathered++;
                    }
                }
            }
        }

        // Update resources in ECS
        int& resources = agent_ecs_.resources(idx);
        resources += gathered;
        if (gathered > 0) {
            chunk->set_dirty(true);
        }
        return;
    }

    world_state_.resource_density = std::clamp(
        world_state_.resource_density + 0.001f, 0.0f, 1.0f);
}

void SimulationTickLoop::place_voxels(vn::simulation::AgentECS::Index idx) {
    if (agent_ecs_.resources(idx) <= 0) return;

    float x = agent_ecs_.x(idx);
    float y = agent_ecs_.y(idx);
    float z = agent_ecs_.z(idx);
    int cx = static_cast<int>(std::floor(x / CHUNK_SIZE));
    int cy = static_cast<int>(std::floor(y / CHUNK_SIZE));
    int cz = static_cast<int>(std::floor(z / CHUNK_SIZE));

    for (auto& chunk : chunks_) {
        if (!chunk) continue;
        if (chunk->get_chunk_x() != cx || chunk->get_chunk_y() != cy || chunk->get_chunk_z() != cz) continue;

        int lx = static_cast<int>(x) - cx * CHUNK_SIZE;
        int ly = static_cast<int>(y) - cy * CHUNK_SIZE;
        int lz = static_cast<int>(z) - cz * CHUNK_SIZE;

        if (!Chunk::is_valid_pos(lx, ly, lz)) return;

        Voxel& existing = chunk->get_voxel(lx, ly, lz);
        if (existing.material != 0) return;

        Voxel built = {1, 128, 128, 128, 255};
        chunk->set_voxel(lx, ly, lz, built);
        // Decrement resources in ECS
        agent_ecs_.resources(idx)--;
        return;
    }
}

void SimulationTickLoop::update_agent_perception(vn::simulation::AgentECS::Index idx) {
    // Get voxels + nearby agents
    // This function is a placeholder; implement actual perception logic.
    (void)idx;
}

void SimulationTickLoop::update_agent(vn::simulation::AgentECS::Index idx, float delta_time) {
    // Update agent state based on perception, cognition, and physics
    // This is a placeholder; implement actual agent update logic.
    if (!agent_ecs_.active(idx)) return;
    
    // Get agent's cognition
    AgentCognition* cognition = agent_ecs_.cognition(idx);
    if (!cognition) return;
    
    // Update cognition (this will process perceptions and make decisions)
    cognition->update(delta_time);
    
    // Get the decided action
    AgentAction action = cognition->decide();
    
    // Apply the action (movement, building, etc.)
    // This is simplified - actual implementation would depend on the action
    float& vx = agent_ecs_.vx(idx);
    float& vy = agent_ecs_.vy(idx);
    float& vz = agent_ecs_.vz(idx);
    
    // Simple movement based on action (for demonstration)
    switch (action) {
        case AgentAction::MOVE:
            // Apply some random movement for now
            vx += (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * 0.1f * delta_time;
            vy += (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * 0.1f * delta_time;
            vz += (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * 0.1f * delta_time;
            break;
        case AgentAction::GATHER:
            gather_resources(idx);
            break;
        case AgentAction::BUILD:
            place_voxels(idx);
            break;
        default:
            // IDLE, COMMUNICATE, DEFEND, FLEE, EXPLORE - no movement by default
            break;
    }
    
    // Apply velocity to position
    float& x = agent_ecs_.x(idx);
    float& y = agent_ecs_.y(idx);
    float& z = agent_ecs_.z(idx);
    x += vx * delta_time;
    y += vy * delta_time;
    z += vz * delta_time;
    
    // Mark spatial hash as dirty if moved significantly
    float last_hx = agent_ecs_.last_hash_x(idx);
    float last_hy = agent_ecs_.last_hash_y(idx);
    float last_hz = agent_ecs_.last_hash_z(idx);
    float dx = x - last_hx;
    float dy = y - last_hy;
    float dz = z - last_hz;
    float dist_moved = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (dist_moved > SPATIAL_HASH_CELL_SIZE * 0.5f) {
        agent_ecs_.last_hash_x(idx) = x;
        agent_ecs_.last_hash_y(idx) = y;
        agent_ecs_.last_hash_z(idx) = z;
        spatial_hash_dirty_ = true;
    }
}

void SimulationTickLoop::meso_tick(float delta_time) {
    // Form groups based on proximity and relation values
    form_groups();
    
    // Update group dynamics
    for (auto& group : groups_) {
        // Calculate average pillar values
        if (group.agent_ids.empty()) continue;
        
        float sum[NumPillars] = {0};
        for (int agent_id : group.agent_ids) {
            // agent_id is the index in the ECS
            if (agent_id >= 0 && static_cast<size_t>(agent_id) < agent_ecs_.size()) {
                auto pillars = agent_ecs_.get_pillars(static_cast<vn::simulation::AgentECS::Index>(agent_id));
                for (int i = 0; i < NumPillars; i++) {
                    sum[i] += pillars[i];
                }
            }
        }
        
        for (int i = 0; i < NumPillars; i++) {
            group.avg_pillar_values[i] = sum[i] / group.agent_ids.size();
        }
        
        // Cohesion affects group stability
        group.cohesion_level = group.avg_pillar_values[6];  // Cohesion pillar
    }
    
    // Fracture tick: agent pillar forces -> voxel yield -> splintering
    fracture_tick(delta_time);

    // Agent-agent Hopf transforms: coupled Bloch rotation within groups
    hopf_transform_pairs();

    // Entity->voxel deformation coupling: agent pillars -> vertex forces -> Verlet integration
    std::vector<Entity> entities;
    for (vn::simulation::AgentECS::Index idx = 0; idx < agent_ecs_.size(); ++idx) {
        if (!agent_ecs_.active(idx)) continue;
        Entity e;
        e.id = static_cast<int>(idx);
        e.native_scale = SCALE_ORGANISM;
        e.pos_x = vn::fp20_t(agent_ecs_.x(idx));
        e.pos_y = vn::fp20_t(agent_ecs_.y(idx));
        auto ap = agent_ecs_.get_pillars(idx);
        for (int i = 0; i < NumPillars; i++)
            e.pillars[i] = ap[i];
        entities.push_back(e);
    }
    if (!entities.empty()) {
        for (auto& chunk : chunks_) {
            if (chunk) {
                tick_entity_voxel_coupling(entities, *chunk, deformation_map_, vn::fp20_t(delta_time));
            }
        }
    }
}

void SimulationTickLoop::form_groups() {
    groups_.clear();
    spatial_hash_.clear();
    
    // Simple grouping by proximity (same chunk)
    for (vn::simulation::AgentECS::Index idx = 0; idx < agent_ecs_.size(); ++idx) {
        if (!agent_ecs_.active(idx)) continue;
        float x = agent_ecs_.x(idx);
        float y = agent_ecs_.y(idx);
        float z = agent_ecs_.z(idx);
        int cx, cy, cz;
        get_cell_coords(x, y, z, cx, cy, cz);
        SpatialHashKey key = hash_cell(cx, cy, cz);
        spatial_hash_[key].push_back(static_cast<int>(idx));
    }
}

void SimulationTickLoop::macro_tick(float delta_time) {
    // Update world state
    update_world(delta_time);
    
    // Update pillar vectors for all agents
    update_pillar_vectors();

    // Log Depth utilization stats from BlochConductor (1 Hz)
    size_t agent_count = 0;
    size_t blackbox_warnings = 0;
    size_t total_sub_pillars = 0;
    size_t active_agents = 0;
    for (auto& bc : bloch_conductors_) {
        if (bc.depth_monitor.ratio < 0.0f) continue; // uninitialized
        agent_count++;
        if (bc.depth_monitor.is_blackbox_warning()) blackbox_warnings++;
        total_sub_pillars += static_cast<size_t>(bc.active_sub_pillar_count());
    }
    for (Index idx = 0; idx < agent_ecs_.size(); ++idx) {
        if (agent_ecs_.active(idx)) active_agents++;
    }
    if (agent_count > 0) {
        std::cout << "[Depth] " << active_agents << " agents, "
                  << agent_count << " monitored, "
                  << blackbox_warnings << " blackbox warnings, "
                  << total_sub_pillars << " active sub-pillars" << std::endl;
    }
}

void SimulationTickLoop::update_world(float delta_time) {
    // Simple environmental changes
    world_state_.temperature += 0.001f * sin(world_state_.tick_count * 0.1f);
    world_state_.hazard_level *= 0.99f;  // Hazard slowly decreases
    world_state_.resource_density -= 0.0001f;  // Resources slowly deplete
}

void SimulationTickLoop::hopf_ensure_state_size() {
    size_t current = hopf_states_.size();
    size_t needed = agent_ecs_.size();
    if (needed > current) {
        hopf_states_.resize(needed);
        for (size_t i = current; i < needed; i++) {
            hopf_states_[i].init();
        }
    }
}

HopfPIDState& SimulationTickLoop::get_hopf_state(vn::simulation::AgentECS::Index idx) {
    hopf_ensure_state_size();
    return hopf_states_[static_cast<size_t>(idx)];
}

void SimulationTickLoop::hopf_apply_environment(
    HopfPIDState& state, float hazard, float resource_density
) {
    // High hazard rotates Integrity away from 1.0 on the Bloch sphere
    if (hazard > 1e-6f) {
        float current_integrity = state.frames[0][Integrity];
        float theta = bloch_value_to_theta(current_integrity);
        float target_theta = bloch_value_to_theta(0.1f);  // hazard pushes toward 0.1
        float delta = (target_theta - theta) * hazard * 0.1f;
        if (std::fabs(delta) > PLANCK_THETA_EPS) {
            state.frames[0][Integrity] = bloch_rotate(
                vn::fp20_t(state.frames[0][Integrity]), vn::fp20_t(delta)
            );
        }
    }

    // Resource density rotates Attraction on the Bloch sphere
    float current_attr = state.frames[0][Attraction];
    float theta_attr = bloch_value_to_theta(current_attr);
    float target_theta_attr = bloch_value_to_theta(resource_density);
    float delta_attr = (target_theta_attr - theta_attr) * 0.1f;
    if (std::fabs(delta_attr) > PLANCK_THETA_EPS) {
        state.frames[0][Attraction] = bloch_rotate(
            vn::fp20_t(state.frames[0][Attraction]), vn::fp20_t(delta_attr)
        );
    }

    state.encode_all_frames();
    state.relational_complexity = compute_relational_complexity(state);
}

void SimulationTickLoop::hopf_tick_all(
    float delta_time, float hazard, float resource_density, bool apply_env
) {
    hopf_ensure_state_size();
    PlanckCapConfig cfg = PlanckCapConfig::engine_default();

    for (Index idx = 0; idx < agent_ecs_.size(); ++idx) {
        if (!agent_ecs_.active(idx)) continue;

        HopfPIDState& hstate = get_hopf_state(idx);

        // Sync frame 0 from ECS (primary pillar state)
        auto psv = agent_ecs_.get_pillars(idx);
        for (int p = 0; p < NumPillars; p++) {
            hstate.frames[0][p] = psv[p];
        }
        for (int f = 1; f < HOPF_FRAME_COUNT; f++) {
            hstate.frames[f] = hstate.frames[0];
        }
        hstate.encode_all_frames();

        // Run the full Hopf-PID tick
        hopf_tick(hstate, hopf_cord_field_, static_cast<uint32_t>(idx), delta_time, cfg);

        // Apply environmental effects as Bloch rotations (skip if GPU decay phase already ran)
        if (apply_env) {
            hopf_apply_environment(hstate, hazard, resource_density);
        }

        // Write frame 0 back to ECS
        for (int p = 0; p < NumPillars; p++) {
            agent_ecs_.pillar(idx, static_cast<PillarIndex>(p)) = hstate.frames[0][p];
        }
    }
}

void SimulationTickLoop::hopf_transform_pairs() {
    hopf_ensure_state_size();

    for (Index idx = 0; idx < agent_ecs_.size(); ++idx) {
        if (!agent_ecs_.active(idx)) continue;
        auto psv = agent_ecs_.get_pillars(idx);
        HopfPIDState& hstate = hopf_states_[static_cast<size_t>(idx)];
        for (int p = 0; p < NumPillars; p++)
            hstate.frames[0][p] = psv[p];
        for (int f = 1; f < HOPF_FRAME_COUNT; f++)
            hstate.frames[f] = hstate.frames[0];
        hstate.encode_all_frames();
    }

    for (const auto& group : groups_) {
        const auto& ids = group.agent_ids;
        if (ids.size() < 2) continue;

        const struct { int op; int tgt; } interactions[] = {
            {Force, Integrity},
            {Influence, Attraction},
            {Warmth, Cohesion},
            {Harm, Resistance},
        };

        for (size_t i = 0; i < ids.size(); i++) {
            Index idx_a = static_cast<Index>(ids[i]);
            if (!agent_ecs_.active(idx_a)) continue;
            HopfPIDState& actor = hopf_states_[static_cast<size_t>(idx_a)];

            for (size_t j = i + 1; j < ids.size(); j++) {
                Index idx_b = static_cast<Index>(ids[j]);
                if (!agent_ecs_.active(idx_b)) continue;
                HopfPIDState& subject = hopf_states_[static_cast<size_t>(idx_b)];

                for (auto& interaction : interactions) {
                    hopf_transform(actor, subject, interaction.op, interaction.tgt,
                                   subject.depth_buffer, subject.constraint_level);
                    hopf_transform(subject, actor, interaction.op, interaction.tgt,
                                   actor.depth_buffer, actor.constraint_level);
                }
            }
        }
    }

    for (Index idx = 0; idx < agent_ecs_.size(); ++idx) {
        if (!agent_ecs_.active(idx)) continue;
        HopfPIDState& hstate = hopf_states_[static_cast<size_t>(idx)];
        for (int p = 0; p < NumPillars; p++) {
            agent_ecs_.pillar(idx, static_cast<PillarIndex>(p)) = hstate.frames[0][p];
        }
    }
}

void SimulationTickLoop::update_pillar_vectors() {
    // GPU Hopf-PID path (preferred — full simulation)
    if (hopf_compute_ && hopf_compute_->isReady()) {
        hopf_ensure_state_size();

        // Sync hopf_states_ from ECS before uploading to GPU
        for (Index idx = 0; idx < agent_ecs_.size() && idx < static_cast<Index>(hopf_states_.size()); ++idx) {
            if (!agent_ecs_.active(idx)) continue;
            auto psv = agent_ecs_.get_pillars(idx);
            HopfPIDState& hstate = hopf_states_[static_cast<size_t>(idx)];
            for (int p = 0; p < NumPillars; p++)
                hstate.frames[0][p] = psv[p];
            for (int f = 1; f < HOPF_FRAME_COUNT; f++)
                hstate.frames[f] = hstate.frames[0];
            hstate.encode_all_frames();
        }

        hopf_compute_->upload(hopf_states_);
        hopf_compute_->dispatch(dt_, 2, PLANCK_THETA_EPS, 0.95f);
        hopf_compute_->download(hopf_states_);

        // Write frame 0 back to ECS pillars
        for (Index idx = 0; idx < agent_ecs_.size() && idx < static_cast<Index>(hopf_states_.size()); ++idx) {
            if (!agent_ecs_.active(idx)) continue;
            for (int p = 0; p < NumPillars; p++) {
                agent_ecs_.pillar(idx, static_cast<PillarIndex>(p)) =
                    hopf_states_[static_cast<size_t>(idx)].frames[0][p];
            }
        }
        return;
    }

    // GPU CUDA pillar compute path (native nvcc kernels)
    if (cuda_compute_ && cuda_compute_->isReady()) {
        cuda_compute_->upload(agent_ecs_);
        cuda_compute_->dispatch(dt_, world_state_.hazard_level,
                                world_state_.resource_density);
        cuda_compute_->download(agent_ecs_);
        return;
    }

    // GPU TRANSFORM path (fallback — simpler, legacy)
    if (transform_compute_ && transform_compute_->isReady()) {
        transform_compute_->upload(agent_ecs_);
        transform_compute_->dispatch(dt_, 0,
                                     world_state_.hazard_level,
                                     world_state_.resource_density);
        transform_compute_->dispatch(dt_, 1,
                                     world_state_.hazard_level,
                                     world_state_.resource_density);
        transform_compute_->dispatch(dt_, 2,
                                     world_state_.hazard_level,
                                     world_state_.resource_density);
        transform_compute_->download(agent_ecs_);
        return;
    }

    // CPU fallback: Hopf-PID engine for all agents (includes environment)
    hopf_tick_all(dt_, world_state_.hazard_level, world_state_.resource_density);
}

int SimulationTickLoop::add_agent(float x, float y, float z) {
    // Add to ECS
    auto cognition = std::make_unique<AgentCognition>(static_cast<int>(agent_ecs_.size()));
    Index idx = agent_ecs_.add_agent(
        PillarStateVector(),  // initial pillars (will be set by cognition? but cognition starts with zeros)
        x, y, z,
        0.0f, 0.0f, 0.0f,   // initial velocity
        true,                 // active
        0,                    // resources
        x, y, z,              // last_hash_x, last_hash_y, last_hash_z
        std::move(cognition)
    );
    // Note: The cognition is now stored in the ECS. The agent's id in the ECS is the index.
    // We don't have an explicit agent id separate from the index in the ECS.
    // If we need to return an agent id that is stable, we might need to change the ECS to store an id.
    // For now, we return the index as the agent id.
    spatial_hash_dirty_ = true;  // New agent added, rebuild hash
    return static_cast<int>(idx);
}

void SimulationTickLoop::tick(float delta_time) {
    micro_tick(delta_time);
    micro_tick_count_++;
    if (micro_tick_count_ % (MICRO_TICKS_PER_SECOND / MESO_TICKS_PER_SECOND) == 0) {
        meso_tick(delta_time);
    }
    if (micro_tick_count_ >= MICRO_TICKS_PER_SECOND) {
        micro_tick_count_ = 0;
        macro_tick(delta_time);
    }
}

void SimulationTickLoop::micro_tick(float delta_time) {
    dt_ = delta_time;

    if (native_worker_) {
        collect_native_responses();
    }
#ifndef VN_USE_NATIVE_REASONING
    else {
        collect_llm_responses();
    }
#endif

    for (Index idx = 0; idx < agent_ecs_.size(); ++idx) {
        if (!agent_ecs_.active(idx)) continue;
        update_agent_perception(idx);
        update_agent(idx, delta_time);
    }

    // Periodically request reasoning for a random agent (every 60 ticks)
    if (micro_tick_count_ % 60 == 0) {
        int active_count = 0;
        for (Index idx = 0; idx < agent_ecs_.size(); ++idx) {
            if (agent_ecs_.active(idx)) active_count++;
        }
        if (active_count > 0) {
            int target = std::rand() % active_count;
            int nth = 0;
            for (Index idx = 0; idx < agent_ecs_.size(); ++idx) {
                if (!agent_ecs_.active(idx)) continue;
                if (nth++ == target) {
                    if (native_worker_) {
                        request_native_reasoning(idx);
                    }
#ifndef VN_USE_NATIVE_REASONING
                    else {
                        request_agent_reasoning(idx);
                    }
#endif
                    break;
                }
            }
        }
    }

    std::vector<Entity> observers;
    for (Index idx = 0; idx < agent_ecs_.size(); ++idx) {
        if (!agent_ecs_.active(idx)) continue;
        Entity e;
        e.id = static_cast<int>(idx); // agent id is the index in ECS
        e.native_scale = SCALE_ORGANISM;
        auto p = agent_ecs_.get_pillars(idx);
        for (int i = 0; i < NumPillars; i++) e.pillars[i] = p[i];
        observers.push_back(e);
    }

    std::vector<Entity> all_entities = observers;
    AttentionStepResult ar = attention_loop_.step(observers, all_entities, influence_buf_, micro_tick_count_);
    (void)ar;

    // Per-agent Bloch Depth utilization & crystallization monitoring
    for (Index idx = 0; idx < agent_ecs_.size() && idx < static_cast<Index>(bloch_conductors_.size()); ++idx) {
        if (!agent_ecs_.active(idx)) continue;
        PillarStateVector psv = agent_ecs_.get_pillars(idx);
        bloch_conductors_[static_cast<size_t>(idx)].tick(psv, dt_);
    }

    // Cord phase synchronization: age cords and sync tau/phi across entangled entities
    {
        uint32_t n = static_cast<uint32_t>(agent_ecs_.size());
        std::vector<PillarStateVector> all_states(n);
        for (Index idx = 0; idx < n; ++idx) {
            if (agent_ecs_.active(idx))
                all_states[idx] = agent_ecs_.get_pillars(idx);
        }
        hopf_cord_field_.tick_all(dt_);
        hopf_cord_field_.sync_all_phases(all_states.data(), n);
        for (Index idx = 0; idx < n; ++idx) {
            if (agent_ecs_.active(idx)) {
                agent_ecs_.pillar(idx, Flux) = vn::fp20_t(all_states[idx][Flux]);
                agent_ecs_.pillar(idx, Relation) = vn::fp20_t(all_states[idx][Relation]);
            }
        }
    }
}

void SimulationTickLoop::remove_agent(int agent_id) {
    // agent_id is the index in the ECS
    if (agent_id >= 0 && static_cast<size_t>(agent_id) < agent_ecs_.size()) {
        agent_ecs_.active(static_cast<vn::simulation::AgentECS::Index>(agent_id)) = false;
        spatial_hash_dirty_ = true;  // Agent removed, rebuild hash
    }
}

void SimulationTickLoop::fracture_tick(float delta_time) {
    (void)delta_time;
    if (agent_ecs_.size() == 0) return;

    // Build spatial lookup of agent chunk occupancy for O(n) pairing
    std::unordered_map<uint64_t, std::vector<vn::simulation::AgentECS::Index>> agent_chunks;
    for (vn::simulation::AgentECS::Index idx = 0; idx < agent_ecs_.size(); ++idx) {
        if (!agent_ecs_.active(idx)) continue;
        float x = agent_ecs_.x(idx);
        float y = agent_ecs_.y(idx);
        float z = agent_ecs_.z(idx);
        int cx = static_cast<int>(std::floor(x / CHUNK_SIZE));
        int cy = static_cast<int>(std::floor(y / CHUNK_SIZE));
        int cz = static_cast<int>(std::floor(z / CHUNK_SIZE));
        uint64_t ck = hash_cell(cx, cy, cz);
        agent_chunks[ck].push_back(idx);
    }
    if (agent_chunks.empty()) return;

    // For each chunk that has both active voxel cells and nearby agents
    for (auto& chunk : chunks_) {
        if (!chunk || chunk->active_cell_count() == 0) continue;
        uint64_t ck = hash_cell(chunk->get_chunk_x(), chunk->get_chunk_y(), chunk->get_chunk_z());
        auto ait = agent_chunks.find(ck);
        if (ait == agent_chunks.end()) continue;

        // Collect force contributions from all agents in this chunk
        vn::fp20_t applied_forces[NumPillars] = {vn::fp20_t(0)};
        for (vn::simulation::AgentECS::Index idx : ait->second) {
            auto ap = agent_ecs_.get_pillars(idx);
            for (int i = 0; i < NumPillars; i++)
                applied_forces[i] = applied_forces[i] + ap[i];
        }

        // Collect keys for safe iteration (splintering may insert new cells)
        std::vector<uint64_t> keys;
        for (const auto& [k, cell] : chunk->get_active_cells())
            if (cell.active) keys.push_back(k);

        std::vector<BCCCoord> spawned_children;
        std::vector<FractureEvent> events;
        YieldMatrix parent_mat;  // default fallback

        for (uint64_t key : keys) {
            VoxelCell* cell = chunk->get_active_cell(key);
            if (!cell || !cell->active) continue;

            uint32_t fractured_face;
            if (FracturePipeline::check_yield(*cell, applied_forces, fractured_face)) {
                FracturePipeline::process_fracture(*cell, fractured_face,
                    influence_buf_, spawned_children, events);
                parent_mat = cell->material;
            }
        }

        // Spawn child cells from splintered faces (child BCC → chunk-local coords)
        for (const BCCCoord& child : spawned_children) {
            uint32_t lx = static_cast<uint32_t>(child.i & (CHUNK_SIZE - 1));
            uint32_t ly = static_cast<uint32_t>(child.j & (CHUNK_SIZE - 1));
            uint32_t lz = static_cast<uint32_t>(child.k & (CHUNK_SIZE - 1));
            chunk->promote_to_active(lx, ly, lz, parent_mat);
        }

        if (!events.empty())
            chunk->set_dirty(true);
    }
}

void SimulationTickLoop::update_chunks() {
    for (auto& chunk : chunks_) {
        if (chunk && chunk->is_dirty()) {
            chunk->update_svo();
        }
    }
}