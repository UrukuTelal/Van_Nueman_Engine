#include <postgresql/libpq-fe.h>
#include <string>
#include <vector>
#include <chrono>
#include <iostream>

// World state management for Van_Nueman
// From FULL_ARCHITECTURE.md: database/world_state - World snapshots

struct WorldSnapshot {
    int server_id;
    std::chrono::system_clock::time_point timestamp;
    std::string checksum;
    std::vector<uint8_t> data;
};

class WorldStateManager {
public:
    WorldStateManager(PGconn* connection) : conn(connection) {}

    // Create a new world snapshot
    bool create_snapshot(int server_id, const std::vector<uint8_t>& world_data) {
        if (!conn) return false;

        // Calculate simple checksum (in production, use SHA-256)
        std::string checksum = std::to_string(std::hash<std::string>{}(std::string(world_data.begin(), world_data.end())));

        const char* sql = "INSERT INTO world_snapshots (server_id, checksum, snapshot_data) "
                          "VALUES ($1, $2, $3)";
        
        const char* params[3];
        std::string s_server = std::to_string(server_id);
        params[0] = s_server.c_str();
        params[1] = checksum.c_str();
        params[2] = reinterpret_cast<const char*>(world_data.data());

        int param_lengths[3] = { (int)s_server.size(), (int)checksum.size(), (int)world_data.size() };
        int param_formats[3] = { 0, 0, 1 };

        PGresult* res = PQexecParams(conn, sql, 3, NULL, params, param_lengths, param_formats, 1);
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    }

    // Get latest snapshot for a server
    WorldSnapshot get_latest_snapshot(int server_id) {
        WorldSnapshot snapshot{server_id};
        if (!conn) return snapshot;

        const char* sql = "SELECT timestamp, checksum, snapshot_data FROM world_snapshots "
                          "WHERE server_id = $1 ORDER BY timestamp DESC LIMIT 1";
        
        std::string s_server = std::to_string(server_id);
        const char* params[1] = { s_server.c_str() };

        PGresult* res = PQexecParams(conn, sql, 1, NULL, params, NULL, NULL, 1);
        if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
            // Timestamp
            std::string ts_str = PQgetvalue(res, 0, 0);
            // Checksum
            snapshot.checksum = PQgetvalue(res, 0, 1);
            // Data
            int len = PQgetlength(res, 0, 2);
            const char* data = PQgetvalue(res, 0, 2);
            snapshot.data.assign(data, data + len);
        }
        PQclear(res);
        return snapshot;
    }

    // List all snapshots for a server
    std::vector<WorldSnapshot> list_snapshots(int server_id, int limit = 10) {
        std::vector<WorldSnapshot> snapshots;
        if (!conn) return snapshots;

        const char* sql = "SELECT timestamp, checksum, snapshot_data FROM world_snapshots "
                          "WHERE server_id = $1 ORDER BY timestamp DESC LIMIT $2";
        
        std::string s_server = std::to_string(server_id);
        std::string s_limit = std::to_string(limit);
        const char* params[2] = { s_server.c_str(), s_limit.c_str() };

        PGresult* res = PQexecParams(conn, sql, 2, NULL, params, NULL, NULL, 1);
        if (PQresultStatus(res) == PGRES_TUPLES_OK) {
            int rows = PQntuples(res);
            snapshots.reserve(rows);
            for (int i = 0; i < rows; i++) {
                WorldSnapshot snap{server_id};
                snap.checksum = PQgetvalue(res, i, 1);
                int len = PQgetlength(res, i, 2);
                const char* data = PQgetvalue(res, i, 2);
                snap.data.assign(data, data + len);
                snapshots.push_back(std::move(snap));
            }
        }
        PQclear(res);
        return snapshots;
    }

    // Clean old snapshots (keep last N per server)
    bool clean_old_snapshots(int server_id, int keep_last = 5) {
        if (!conn) return false;

        const char* sql = "DELETE FROM world_snapshots WHERE server_id = $1 AND timestamp NOT IN "
                          "(SELECT timestamp FROM world_snapshots WHERE server_id = $1 "
                          "ORDER BY timestamp DESC LIMIT $2)";
        
        std::string s_server = std::to_string(server_id);
        std::string s_keep = std::to_string(keep_last);
        const char* params[2] = { s_server.c_str(), s_keep.c_str() };

        PGresult* res = PQexecParams(conn, sql, 2, NULL, params, NULL, NULL, 0);
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    }

private:
    PGconn* conn;
};
