// drone_fleet.h - Drone Fleet Management System
// Multi-drone coordination, route planning, cargo missions
// Integration with emergency console, CRISPR vault, cellular automata terrain

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <array>
#include <queue>
#include <memory>
#include <cmath>

#include "emergency_console.h"
#include "crispr_vault.h"

namespace vn {
namespace gameplay {

constexpr uint32_t MAX_DRONES_PER_FLEET = 16;
constexpr uint32_t MAX_WAYPOINTS = 64;
constexpr uint32_t NUM_PILLARS = 16;

enum class DroneStatus {
    IDLE,
    MOVING,
    COLLECTING,
    DELIVERING,
    RETURNING,
    EMERGENCY,
    RECHARGING
};

enum class MissionType {
    PATROL,
    COLLECTION,
    DELIVERY,
    SURVEY,
    EMERGENCY_RESPONSE,
    CREATURE_TRANSPORT
};

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    float distance_to(const Vec3& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
};

struct Waypoint {
    Vec3 position;
    uint32_t id;
    float arrival_time;
    bool is_target;
    bool is_waypoint;
    char label[64];
    Waypoint() : id(0), arrival_time(0.0f), is_target(false), is_waypoint(true) {
        label[0] = '\0';
    }
};

struct Route {
    std::vector<Waypoint> waypoints;
    uint32_t route_id;
    float total_distance;
    float estimated_time;
    bool is_valid;
    Route() : route_id(0), total_distance(0.0f), estimated_time(0.0f), is_valid(false) {}
};

struct CargoManifest {
    uint32_t genome_id;
    uint32_t quantity;
    float weight;
    char description[128];
    CargoManifest() : genome_id(0), quantity(0), weight(0.0f) {
        description[0] = '\0';
    }
};

class Drone {
public:
    Drone(uint32_t drone_id, uint32_t owner_id);
    ~Drone();

    void update(float delta_time);

    uint32_t get_id() const { return drone_id_; }
    const Vec3& get_position() const { return position_; }
    const Vec3& get_velocity() const { return velocity_; }
    DroneStatus get_status() const { return status_; }
    float get_battery() const { return battery_; }
    float get_cargo_capacity() const { return cargo_capacity_; }
    float get_current_load() const { return current_load_; }
    const float* get_pillars() const { return pillars_; }

    void set_position(const Vec3& pos);
    void set_status(DroneStatus status);

    bool assign_route(const Route& route);
    void clear_route();

    bool add_cargo(const CargoManifest& cargo);
    bool remove_cargo(uint32_t genome_id);
    void clear_cargo();

    bool has_cargo() const { return current_load_ > 0.0f; }

    void set_pillar_values(const float pillars[NUM_PILLARS]);
    void update_pillars_from_behavior();

    bool needs_recharge() const { return battery_ < 20.0f; }
    void recharge(float amount);

    bool is_available() const { 
        return status_ == DroneStatus::IDLE || status_ == DroneStatus::RECHARGING;
    }

private:
    uint32_t drone_id_;
    uint32_t owner_id_;
    Vec3 position_;
    Vec3 velocity_;
    DroneStatus status_;
    float battery_;
    float max_battery_;
    float speed_;
    float cargo_capacity_;
    float current_load_;
    float pillars_[NUM_PILLARS];

    Route current_route_;
    size_t current_waypoint_index_;
    std::vector<CargoManifest> cargo_;

    void move_toward_waypoint(float delta_time);
    float compute_awareness();
    float compute_willpower();
    float compute_force();
};

class DroneFleet {
public:
    DroneFleet(uint32_t fleet_id, uint32_t player_id);
    ~DroneFleet();

    Drone* create_drone();
    bool remove_drone(uint32_t drone_id);
    Drone* get_drone(uint32_t drone_id);
    const Drone* get_drone(uint32_t drone_id) const;

    uint32_t get_drone_count() const { return static_cast<uint32_t>(drones_.size()); }
    uint32_t get_available_count() const;

    void update(float delta_time);

    bool assign_mission(MissionType type, const std::vector<uint32_t>& drone_ids,
                       const std::vector<Waypoint>& waypoints,
                       const std::vector<CargoManifest>& cargo);
    void cancel_mission(uint32_t mission_id);
    void emergency_recall();

    const std::vector<Route>& get_planned_routes() const { return planned_routes_; }

    Route compute_route(const Vec3& start, const Vec3& end, 
                        const float terrain_cost[64][64]) const;
    bool replan_route(uint32_t drone_id, const Vec3& new_target);

    void set_emergency_console(EmergencyConsole* console) { emergency_console_ = console; }
    void set_crispr_vault(CRISPRVault* vault) { crispr_vault_ = vault; }

    void get_fleet_status(char* out_json, uint32_t buffer_size) const;

    const std::vector<Drone*>& get_drones() { return drones_; }

private:
    uint32_t fleet_id_;
    uint32_t player_id_;
    std::vector<Drone*> drones_;
    std::vector<Route> planned_routes_;

    EmergencyConsole* emergency_console_;
    CRISPRVault* crispr_vault_;

    uint32_t next_route_id_;
    uint32_t next_mission_id_;

    std::vector<Route> find_optimal_routes(const std::vector<Vec3>& targets,
                                           const float terrain_cost[64][64]) const;

    bool check_collision(Drone* drone, const Vec3& new_pos) const;
    void resolve_collisions();

    float compute_terrain_cost(const Vec3& from, const Vec3& to,
                               const float terrain_cost[64][64]) const;
};

} // namespace gameplay
} // namespace vn