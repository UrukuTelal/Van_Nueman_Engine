#include <Winsock2.h>
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include "../api/pillars_api.h"
#include "protocol.h"

#pragma comment(lib, "ws2_32.lib")

using namespace VanNueman::Protocol;

class FederatedServer {
public:
    FederatedServer(PillarsAPIContext ctx) : pillars_ctx(ctx), fed_socket(INVALID_SOCKET) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            return;
        }
        init_network();
    }

    ~FederatedServer() {
        if (fed_socket != INVALID_SOCKET)
            closesocket(fed_socket);
        WSACleanup();
    }

    void send_msg(uint32_t target, const FederationMsg& msg) {
        if (fed_socket == INVALID_SOCKET) return;

        sockaddr_in target_addr;
        target_addr.sin_family = AF_INET;
        target_addr.sin_port = htons(static_cast<u_short>(7778 + target));
        target_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        int sent = sendto(fed_socket, reinterpret_cast<const char*>(&msg), sizeof(msg),
                          0, reinterpret_cast<sockaddr*>(&target_addr), sizeof(target_addr));
        if (sent == SOCKET_ERROR) {
            std::cerr << "send_msg failed: " << WSAGetLastError() << std::endl;
        }
    }

    void receive_msg() {
        if (fed_socket == INVALID_SOCKET) return;

        // Set non-blocking with a short timeout
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 1000;  // 1ms poll
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fed_socket, &readfds);

        while (select(0, &readfds, nullptr, nullptr, &tv) > 0) {
            uint8_t buf[sizeof(FederationMsg) + 64];
            sockaddr_in from_addr;
            int from_len = sizeof(from_addr);

            int received = recvfrom(fed_socket, reinterpret_cast<char*>(buf), sizeof(buf),
                                    0, reinterpret_cast<sockaddr*>(&from_addr), &from_len);
            if (received <= 0) break;

            if (static_cast<size_t>(received) < sizeof(MessageHeader)) continue;

            MessageHeader* hdr = reinterpret_cast<MessageHeader*>(buf);
            if (hdr->magic != MAGIC) continue;

            switch (hdr->type) {
                case MSG_FEDERATION_MSG:
                case MSG_PILLAR_UPDATE: {
                    if (static_cast<size_t>(received) < sizeof(FederationMsg)) continue;
                    FederationMsg* fed = reinterpret_cast<FederationMsg*>(buf);
                    if (fed->hop_count >= 3) continue;
                    // Re-broadcast relay
                    FederationMsg relay = *fed;
                    relay.hop_count++;
                    send_msg(fed->target_server, relay);
                    break;
                }
                default:
                    break;
            }
        }
    }

private:
    void init_network() {
        fed_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (fed_socket == INVALID_SOCKET) {
            std::cerr << "socket creation failed: " << WSAGetLastError() << std::endl;
            return;
        }

        // Allow address reuse
        int opt = 1;
        setsockopt(fed_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt));

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(7778);
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(fed_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
        }

        // Set non-blocking mode for receive polling
        u_long nonblock = 1;
        ioctlsocket(fed_socket, FIONBIO, &nonblock);
    }

    SOCKET fed_socket;
    PillarsAPIContext pillars_ctx;
};
