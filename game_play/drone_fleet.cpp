// drone_fleet.cpp - Drone Fleet Implementation
// Route planning, fleet coordination, terrain-aware navigation

#include "drone_fleet.h"
#include <algorithm>
#include <cstring>

namespace vn {
namespace gameplay {

const float TERRAIN_GRID_SIZE = 10.0f;
const float WAYPOINT_THRESHOLD = 2.0f;
const float COLLISION_RADIUS = 3.0f;

Drone::Drone(uint32_t drone_id, uint32_t owner_id)
    : drone_id_(drone_id)
    , owner_id_(owner_id)
    , status_(DroneStatus::IDLE)
    , battery_(100.0f)
    , max_battery_(100.0f)
    , speed_(20.0f)
    , cargo_capacity_(50.0f)
    , current_load_(0.0f)
    , current_waypoint_index_(0)
{
    for (int i = 0; i < NUM_PILLARS; ++i) {
        pillars_[i] = (i < 3) ? 1.0f : 0.0f;
    }
    pillars_[0] = 0.8f;
    pillars_[1] = 0.7f;
    pillars_[2] = 0.9f;
}

Drone::~Drone() = default;

void Drone::update(float delta_time) {
    if (status_ == DroneStatus::IDLE || status_ == DroneStatus::RECHARGING) {
        return;
    }

    if (!current_route_.waypoints.empty() && 
        current_waypoint_index_ < current_route_.waypoints.size()) {
        move_toward_waypoint(delta_time);
    }

    float movement_cost = velocity_.distance_to(Vec3(0, 0, 0)) * 0.1f;
    battery_ = std::max(0.0f, battery_ - movement_cost * delta_time);

    update_pillars_from_behavior();

    if (needs_recharge()) {
        status_ = DroneStatus::EMERGENCY;
    }
}

void Drone::move_toward_waypoint(float delta_time) {
    if (current_waypoint_index_ >= current_route_.waypoints.size()) {
        status_ = DroneStatus::IDLE;
        clear_route();
        return;
    }

    const Waypoint& target = current_route_.waypoints[current_waypoint_index_];
    Vec3 direction = target.position;
    direction.x -= position_.x;
    direction.y -= position_.y;
    direction.z -= position_.z;

    float dist = std::sqrt(direction.x * direction.x + 
                           direction.y * direction.y + 
                           direction.z * direction.z);

    if (dist < WAYPOINT_THRESHOLD) {
        current_waypoint_index_++;
        if (current_waypoint_index_ >= current_route_.waypoints.size()) {
            status_ = (status_ == DroneStatus::DELIVERING) ? 
                      DroneStatus::RETURNING : DroneStatus::IDLE;
            clear_route();
        }
        return;
    }

    if (dist > 0.001f) {
        float inv_dist = 1.0f / dist;
        velocity_.x = direction.x * inv_dist * speed_;
        velocity_.y = direction.y * inv_dist * speed_;
        velocity_.z = direction.z * inv_dist * speed_;
    }

    position_.x += velocity_.x * delta_time;
    position_.y += velocity_.y * delta_time;
    position_.z += velocity_.z * delta_time;
}

bool Drone::assign_route(const Route& route) {
    if (route.waypoints.empty() || !route.is_valid) {
        return false;
    }

    if (battery_ < route.total_distance * 0.5f) {
        return false;
    }

    current_route_ = route;
    current_waypoint_index_ = 0;
    status_ = (status_ == DroneStatus::IDLE) ? DroneStatus::MOVING : status_;
    return true;
}

void Drone::clear_route() {
    current_route_.waypoints.clear();
    current_waypoint_index_ = 0;
}

bool Drone::add_cargo(const CargoManifest& cargo) {
    if (current_load_ + cargo.weight > cargo_capacity_) {
        return false;
    }
    cargo_.push_back(cargo);
    current_load_ += cargo.weight;
    return true;
}

bool Drone::remove_cargo(uint32_t genome_id) {
    auto it = std::find_if(cargo_.begin(), cargo_.end(),
        [genome_id](const CargoManifest& c) { return c.genome_id == genome_id; });
    
    if (it != cargo_.end()) {
        current_load_ -= it->weight;
        cargo_.erase(it);
        return true;
    }
    return false;
}

void Drone::clear_cargo() {
    cargo_.clear();
    current_load_ = 0.0f;
}

void Drone::set_position(const Vec3& pos) {
    position_ = pos;
}

void Drone::set_status(DroneStatus status) {
    status_ = status;
}

void Drone::set_pillar_values(const float pillars[NUM_PILLARS]) {
    std::memcpy(pillars_, pillars, sizeof(float) * NUM_PILLARS);
}

void Drone::recharge(float amount) {
    battery_ = std::min(max_battery_, battery_ + amount);
    if (battery_ > 30.0f && status_ == DroneStatus::EMERGENCY) {
        status_ = DroneStatus::IDLE;
    }
}

float Drone::compute_awareness() {
    float awareness = 0.5f;
    if (!current_route_.waypoints.empty()) {
        float dist_to_target = position_.distance_to(
            current_route_.waypoints.back().position);
        awareness = 1.0f - std::min(dist_to_target / 1000.0f, 1.0f);
    }
    return awareness * pillars_[0];
}

float Drone::compute_willpower() {
    float willpower = (battery_ / max_battery_) * 0.5f + 0.5f;
    return willpower * pillars_[1];
}

float Drone::compute_force() {
    float force = (speed_ / 30.0f) * 0.3f + (current_load_ / cargo_capacity_) * 0.7f;
    return force * pillars_[2];
}

void Drone::update_pillars_from_behavior() {
    pillars_[0] = compute_awareness();
    pillars_[1] = compute_willpower();
    pillars_[2] = compute_force();
}

DroneFleet::DroneFleet(uint32_t fleet_id, uint32_t player_id)
    : fleet_id_(fleet_id)
    , player_id_(player_id)
    , emergency_console_(nullptr)
    , crispr_vault_(nullptr)
    , next_route_id_(1)
    , next_mission_id_(1)
{
}

DroneFleet::~DroneFleet() {
    for (auto drone : drones_) {
        delete drone;
    }
}

Drone* DroneFleet::create_drone() {
    if (drones_.size() >= MAX_DRONES_PER_FLEET) {
        return nullptr;
    }
    uint32_t drone_id = static_cast<uint32_t>(drones_.size() + 1);
    Drone* drone = new Drone(drone_id, player_id_);
    drones_.push_back(drone);
    return drone;
}

bool DroneFleet::remove_drone(uint32_t drone_id) {
    auto it = std::find_if(drones_.begin(), drones_.end(),
        [drone_id](Drone* d) { return d->get_id() == drone_id; });
    
    if (it != drones_.end()) {
        delete *it;
        drones_.erase(it);
        return true;
    }
    return false;
}

Drone* DroneFleet::get_drone(uint32_t drone_id) {
    for (auto drone : drones_) {
        if (drone->get_id() == drone_id) {
            return drone;
        }
    }
    return nullptr;
}

const Drone* DroneFleet::get_drone(uint32_t drone_id) const {
    for (auto drone : drones_) {
        if (drone->get_id() == drone_id) {
            return drone;
        }
    }
    return nullptr;
}

uint32_t DroneFleet::get_available_count() const {
    uint32_t count = 0;
    for (const auto drone : drones_) {
        if (drone->is_available()) {
            count++;
        }
    }
    return count;
}

void DroneFleet::update(float delta_time) {
    for (auto drone : drones_) {
        drone->update(delta_time);
    }
    resolve_collisions();
}

Route DroneFleet::compute_route(const Vec3& start, const Vec3& end,
                                  const float terrain_cost[64][64]) const {
    Route route;
    route.route_id = next_route_id_;
    route.is_valid = true;

    Waypoint start_wp;
    start_wp.position = start;
    start_wp.id = 0;
    start_wp.is_waypoint = true;
    route.waypoints.push_back(start_wp);

    float direct_distance = start.distance_to(end);
    uint32_t intermediate_count = static_cast<uint32_t>(
        std::max(1.0f, direct_distance / 50.0f));

    for (uint32_t i = 1; i <= intermediate_count; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(intermediate_count + 1);
        Waypoint wp;
        wp.position.x = start.x + (end.x - start.x) * t;
        wp.position.y = start.y + (end.y - start.y) * t;
        wp.position.z = start.z + (end.z - start.z) * t;
        wp.id = i;
        wp.is_waypoint = (i < intermediate_count);
        
        if (terrain_cost != nullptr) {
            int grid_x = static_cast<int>(wp.position.x / TERRAIN_GRID_SIZE);
            int grid_z = static_cast<int>(wp.position.z / TERRAIN_GRID_SIZE);
            if (grid_x >= 0 && grid_x < 64 && grid_z >= 0 && grid_z < 64) {
                wp.arrival_time = route.estimated_time + 
                    compute_terrain_cost(route.waypoints.back().position, wp.position, terrain_cost);
            }
        }
        
        route.waypoints.push_back(wp);
    }

    Waypoint end_wp;
    end_wp.position = end;
    end_wp.id = intermediate_count + 1;
    end_wp.is_target = true;
    end_wp.is_waypoint = false;
    route.waypoints.push_back(end_wp);

    route.total_distance = start.distance_to(end);
    route.estimated_time = route.total_distance / 20.0f;

    return route;
}

bool DroneFleet::replan_route(uint32_t drone_id, const Vec3& new_target) {
    Drone* drone = get_drone(drone_id);
    if (!drone) {
        return false;
    }

    Route new_route = compute_route(drone->get_position(), new_target, nullptr);
    return drone->assign_route(new_route);
}

bool DroneFleet::assign_mission(MissionType type, const std::vector<uint32_t>& drone_ids,
                                 const std::vector<Waypoint>& waypoints,
                                 const std::vector<CargoManifest>& cargo) {
    if (drone_ids.empty() || waypoints.empty()) {
        return false;
    }

    size_t drones_to_use = std::min(drone_ids.size(), waypoints.size());
    
    for (size_t i = 0; i < drones_to_use; ++i) {
        Drone* drone = get_drone(drone_ids[i]);
        if (!drone || !drone->is_available()) {
            continue;
        }

        Route route;
        route.route_id = next_route_id_++;
        route.waypoints = waypoints;
        route.is_valid = true;

        float total_dist = 0.0f;
        for (size_t j = 1; j < waypoints.size(); ++j) {
            total_dist += waypoints[j-1].position.distance_to(waypoints[j].position);
        }
        route.total_distance = total_dist;
        route.estimated_time = total_dist / 20.0f;

        drone->assign_route(route);

        for (const auto& c : cargo) {
            drone->add_cargo(c);
        }

        switch (type) {
            case MissionType::COLLECTION:
            case MissionType::CREATURE_TRANSPORT:
                drone->set_status(DroneStatus::COLLECTING);
                break;
            case MissionType::DELIVERY:
                drone->set_status(DroneStatus::DELIVERING);
                break;
            case MissionType::EMERGENCY_RESPONSE:
                drone->set_status(DroneStatus::EMERGENCY);
                break;
            default:
                drone->set_status(DroneStatus::MOVING);
                break;
        }
    }

    return true;
}

void DroneFleet::cancel_mission(uint32_t mission_id) {
    for (auto drone : drones_) {
        drone->clear_route();
        drone->set_status(DroneStatus::IDLE);
    }
}

void DroneFleet::emergency_recall() {
    for (auto drone : drones_) {
        drone->set_status(DroneStatus::RETURNING);
        Route return_route;
        return_route.route_id = next_route_id_++;
        return_route.is_valid = true;
        
        Waypoint home;
        home.position = Vec3(0, 0, 0);
        home.is_target = true;
        return_route.waypoints.push_back(home);
        
        drone->assign_route(return_route);
    }
}

bool DroneFleet::check_collision(Drone* drone, const Vec3& new_pos) const {
    for (auto other : drones_) {
        if (other == drone) continue;
        if (new_pos.distance_to(other->get_position()) < COLLISION_RADIUS) {
            return true;
        }
    }
    return false;
}

void DroneFleet::resolve_collisions() {
    for (size_t i = 0; i < drones_.size(); ++i) {
        for (size_t j = i + 1; j < drones_.size(); ++j) {
            Vec3 pos_i = drones_[i]->get_position();
            Vec3 pos_j = drones_[j]->get_position();
            float dist = pos_i.distance_to(pos_j);
            
            if (dist < COLLISION_RADIUS && dist > 0.001f) {
                Vec3 push;
                push.x = (pos_i.x - pos_j.x) / dist * COLLISION_RADIUS * 0.5f;
                push.y = (pos_i.y - pos_j.y) / dist * COLLISION_RADIUS * 0.5f;
                push.z = (pos_i.z - pos_j.z) / dist * COLLISION_RADIUS * 0.5f;
                
                Vec3 new_pos_i = pos_i;
                new_pos_i.x += push.x;
                new_pos_i.y += push.y;
                new_pos_i.z += push.z;
                drones_[i]->set_position(new_pos_i);
            }
        }
    }
}

float DroneFleet::compute_terrain_cost(const Vec3& from, const Vec3& to,
                                        const float terrain_cost[64][64]) const {
    if (!terrain_cost) {
        return from.distance_to(to) / 20.0f;
    }

    float base_time = from.distance_to(to) / 20.0f;
    int grid_x = static_cast<int>(to.x / TERRAIN_GRID_SIZE);
    int grid_z = static_cast<int>(to.z / TERRAIN_GRID_SIZE);

    if (grid_x >= 0 && grid_x < 64 && grid_z >= 0 && grid_z < 64) {
        float terrain_factor = 1.0f + terrain_cost[grid_x][grid_z] * 2.0f;
        return base_time * terrain_factor;
    }

    return base_time;
}

void DroneFleet::get_fleet_status(char* out_json, uint32_t buffer_size) const {
    if (!out_json || buffer_size == 0) return;

    std::string json = "{\"fleet_id\":" + std::to_string(fleet_id_) + 
                       ",\"drones\":[";
    
    for (size_t i = 0; i < drones_.size(); ++i) {
        const Drone* d = drones_[i];
        json += "{\"id\":" + std::to_string(d->get_id()) +
                ",\"status\":" + std::to_string(static_cast<int>(d->get_status())) +
                ",\"battery\":" + std::to_string(d->get_battery()) +
                ",\"position\":[" + std::to_string(d->get_position().x) + "," +
                               std::to_string(d->get_position().y) + "," +
                               std::to_string(d->get_position().z) + "]}";
        if (i < drones_.size() - 1) json += ",";
    }
    
    json += "],\"available_count\":" + std::to_string(get_available_count()) + "}";
    
    std::strncpy(out_json, json.c_str(), buffer_size - 1);
    out_json[buffer_size - 1] = '\0';
}

} // namespace gameplay
} // namespace vn