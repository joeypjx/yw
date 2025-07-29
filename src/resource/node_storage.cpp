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

bool NodeStorage::storeBoxInfo(const node::BoxInfo& node_info) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 从node_info中获取host_ip
        std::string host_ip = node_info.host_ip;
        
        // 检查是否已存在该host_ip的节点数据
        auto it = m_nodes.find(host_ip);
        std::shared_ptr<NodeData> node;
        
        if (it != m_nodes.end()) {
            // 如果已存在，则更新现有数据
            node = it->second;
            LogManager::getLogger()->debug("Updating existing node info for host: {}", host_ip);
        } else {
            // 如果不存在，则创建新的节点数据
            node = std::make_shared<NodeData>();
            LogManager::getLogger()->debug("Creating new node info for host: {}", host_ip);
        }
        
        // 更新节点数据
        node->box_id = node_info.box_id;
        node->slot_id = node_info.slot_id;
        node->cpu_id = node_info.cpu_id;
        node->srio_id = node_info.srio_id;
        node->host_ip = node_info.host_ip;
        node->hostname = node_info.hostname;
        node->service_port = node_info.service_port;
        node->box_type = node_info.box_type;
        node->board_type = node_info.board_type;
        node->cpu_type = node_info.cpu_type;
        node->os_type = node_info.os_type;
        node->resource_type = node_info.resource_type;
        node->cpu_arch = node_info.cpu_arch;
        
        // 更新GPU信息
        std::vector<GpuInfo> gpu_vector;
        for (const auto& gpu : node_info.gpu) {
            GpuInfo gpu_info;
            gpu_info.index = gpu.index;
            gpu_info.name = gpu.name;
            gpu_vector.push_back(gpu_info);
        }
        node->gpu = gpu_vector;
        
        // 更新心跳时间
        node->last_heartbeat = std::chrono::system_clock::now();
        
        // 存储或更新节点数据
        m_nodes[host_ip] = node;
        
        LogManager::getLogger()->debug("Node info {} for host: {}", 
                                     (it != m_nodes.end() ? "updated" : "stored"), host_ip);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getLogger()->error("Failed to store node info for host {}: {}", node_info.host_ip, e.what());
        return false;
    }
}

bool NodeStorage::storeUDPInfo(const UdpInfo& udp_info) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        int box_id = static_cast<int>(udp_info.boxid);
        bool has_updated = false;
        
        // 遍历所有board，为每个有效的board创建或更新节点数据
        for (int board_index = 0; board_index < 14; board_index++) {
            const auto& board = udp_info.board[board_index];
            
            // 检查board是否有效（通过检查moduletype是否为0）
            if (board.moduletype == 0) {
                continue; // 跳过无效的board
            }
            
            // 计算slot_id（board_index + 1，因为数组索引从0开始，但slot_id从1开始）
            int slot_id = board_index + 1;
            
            // 使用Utils::calculateHostIP方法计算host_ip
            std::string host_ip = Utils::calculateHostIP(box_id, slot_id);
            
            // 检查是否已存在该host_ip的节点数据
            auto it = m_nodes.find(host_ip);
            std::shared_ptr<NodeData> node;
            
            if (it != m_nodes.end()) {
                // 如果已存在，则更新现有数据
                node = it->second;
                LogManager::getLogger()->debug("Updating existing node with UDP info for host: {} (box_id={}, slot_id={})", 
                                             host_ip, box_id, slot_id);
            } else {
                // 如果不存在，则创建新的节点数据
                node = std::make_shared<NodeData>();
                LogManager::getLogger()->debug("Creating new node with UDP info for host: {} (box_id={}, slot_id={})", 
                                             host_ip, box_id, slot_id);
            }
            
            // 更新基本节点信息
            node->box_id = box_id;
            node->slot_id = slot_id;
            node->host_ip = host_ip;
            
            // 更新BMC相关信息
            node->ipmb_address = static_cast<int>(board.ipmbaddr);
            node->module_type = static_cast<int>(board.moduletype);
            node->bmc_company = static_cast<int>(board.bmccompany);
            
            // 转换BMC版本字符串
            std::string bmc_version(reinterpret_cast<const char*>(board.bmcversion), 8);
            // 去除null字符
            size_t null_pos = bmc_version.find('\0');
            if (null_pos != std::string::npos) {
                bmc_version = bmc_version.substr(0, null_pos);
            }
            node->bmc_version = bmc_version;
            
            // 更新心跳时间
            node->last_heartbeat = std::chrono::system_clock::now();
            
            // 存储或更新节点数据
            m_nodes[host_ip] = node;
            
            LogManager::getLogger()->debug("Updated BMC info for host {}: ipmb={}, module={}, company={}, version={}", 
                                         host_ip, node->ipmb_address, node->module_type, node->bmc_company, node->bmc_version);
            
            has_updated = true;
        }
        
        if (has_updated) {
            LogManager::getLogger()->debug("UDP info processed for box_id: {} ({} boards updated)", box_id, 
                                         std::count_if(udp_info.board, udp_info.board + 14, 
                                                     [](const UdpBoardInfo& board) { return board.moduletype != 0; }));
        } else {
            LogManager::getLogger()->warn("No valid boards found in UDP info for box_id: {}", box_id);
        }
        
        return has_updated;
        
    } catch (const std::exception& e) {
        LogManager::getLogger()->error("Failed to store UDP info: {}", e.what());
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

std::vector<std::shared_ptr<NodeData>> NodeStorage::getActiveNodes() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::shared_ptr<NodeData>> active_nodes;
    auto now = std::chrono::system_clock::now();
    auto timeout_duration = std::chrono::seconds(10); // 10秒超时
    
    for (const auto& pair : m_nodes) {
        const auto& node = pair.second;
        
        // 计算时间差
        auto time_diff = now - node->last_heartbeat;
        
        // 如果时间差小于等于10秒，则认为节点活跃
        if (time_diff <= timeout_duration) {
            active_nodes.push_back(node);
        }
    }
    
    LogManager::getLogger()->debug("Found {} active nodes out of {} total nodes", 
                                  active_nodes.size(), m_nodes.size());
    
    return active_nodes;
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