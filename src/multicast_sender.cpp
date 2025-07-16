#include "multicast_sender.h"
#include "ip_utils.h"
#include "json.hpp"
#include <iostream>
#include <chrono>

// Network headers
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using json = nlohmann::json;

MulticastSender::MulticastSender(const std::string& multicast_ip, int multicast_port, int manager_port, const std::string& config_path)
    : m_multicast_ip(multicast_ip),
      m_multicast_port(multicast_port),
      m_manager_port(manager_port),
      m_running(false) {
    m_manager_ip = IPAddressUtil::getIPAddress(config_path);
}

MulticastSender::~MulticastSender() {
    if (m_running) {
        stop();
    }
}

void MulticastSender::start() {
    if (m_running) {
        return;
    }
    m_running = true;
    m_threads.emplace_back(&MulticastSender::heartbeatLoop, this);
    m_threads.emplace_back(&MulticastSender::resourceLoop, this);
    std::cout << "MulticastSender started." << std::endl;
}

void MulticastSender::stop() {
    m_running = false;
    for (auto& thread : m_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    std::cout << "MulticastSender stopped." << std::endl;
}

void MulticastSender::heartbeatLoop() {
    json heartbeat_msg = {
        {"api_version", 1},
        {"data", {
            {"manager_ip", m_manager_ip},
            {"manager_port", m_manager_port},
            {"url", "/heartbeat"}
        }}
    };
    std::string msg_str = heartbeat_msg.dump();

    while (m_running) {
        if (!sendMulticastMessage(msg_str)) {
            std::cerr << "Failed to send heartbeat multicast message." << std::endl;
        } else {
            std::cout << "Sent heartbeat message." << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void MulticastSender::resourceLoop() {
    json resource_msg = {
        {"api_version", 1},
        {"data", {
            {"manager_ip", m_manager_ip},
            {"manager_port", m_manager_port},
            {"url", "/resource"}
        }}
    };
    std::string msg_str = resource_msg.dump();

    while (m_running) {
        if (!sendMulticastMessage(msg_str)) {
            std::cerr << "Failed to send resource multicast message." << std::endl;
        } else {
            std::cout << "Sent resource message." << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

bool MulticastSender::sendMulticastMessage(const std::string& message) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return false;
    }

    struct sockaddr_in multicast_addr;
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_addr.s_addr = inet_addr(m_multicast_ip.c_str());
    multicast_addr.sin_port = htons(m_multicast_port);

    if (sendto(sock, message.c_str(), message.length(), 0, (struct sockaddr*)&multicast_addr, sizeof(multicast_addr)) < 0) {
        perror("sendto");
        close(sock);
        return false;
    }

    close(sock);
    return true;
}
