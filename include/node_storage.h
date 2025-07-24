#ifndef NODE_STORAGE_H
#define NODE_STORAGE_H

#include "json.hpp"
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>

struct NodeData {
    int api_version;
    int box_id;
    int slot_id;
    int cpu_id;
    int srio_id;
    std::string host_ip;
    std::string hostname;
    uint16_t service_port;
    std::string box_type;
    std::string board_type;
    std::string cpu_type;
    std::string os_type;
    std::string resource_type;
    std::string cpu_arch;
    nlohmann::json gpu;
    std::chrono::system_clock::time_point last_heartbeat;
};

class NodeStorage {
public:
    NodeStorage();
    ~NodeStorage();

    bool storeNodeData(const std::string& host_ip, const nlohmann::json& data);
    
    std::shared_ptr<NodeData> getNodeData(const std::string& host_ip);
    
    std::vector<std::shared_ptr<NodeData>> getAllNodes();
    
    bool removeNode(const std::string& host_ip);
    
    size_t getNodeCount();
    
    std::vector<std::string> getActiveNodeIPs();

private:
    std::unordered_map<std::string, std::shared_ptr<NodeData>> m_nodes;
    std::mutex m_mutex;
};

#endif // NODE_STORAGE_H