// relay_station.h - Relay Station for multiplayer navigation beacons
// From GAMEPLAY.md: "Build a relay station to help other players."
// Other players can use it for navigation waypoints.

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cstring>

#include <PillarEnum.h>

// Relay Station state
struct RelayStation {
    uint32_t id;                    // From compute_entity_id(baseline_pillars)
    uint32_t owner_player_id;
    float pos_x, pos_y, pos_z;       // Position in star system
    uint32_t star_system_id;
    
    // Pillar state (defines station identity)
    float baseline_pillars[NumPillars];
    
    // Station properties
    float signal_strength;            // 0.0 - 1.0 (affected by Presence pillar)
    float coverage_radius;             // Light-years
    uint32_t max_users;               // Max simultaneous users
    uint32_t current_users;
    
    bool is_public;                    // Can other players use it?
    bool is_active;
    
    // Telemetry
    uint32_t total_uses;
    float uptime_hours;
    uint32_t created_timestamp;
};

// Relay Station Manager
class RelayStationManager {
public:
    RelayStationManager(uint32_t player_id);
    
    // Build a new relay station
    // Returns station ID (derived from PSV + position)
    uint32_t build_station(float x, float y, float z, uint32_t star_system_id,
                              const float pillar_template[NumPillars]);
    
    // Destroy/remove station
    bool destroy_station(uint32_t station_id);
    
    // Find stations by ID or owner
    RelayStation* get_station(uint32_t station_id);
    std::vector<RelayStation*> get_player_stations(uint32_t player_id);
    
    // Find stations in range of a position
    std::vector<RelayStation*> find_stations_in_range(float x, float y, float z,
                                                       float range) const;
    
    // Player uses a relay station (for navigation, etc.)
    bool use_station(uint32_t station_id, uint32_t user_player_id);
    bool leave_station(uint32_t station_id, uint32_t user_player_id);
    
    // Update station (called per tick)
    void update(float delta_time);
    
    // Get all active stations (for rendering/network)
    const std::vector<RelayStation>& get_all_stations() const { return stations_; }
    
    // Check if two stations are functionally identical (same PSV + position)
    bool are_stations_identical(const RelayStation& a, const RelayStation& b) const;
    
    // Calculate signal strength from pillars
    float calculate_signal_strength(const float pillars[NumPillars]) const;
    
    uint32_t get_station_count() const { return static_cast<uint32_t>(stations_.size()); }
    
private:
    uint32_t player_id_;
    std::vector<RelayStation> stations_;
    uint32_t next_station_id_;
    
    // Compute station ID from position + pillar template
    uint32_t compute_station_id(float x, float y, float z,
                                   const float pillars[NumPillars]) const;
};
