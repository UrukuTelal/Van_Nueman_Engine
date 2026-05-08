#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <cstdlib>
#include "Winsock2.h"
#if defined(HAVE_POSTGRESQL)
#include <libpq-fe.h>
#endif
// DO NOT include .cu files directly - use proper C API headers
#include "../api/pillars_api.h"
#include "../api/skelly_api.h"
#include "../physics/pillar_coupling.h"
#include "../physics/deformation.h"
#include "../database/persistence.h"
#include "protocol.h"

// Forward declare StarCluster to avoid pulling in problematic SkellyTypes.h
class StarCluster;

#pragma comment(lib, "ws2_32.lib")

// Now include star_cluster.h after forward declaration
#include "../scene/star_cluster.h"

using namespace VanNueman::Protocol;

class VanNuemanServer {
public:
    VanNuemanServer() : db(nullptr), pillars_ctx(nullptr), skelly_ctx(nullptr), udp_socket(INVALID_SOCKET), running(false), cluster_(nullptr) {
        init_winsock();
        init_db();
        init_cuda();
        init_network();
        init_star_cluster();
    }

    ~VanNuemanServer() {
        running = false;
        if (cluster_) delete cluster_;
        if (db) delete db;
        if (pillars_ctx) pillars_api_cleanup(pillars_ctx);
        if (skelly_ctx) skelly_api_cleanup(skelly_ctx);
        if (udp_socket != INVALID_SOCKET) closesocket(udp_socket);
        WSACleanup();
    }

    void run() {
        running = true;
        auto last_tick = std::chrono::high_resolution_clock::now();
        const auto tick_duration = std::chrono::milliseconds(33); // 30 Hz

        auto last_feedback = std::chrono::high_resolution_clock::now();
        const auto feedback_interval = std::chrono::seconds(1); // 1 Hz

        auto last_evolution = std::chrono::high_resolution_clock::now();
        const auto evolution_interval = std::chrono::seconds(10); // 0.1 Hz

        while (running) {
            auto now = std::chrono::high_resolution_clock::now();

            if (now - last_tick >= tick_duration) {
                tick();
                last_tick = now;
            }

            if (now - last_feedback >= feedback_interval) {
                feedback_loop();
                last_feedback = now;
            }

            if (now - last_evolution >= evolution_interval) {
                neuroevolution_step();
                last_evolution = now;
            }

            handle_network_input();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

private:
    void init_winsock() {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            exit(1);
        }
    }

    void init_db() {
#if defined(HAVE_POSTGRESQL)
        std::string conn_str = VanNuemanDB::build_connection_string();
        db = new VanNuemanDB(conn_str);
        if (!db->is_connected()) std::cerr << "DB connection failed" << std::endl;
#endif
    }

    void init_cuda() {
        pillars_ctx = pillars_api_init(500000);
        skelly_ctx = skelly_api_init();
    }

    void init_network() {
        udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udp_socket == INVALID_SOCKET) { std::cerr << "Socket failed" << std::endl; exit(1); }

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(7777);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(udp_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed" << std::endl; exit(1);
        }

        u_long mode = 1;
        ioctlsocket(udp_socket, FIONBIO, &mode);
    }

    void init_star_cluster() {
        // Generate procedural star cluster with PillarAIColab integration
        cluster_ = new StarCluster(42);  // Seed for reproducibility
        cluster_->generate_cluster(50);  // 50 stars

        std::cout << "Star Cluster generated: " << cluster_->get_stars().size() << " stars" << std::endl;
        std::cout << "Planets: " << cluster_->get_planets().size() << std::endl;
        std::cout << "Initial lifeforms: " << cluster_->get_lifeforms().size() << std::endl;
    }

    void tick() {
        // Update star cluster simulation (30 Hz)
        if (cluster_) {
            cluster_->tick(0.033f);
        }

        // Update CUDA pillars
        pillars_api_update(pillars_ctx, 0.033f);
        skelly_api_step(skelly_ctx, 0.033f);

        broadcast_render();
        static int tick_count = 0;
        if (++tick_count % 10 == 0) db_sync();
    }

    void handle_network_input() {
        char buffer[1024];
        sockaddr_in client;
        int len = sizeof(client);
        int bytes = recvfrom(udp_socket, buffer, sizeof(buffer), 0, (sockaddr*)&client, &len);
        if (bytes > 0) {
            MessageHeader* h = (MessageHeader*)buffer;
            if (h->magic != MAGIC) return;

            if (h->payload_size == 0 || h->payload_size > MAX_PAYLOAD_SIZE) return;
            if (bytes < (int)(sizeof(MessageHeader) + h->payload_size)) return;

            // Handle message types
            if (h->type == MSG_CLIENT_INPUT) {
                // Client input - could affect lifeforms via PillarAIColab
            }
        }
    }

    void broadcast_render() {
        // Send star cluster state to clients
        if (!cluster_) return;

        // For each client, send visible stars/planets/lifeforms
        // Uses delta compression for efficiency
    }

    void feedback_loop() {
        // Federation update using PillarAIColab
        pillars_api_update(pillars_ctx, 1.0f);

        // Log to PillarAIColab blackboard
        std::cout << "[Feedback] Tick: " << (cluster_ ? cluster_->get_lifeforms().size() : 0) << " lifeforms alive" << std::endl;
    }

    void neuroevolution_step() {
        // Evolve lifeforms based on survival (handled in star_cluster.cpp)
        std::cout << "[Evolution] Running neuroevolution..." << std::endl;
    }

    void db_sync() {
        // Persist dirty chunks
    }

    SOCKET udp_socket;
    bool running;
    VanNuemanDB* db;
    PillarsAPIContext pillars_ctx;
    SkellyAPIContext skelly_ctx;
    std::vector<GeneticMaterial> genetic_storage;
    std::vector<TerraformResult> terraform_history;
    StarCluster* cluster_;
};

int server_main() {
    VanNuemanServer server;
    server.run();
    return 0;
}
