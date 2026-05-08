#include <iostream>
#include <Winsock2.h>
#include <thread>
#include <chrono>
#include <Ws2tcpip.h>
#include "protocol.h"
#include "../include/Entity.h"

#pragma comment(lib, "ws2_32.lib")

using namespace VanNueman::Protocol;

class VanNuemanClient {
public:
    VanNuemanClient(const char* ip, int port) : running(false), udp_socket(INVALID_SOCKET) {
        init_winsock();
        init_network(ip, port);
    }

    ~VanNuemanClient() {
        running = false;
        if (udp_socket != INVALID_SOCKET) {
            closesocket(udp_socket);
        }
        WSACleanup();
    }

    void run() {
        running = true;
        while (running) {
            handle_input();
            handle_network();
            render();
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    }

private:
    void init_winsock() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }

    void init_network(const char* ip, int port) {
        udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udp_socket == INVALID_SOCKET) return;
        
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        inet_pton(AF_INET, ip, &addr.sin_addr);
        addr.sin_port = htons(port);
        connect(udp_socket, (sockaddr*)&addr, sizeof(addr));
        u_long mode = 1;
        ioctlsocket(udp_socket, FIONBIO, &mode);
    }

    void handle_input() {
        // Send client input to server
    }

    void handle_network() {
        char buffer[1024];
        int bytes = recv(udp_socket, buffer, sizeof(buffer), 0);
        if (bytes < 0) {
            int wsa_err = WSAGetLastError();
            if (wsa_err != WSAEWOULDBLOCK) {
                std::cerr << "recv error: " << wsa_err << std::endl;
            }
            return;
        }
        if (bytes == 0) return;
        if (bytes < sizeof(MessageHeader)) {
            std::cerr << "Truncated message: " << bytes << " bytes < header size " << sizeof(MessageHeader) << std::endl;
            return;
        }
        MessageHeader* h = (MessageHeader*)buffer;
        if (h->magic != MAGIC) {
            std::cerr << "Invalid magic, discarding message" << std::endl;
            return;
        }
        if (h->type == MSG_RENDER_PACKET) {
            // Handle render packet
        }
    }

    void render() {
        // Vulkan render 1080p
    }

    SOCKET udp_socket;
    bool running;
};

int client_main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: client <ip> <port>" << std::endl;
        return 1;
    }
    
    VanNuemanClient client(argv[1], atoi(argv[2]));
    client.run();
    return 0;
}
