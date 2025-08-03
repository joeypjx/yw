#ifndef NODE_STORAGE_H
#define NODE_STORAGE_H

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <cstdint>
#include "bmc_listener.h"
#include "node_model.h"
#include "utils.h"
#include "json.hpp"

// GPU信息结构体
struct GpuInfo {
    int index;
    std::string name;
    
    GpuInfo() : index(0) {}
    GpuInfo(int idx, const std::string& gpu_name) : index(idx), name(gpu_name) {}
};

// JSON序列化支持
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GpuInfo, index, name);

// 节点数据结构体
struct NodeData {
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
    std::vector<GpuInfo> gpu;
    
    // BMC相关信息
    int ipmb_address;
    int module_type;
    int bmc_company;
    std::string bmc_version;

    // 节点状态，online/offline
    std::string status;
    
    std::chrono::system_clock::time_point last_heartbeat;
    
    NodeData() : box_id(0), slot_id(0), cpu_id(0), srio_id(0), service_port(0),
                 ipmb_address(0), module_type(0), bmc_company(0) {}
};

// 手动实现JSON序列化，因为last_heartbeat是time_point类型
inline void to_json(nlohmann::json& j, const NodeData& node) {
    j = nlohmann::json{
        {"box_id", node.box_id},
        {"slot_id", node.slot_id},
        {"cpu_id", node.cpu_id},
        {"srio_id", node.srio_id},
        {"host_ip", node.host_ip},
        {"hostname", node.hostname},
        {"service_port", node.service_port},
        {"box_type", node.box_type},
        {"board_type", node.board_type},
        {"cpu_type", node.cpu_type},
        {"os_type", node.os_type},
        {"resource_type", node.resource_type},
        {"cpu_arch", node.cpu_arch},
        {"gpu", node.gpu},
        {"ipmb_address", node.ipmb_address},
        {"module_type", node.module_type},
        {"bmc_company", node.bmc_company},
        {"bmc_version", node.bmc_version},
        {"status", node.status},
        {"last_heartbeat", std::chrono::duration_cast<std::chrono::seconds>(node.last_heartbeat.time_since_epoch()).count()}
    };
}

inline void from_json(const nlohmann::json& j, NodeData& node) {
    j.at("box_id").get_to(node.box_id);
    j.at("slot_id").get_to(node.slot_id);
    j.at("cpu_id").get_to(node.cpu_id);
    j.at("srio_id").get_to(node.srio_id);
    j.at("host_ip").get_to(node.host_ip);
    j.at("hostname").get_to(node.hostname);
    j.at("service_port").get_to(node.service_port);
    j.at("box_type").get_to(node.box_type);
    j.at("board_type").get_to(node.board_type);
    j.at("cpu_type").get_to(node.cpu_type);
    j.at("os_type").get_to(node.os_type);
    j.at("resource_type").get_to(node.resource_type);
    j.at("cpu_arch").get_to(node.cpu_arch);
    j.at("gpu").get_to(node.gpu);
    j.at("ipmb_address").get_to(node.ipmb_address);
    j.at("module_type").get_to(node.module_type);
    j.at("bmc_company").get_to(node.bmc_company);
    j.at("bmc_version").get_to(node.bmc_version);
    j.at("status").get_to(node.status);
    
    // 处理时间戳
    long long timestamp;
    j.at("last_heartbeat").get_to(timestamp);
    node.last_heartbeat = std::chrono::system_clock::time_point(std::chrono::seconds(timestamp));
}

struct NodeDataList {
    std::vector<NodeData> nodes;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NodeDataList, nodes);

class NodeStorage {
private:
    std::unordered_map<std::string, std::shared_ptr<NodeData>> m_nodes;
    mutable std::mutex m_mutex;

public:
    NodeStorage();
    ~NodeStorage();
    
    // 存储节点信息
    bool storeBoxInfo(const node::BoxInfo& node_info);
    
    // 存储UDP信息
    bool storeUDPInfo(const UdpInfo& udp_info);
    
    // 获取节点数据
    std::shared_ptr<NodeData> getNodeData(const std::string& host_ip);
    
    // 获取所有节点
    NodeDataList getAllNodes();
    
    // 获取活跃节点（10秒内有心跳的节点）
    std::vector<std::shared_ptr<NodeData>> getActiveNodes();
    
    // 删除节点
    bool removeNode(const std::string& host_ip);
    
    // 获取节点数量
    size_t getNodeCount();
    
    // 获取活跃节点IP列表
    std::vector<std::string> getActiveNodeIPs();
};

#endif // NODE_STORAGE_H