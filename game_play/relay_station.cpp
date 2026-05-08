// relay_station.cpp - Relay Station implementation
// Build relay stations to help other players navigate.

#include "relay_station.h"
#include <ctime>
#include <cmath>
#include <cstdlib>

RelayStationManager::RelayStationManager(uint32_t player_id) :
    player_id_(player_id), next_station_id_(6000) {
}

uint32_t RelayStationManager::build_station(float x, float y, float z,
                                               uint32_t star_system_id,
                                               const float pillar_template[NUM_PILLARS]) {
    RelayStation station;
    
    // Set position and system
    station.pos_x = x;
    station.pos_y = y;
    station.pos_z = z;
    station.star_system_id = star_system_id;
    station.owner_player_id = player_id_;
    
    // Copy pillar template as baseline
    for (int i = 0; i < NUM_PILLARS; i++) {
        station.baseline_pillars[i] = pillar_template[i];
    }
    
    // Compute ID from position + pillars (same PSV + position = same station)
    station.id = compute_station_id(x, y, z, pillar_template);
    
    // Check for duplicate (same identity already exists)
    for (const auto& s : stations_) {
        if (s.id == station.id) {
            return 0;  // Station already exists at this position with same PSV
        }
    }
    
    // Initialize station properties
    station.signal_strength = calculate_signal_strength(pillar_template);
    station.coverage_radius = 5.0f + pillar_template[8] * 15.0f;  // Presence affects range
    station.max_users = static_cast<uint32_t>(2 + pillar_template[6] * 18); // Cohesion affects capacity
    station.current_users = 0;
    station.is_public = true;
    station.is_active = true;
    station.total_uses = 0;
    station.uptime_hours = 0.0f;
    station.created_timestamp = static_cast<uint32_t>(time(nullptr));
    
    stations_.push_back(station);
    return station.id;
}

bool RelayStationManager::destroy_station(uint32_t station_id) {
    for (auto it = stations_.begin(); it != stations_.end(); ++it) {
        if (it->id == station_id && it->owner_player_id == player_id_) {
            stations_.erase(it);
            return true;
        }
    }
    return false;
}

RelayStation* RelayStationManager::get_station(uint32_t station_id) {
    for (auto& s : stations_) {
        if (s.id == station_id) return &s;
    }
    return nullptr;
}

std::vector<RelayStation*> RelayStationManager::get_player_stations(uint32_t player_id) {
    std::vector<RelayStation*> result;
    for (auto& s : stations_) {
        if (s.owner_player_id == player_id) {
            result.push_back(&s);
        }
    }
    return result;
}

std::vector<RelayStation*> RelayStationManager::find_stations_in_range(float x, float y, float z,
                                                                     float range) const {
    std::vector<RelayStation*> result;
    for (auto& s : stations_) {
        if (!s.is_active) continue;
        
        float dx = s.pos_x - x;
        float dy = s.pos_y - y;
        float dz = s.pos_z - z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        
        if (dist <= s.coverage_radius && dist <= range) {
            result.push_back(const_cast<RelayStation*>(&s));
        }
    }
    return result;
}

bool RelayStationManager::use_station(uint32_t station_id, uint32_t user_player_id) {
    for (auto& s : stations_) {
        if (s.id == station_id && s.is_active) {
            if (!s.is_public && s.owner_player_id != user_player_id) {
                return false;  // Private station
            }
            if (s.current_users >= s.max_users) {
                return false;  // Station full
            }
            s.current_users++;
            s.total_uses++;
            return true;
        }
    }
    return false;
}

bool RelayStationManager::leave_station(uint32_t station_id, uint32_t user_player_id) {
    for (auto& s : stations_) {
        if (s.id == station_id) {
            if (s.current_users > 0) {
                s.current_users--;
                return true;
            }
        }
    }
    return false;
}

void RelayStationManager::update(float delta_time) {
    for (auto& s : stations_) {
        if (!s.is_active) continue;
        s.uptime_hours += delta_time / 3600.0f;
        
        // Signal strength can fluctuate based on pillars
        float fluctuation = (rand() % 100) / 1000.0f - 0.05f;
        s.signal_strength = std::max(0.0f, std::min(1.0f, s.signal_strength + fluctuation));
    }
}

bool RelayStationManager::are_stations_identical(const RelayStation& a, const RelayStation& b) const {
    // Same position and same PSV = functionally identical
    if (fabs(a.pos_x - b.pos_x) > 0.001f) return false;
    if (fabs(a.pos_y - b.pos_y) > 0.001f) return false;
    if (fabs(a.pos_z - b.pos_z) > 0.001f) return false;
    
    for (int i = 0; i < NUM_PILLARS; i++) {
        if (fabs(a.baseline_pillars[i] - b.baseline_pillars[i]) > 0.001f) {
            return false;
        }
    }
    return true;
}

float RelayStationManager::calculate_signal_strength(const float pillars[NUM_PILLARS]) const {
    // Signal strength based on Presence, Influence, and Integrity pillars
    float presence = pillars[8];    // PILLAR_PRESENCE
    float influence = pillars[3];   // PILLAR_INFLUENCE
    float integrity = pillars[5];    // PILLAR_INTEGRITY
    
    return std::min(1.0f, (presence * 0.5f + influence * 0.3f + integrity * 0.2f));
}

uint32_t RelayStationManager::compute_station_id(float x, float y, float z,
                                                   const float pillars[NUM_PILLARS]) const {
    // FNV-1a hash of position + pillar values
    uint32_t hash = 2166136261u;
    
    // Hash position
    uint32_t bits;
    memcpy(&bits, &x, sizeof(float));
    hash ^= bits; hash *= 16777619u;
    memcpy(&bits, &y, sizeof(float));
    hash ^= bits; hash *= 16777619u;
    memcpy(&bits, &z, sizeof(float));
    hash ^= bits; hash *= 16777619u;
    
    // Hash pillars
    for (int i = 0; i < NUM_PILLARS; i++) {
        memcpy(&bits, &pillars[i], sizeof(float));
        hash ^= bits;
        hash *= 16777619u;
    }
    
    return hash ? hash : 1u;
}
