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
    
    int64_t last_heartbeat = 0;  // 毫秒时间戳
    
    NodeData() : box_id(0), slot_id(0), cpu_id(0), srio_id(0), service_port(0),
                 ipmb_address(0), module_type(0), bmc_company(0) {}
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NodeData, box_id, slot_id, cpu_id, srio_id, host_ip, hostname, service_port, box_type, board_type, cpu_type, os_type, resource_type, cpu_arch, gpu, ipmb_address, module_type, bmc_company, bmc_version, status, last_heartbeat);

struct NodeDataList {
    std::vector<NodeData> nodes;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NodeDataList, nodes);

class NodeStorage {
private:
    //m_nodes 共享指针
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
    std::vector<std::shared_ptr<NodeData>> getAllNodes();
    
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