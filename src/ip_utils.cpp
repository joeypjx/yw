#include "ip_utils.h"
#include "log_manager.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>

// POSIX and network headers
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

using json = nlohmann::json;

// Helper function to get IP from a specific interface name
namespace {
std::string getIPForInterface(const std::string& interfaceName) {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        return "";
    }

    std::string ip;
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if (std::string(ifa->ifa_name) == interfaceName) {
            struct sockaddr_in* sa = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            ip = inet_ntoa(sa->sin_addr);
            break;
        }
    }

    freeifaddrs(ifaddr);
    return ip;
}
}

std::string IPAddressUtil::getIPAddress(const std::string& configPath) {
    if (!configPath.empty()) {
        std::string ip = getIPFromConfig(configPath);
        if (!ip.empty()) {
            LogManager::getLogger()->info("IP address loaded from config: {}", ip);
            return ip;
        }
    }

    std::string ip = getSmartDefaultIP();
    if (!ip.empty()) {
        LogManager::getLogger()->info("Smart default IP address selected: {}", ip);
        return ip;
    }

    LogManager::getLogger()->info("Falling back to loopback IP address.");
    return "127.0.0.1";
}

std::string IPAddressUtil::getIPFromConfig(const std::string& configPath) {
    std::ifstream configFile(configPath);
    if (!configFile.is_open()) {
        // File doesn't exist or can't be opened, which is fine.
        return "";
    }

    try {
        json config;
        configFile >> config;

        if (config.contains("ip_address")) {
            return config["ip_address"].get<std::string>();
        }

        if (config.contains("interface_name")) {
            std::string interfaceName = config["interface_name"].get<std::string>();
            return getIPForInterface(interfaceName);
        }

    } catch (const json::exception& e) {
        LogManager::getLogger()->error("Error parsing IP config file {}: {}", configPath, e.what());
    }

    return "";
}

std::string IPAddressUtil::getSmartDefaultIP() {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "";
    }

    std::vector<std::pair<std::string, std::string>> candidates;
    const std::vector<std::string> priorityPrefixes = {"en", "eth", "bond"};

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;

        // Must be UP, RUNNING, and not a LOOPBACK interface
        if ((ifa->ifa_flags & IFF_UP) && (ifa->ifa_flags & IFF_RUNNING) && !(ifa->ifa_flags & IFF_LOOPBACK)) {
            if (ifa->ifa_addr->sa_family == AF_INET) { // IPv4 only
                struct sockaddr_in* sa = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
                std::string ip = inet_ntoa(sa->sin_addr);
                candidates.emplace_back(ifa->ifa_name, ip);
            }
        }
    }

    freeifaddrs(ifaddr);

    // Prioritize based on interface name prefix
    for (const auto& prefix : priorityPrefixes) {
        for (const auto& candidate : candidates) {
            if (candidate.first.rfind(prefix, 0) == 0) { // starts with prefix
                return candidate.second;
            }
        }
    }

    // If no priority match, return the first available candidate
    if (!candidates.empty()) {
        return candidates.front().second;
    }

    return "";
}
