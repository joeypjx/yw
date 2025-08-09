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

    // 组件信息
    std::vector<node::ComponentInfo> component;
    
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

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NodeData, box_id, slot_id, cpu_id, srio_id, host_ip, hostname, service_port, box_type, board_type, cpu_type, os_type, resource_type, cpu_arch, gpu, component, ipmb_address, module_type, bmc_company, bmc_version, status, last_heartbeat);

class NodeStorage {
private:
    //m_nodes 共享指针
    std::unordered_map<std::string, std::shared_ptr<NodeData>> m_nodes;
    mutable std::mutex m_mutex;
    // 活跃节点判定超时时长（毫秒）
    int64_t m_active_timeout_ms = 10000; // 默认10秒

public:
    NodeStorage();
    ~NodeStorage();
    
    // 存储节点信息
    bool storeBoxInfo(const node::BoxInfo& node_info);
    
    // 存储UDP信息
    bool storeUDPInfo(const UdpInfo& udp_info);

    // 存储组件信息
    bool storeComponentInfo(const std::string& host_ip, const std::vector<node::ComponentInfo>& component_info);
    
    // 获取节点数据
    std::shared_ptr<NodeData> getNodeData(const std::string& host_ip);
    // 获取节点数据（只读）
    std::shared_ptr<const NodeData> getNodeDataReadonly(const std::string& host_ip);
    
    // 获取所有节点
    std::vector<std::shared_ptr<NodeData>> getAllNodes();
    // 获取所有节点（只读）
    std::vector<std::shared_ptr<const NodeData>> getAllNodesReadonly();
    
    // 获取活跃节点（10秒内有心跳的节点）
    std::vector<std::shared_ptr<NodeData>> getActiveNodes();
    // 获取活跃节点（只读）
    std::vector<std::shared_ptr<const NodeData>> getActiveNodesReadonly();
    
    // 删除节点
    bool removeNode(const std::string& host_ip);
    
    // 获取节点数量
    size_t getNodeCount();
    
    // 获取活跃节点IP列表
    std::vector<std::string> getActiveNodeIPs();

    // 设置活跃节点判定超时时长（毫秒）
    void setActiveTimeoutMs(int64_t timeout_ms);

    // 更新节点状态（线程安全）
    void updateNodeStatus(const std::string& host_ip, const std::string& status);
};

#endif // NODE_STORAGE_H