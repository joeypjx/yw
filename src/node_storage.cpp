#include "node_storage.h"
#include "log_manager.h"
#include <chrono>

NodeStorage::NodeStorage() {
    LogManager::getLogger()->info("NodeStorage initialized");
}

NodeStorage::~NodeStorage() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_nodes.clear();
    LogManager::getLogger()->info("NodeStorage destroyed");
}

bool NodeStorage::storeNodeData(const std::string& host_ip, const nlohmann::json& data) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto node = std::make_shared<NodeData>();
        node->api_version = data.value("api_version", 1);
        
        if (data.contains("data") && data["data"].is_object()) {
            const auto& nodeData = data["data"];
            
            node->box_id = nodeData.value("box_id", 0);
            node->slot_id = nodeData.value("slot_id", 0);
            node->cpu_id = nodeData.value("cpu_id", 0);
            node->srio_id = nodeData.value("srio_id", 0);
            node->host_ip = nodeData.value("host_ip", host_ip);
            node->hostname = nodeData.value("hostname", "");
            node->service_port = nodeData.value("service_port", 0);
            node->box_type = nodeData.value("box_type", "");
            node->board_type = nodeData.value("board_type", "");
            node->cpu_type = nodeData.value("cpu_type", "");
            node->os_type = nodeData.value("os_type", "");
            node->resource_type = nodeData.value("resource_type", "");
            node->cpu_arch = nodeData.value("cpu_arch", "");
            
            if (nodeData.contains("gpu") && nodeData["gpu"].is_array()) {
                node->gpu = nodeData["gpu"];
            } else {
                node->gpu = nlohmann::json::array();
            }
        }
        
        node->last_heartbeat = std::chrono::system_clock::now();
        
        m_nodes[host_ip] = node;
        
        LogManager::getLogger()->debug("Node data stored for host: {}", host_ip);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getLogger()->error("Failed to store node data for host {}: {}", host_ip, e.what());
        return false;
    }
}

std::shared_ptr<NodeData> NodeStorage::getNodeData(const std::string& host_ip) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_nodes.find(host_ip);
    if (it != m_nodes.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<NodeData>> NodeStorage::getAllNodes() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::shared_ptr<NodeData>> nodes;
    nodes.reserve(m_nodes.size());
    
    for (const auto& pair : m_nodes) {
        nodes.push_back(pair.second);
    }
    
    return nodes;
}

bool NodeStorage::removeNode(const std::string& host_ip) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_nodes.find(host_ip);
    if (it != m_nodes.end()) {
        m_nodes.erase(it);
        LogManager::getLogger()->info("Node removed: {}", host_ip);
        return true;
    }
    
    LogManager::getLogger()->warn("Node not found for removal: {}", host_ip);
    return false;
}

size_t NodeStorage::getNodeCount() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_nodes.size();
}

std::vector<std::string> NodeStorage::getActiveNodeIPs() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> ips;
    ips.reserve(m_nodes.size());
    
    for (const auto& pair : m_nodes) {
        ips.push_back(pair.first);
    }
    
    return ips;
}