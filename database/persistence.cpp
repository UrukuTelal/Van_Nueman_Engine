#include <postgresql/libpq-fe.h>
#include <string>
#include <vector>
#include <cstring>
#include <iostream>

// PostgreSQL connection wrapper for Van_Nueman world persistence
// From FULL_ARCHITECTURE.md: database/persistence - PostgreSQL SVO saves (64³ chunks)

class VanNuemanDB {
public:
    VanNuemanDB(const std::string& conn_str) {
        std::string conn_str_with_timeout = conn_str;
        if (conn_str_with_timeout.find("connect_timeout") == std::string::npos) {
            if (conn_str_with_timeout.find("://") != std::string::npos) {
                if (conn_str_with_timeout.find('?') != std::string::npos) {
                    conn_str_with_timeout += "&connect_timeout=10";
                } else {
                    conn_str_with_timeout += "?connect_timeout=10";
                }
            } else {
                conn_str_with_timeout += " connect_timeout=10";
            }
        }
        conn = PQconnectdb(conn_str_with_timeout.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            std::cerr << "Connection failed: " << PQerrorMessage(conn) << std::endl;
            connected = false;
        } else {
            connected = true;
            init_tables();
        }
    }

    // Build connection string from environment variables with timeout
    static std::string build_connection_string() {
        const char* db_host = getenv("VAN_NUEMAN_DB_HOST");
        const char* db_port = getenv("VAN_NUEMAN_DB_PORT");
        const char* db_user = getenv("VAN_NUEMAN_DB_USER");
        const char* db_pass = getenv("VAN_NUEMAN_DB_PASS");

        std::string host = db_host ? db_host : "localhost";
        std::string port = db_port ? db_port : "5432";
        std::string user = db_user ? db_user : "postgres";
        std::string pass = db_pass ? db_pass : "";

        return "host=" + host + " port=" + port + " dbname=van_nueman user=" + user +
               " password=" + pass + " connect_timeout=10";
    }

    ~VanNuemanDB() {
        if (conn) PQfinish(conn);
    }

    bool is_connected() const { return connected; }

    // Save SVO chunk (64³ voxels)
    bool save_svo_chunk(int server_id, int x, int y, int z, int level,
                        const void* voxel_data, size_t data_size) {
        if (!connected) return false;

        const char* sql = "INSERT INTO svo_chunks (server_id, x, y, z, level, voxel_data, last_modified) "
                          "VALUES ($1, $2, $3, $4, $5, $6, CURRENT_TIMESTAMP) "
                          "ON CONFLICT (server_id, x, y, z, level) "
                          "DO UPDATE SET voxel_data = EXCLUDED.voxel_data, last_modified = CURRENT_TIMESTAMP";

        const char* params[6];
        std::string s_server = std::to_string(server_id);
        std::string s_x = std::to_string(x);
        std::string s_y = std::to_string(y);
        std::string s_z = std::to_string(z);
        std::string s_level = std::to_string(level);
        
        params[0] = s_server.c_str();
        params[1] = s_x.c_str();
        params[2] = s_y.c_str();
        params[3] = s_z.c_str();
        params[4] = s_level.c_str();
        params[5] = (const char*)voxel_data;  // Binary data

        int param_lengths[6] = {(int)s_server.size(), (int)s_x.size(), (int)s_y.size(),
                                  (int)s_z.size(), (int)s_level.size(), (int)data_size};
        int param_formats[6] = {0, 0, 0, 0, 0, 1};  // Last param is binary

        PGresult* res = PQexecParams(conn, sql, 6, NULL, params,
                                      param_lengths, param_formats, 1);
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    }

    // Load SVO chunk
    std::vector<uint8_t> load_svo_chunk(int server_id, int x, int y, int z, int level) {
        std::vector<uint8_t> result;
        if (!connected) return result;

        const char* sql = "SELECT voxel_data FROM svo_chunks "
                          "WHERE server_id = $1 AND x = $2 AND y = $3 AND z = $4 AND level = $5";
        
        const char* params[5];
        std::string s_server = std::to_string(server_id);
        std::string s_x = std::to_string(x);
        std::string s_y = std::to_string(y);
        std::string s_z = std::to_string(z);
        std::string s_level = std::to_string(level);
        
        params[0] = s_server.c_str();
        params[1] = s_x.c_str();
        params[2] = s_y.c_str();
        params[3] = s_z.c_str();
        params[4] = s_level.c_str();

        PGresult* res = PQexecParams(conn, sql, 5, NULL, params, NULL, NULL, 1);
        if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
            int len = PQgetlength(res, 0, 0);
            const char* data = PQgetvalue(res, 0, 0);
            result.assign(data, data + len);
        }
        PQclear(res);
        return result;
    }

    // Save world snapshot
    bool save_world_snapshot(int server_id, const std::string& checksum,
                             const void* snapshot_data, size_t data_size) {
        if (!connected) return false;

        const char* sql = "INSERT INTO world_snapshots (server_id, checksum, snapshot_data) "
                          "VALUES ($1, $2, $3)";
        
        const char* params[3];
        std::string s_server = std::to_string(server_id);
        params[0] = s_server.c_str();
        params[1] = checksum.c_str();
        params[2] = (const char*)snapshot_data;

        int param_lengths[3] = {(int)s_server.size(), (int)checksum.size(), (int)data_size};
        int param_formats[3] = {0, 0, 1};

        PGresult* res = PQexecParams(conn, sql, 3, NULL, params,
                                      param_lengths, param_formats, 1);
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    }

    // Log feedback (for federation log-decay loops)
    bool log_feedback(int source_server, int target_server, const std::string& force_type,
                      float magnitude, int pillar) {
        if (!connected) return false;

        const char* sql = "INSERT INTO feedback_logs (source_server, target_server, force_type, magnitude, pillar) "
                          "VALUES ($1, $2, $3, $4, $5)";
        
        const char* params[5];
        std::string s_source = std::to_string(source_server);
        std::string s_target = std::to_string(target_server);
        params[0] = s_source.c_str();
        params[1] = s_target.c_str();
        params[2] = force_type.c_str();
        params[3] = std::to_string(magnitude).c_str();
        params[4] = std::to_string(pillar).c_str();

        PGresult* res = PQexecParams(conn, sql, 5, NULL, params, NULL, NULL, 1);
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    }

    // Save neuroevolution weights
    bool save_evolution_weights(int server_id, const std::string& pillar_pair,
                                 float weight, float fitness, int generation) {
        if (!connected) return false;

        const char* sql = "INSERT INTO evolution_weights (server_id, pillar_pair, weight, fitness, generation) "
                          "VALUES ($1, $2, $3, $4, $5)";
        
        const char* params[5];
        std::string s_server = std::to_string(server_id);
        params[0] = s_server.c_str();
        params[1] = pillar_pair.c_str();
        params[2] = std::to_string(weight).c_str();
        params[3] = std::to_string(fitness).c_str();
        params[4] = std::to_string(generation).c_str();

        PGresult* res = PQexecParams(conn, sql, 5, NULL, params, NULL, NULL, 1);
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    }

private:
    PGconn* conn = nullptr;
    bool connected = false;

    void init_tables() {
        // Execute schema.sql if tables don't exist
        // In production, use migration system
    }
};
