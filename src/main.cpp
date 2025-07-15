#include <iostream>
#include "resource_storage.h"
#include <nlohmann/json.hpp>

int main() {
    std::cout << "Resource Storage Demo" << std::endl;
    
    // Create ResourceStorage instance
    ResourceStorage storage("127.0.0.1", "test", "HZ715Net");
    
    // Connect to TDengine
    if (!storage.connect()) {
        std::cerr << "Failed to connect to TDengine" << std::endl;
        return 1;
    }
    
    // Create database
    if (!storage.createDatabase("resource")) {
        std::cerr << "Failed to create database" << std::endl;
        return 1;
    }
    
    // Create tables
    if (!storage.createResourceTable()) {
        std::cerr << "Failed to create resource tables" << std::endl;
        return 1;
    }
    
    // Sample complete resource data matching docs/data.json format
    nlohmann::json resourceData = {
        {"cpu", {
            {"usage_percent", 75.5},
            {"load_avg_1m", 1.2},
            {"load_avg_5m", 1.5},
            {"load_avg_15m", 1.8},
            {"core_count", 8},
            {"core_allocated", 6},
            {"temperature", 65.0},
            {"voltage", 1.2},
            {"current", 2.5},
            {"power", 85.0}
        }},
        {"memory", {
            {"total", 16777216000},
            {"used", 8388608000},
            {"free", 8388608000},
            {"usage_percent", 50.0}
        }},
        {"network", nlohmann::json::array({
            {
                {"interface", "eth0"},
                {"rx_bytes", 1000000},
                {"tx_bytes", 2000000},
                {"rx_packets", 1000},
                {"tx_packets", 1500},
                {"rx_errors", 0},
                {"tx_errors", 0}
            },
            {
                {"interface", "wlan0"},
                {"rx_bytes", 500000},
                {"tx_bytes", 800000},
                {"rx_packets", 600},
                {"tx_packets", 800},
                {"rx_errors", 1},
                {"tx_errors", 0}
            }
        })},
        {"disk", nlohmann::json::array({
            {
                {"device", "/dev/sda1"},
                {"mount_point", "/"},
                {"total", 1000000000000},
                {"used", 500000000000},
                {"free", 500000000000},
                {"usage_percent", 50.0}
            },
            {
                {"device", "/dev/nvme0n1"},
                {"mount_point", "/home"},
                {"total", 2000000000000},
                {"used", 800000000000},
                {"free", 1200000000000},
                {"usage_percent", 40.0}
            }
        })},
        {"gpu", nlohmann::json::array({
            {
                {"index", 0},
                {"name", "NVIDIA GeForce RTX 3080"},
                {"compute_usage", 85.5},
                {"mem_usage", 70.0},
                {"mem_used", 7000000000},
                {"mem_total", 10000000000},
                {"temperature", 75.0},
                {"voltage", 1.1},
                {"current", 15.0},
                {"power", 320.0}
            },
            {
                {"index", 1},
                {"name", "NVIDIA GeForce RTX 3090"},
                {"compute_usage", 90.0},
                {"mem_usage", 80.0},
                {"mem_used", 19000000000},
                {"mem_total", 24000000000},
                {"temperature", 80.0},
                {"voltage", 1.2},
                {"current", 18.0},
                {"power", 350.0}
            }
        })}
    };
    
    // Insert data
    if (storage.insertResourceData("192.168.1.100", resourceData)) {
        std::cout << "Successfully inserted resource data" << std::endl;
    } else {
        std::cerr << "Failed to insert resource data" << std::endl;
        return 1;
    }
    
    return 0;
}