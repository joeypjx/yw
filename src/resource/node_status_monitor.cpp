#include "node_status_monitor.h"
#include "log_manager.h"
#include <chrono>

NodeStatusMonitor::NodeStatusMonitor(std::shared_ptr<NodeStorage> node_storage, std::shared_ptr<AlarmManager> alarm_manager)
    : m_node_storage(node_storage), m_alarm_manager(alarm_manager), m_running(false) {
    LogManager::getLogger()->info("NodeStatusMonitor created.");
}

NodeStatusMonitor::~NodeStatusMonitor() {
    stop();
}

void NodeStatusMonitor::start() {
    if (m_running) {
        return;
    }
    m_running = true;
    m_monitor_thread = std::thread(&NodeStatusMonitor::run, this);
    LogManager::getLogger()->info("NodeStatusMonitor started.");
}

void NodeStatusMonitor::stop() {
    m_running = false;
    if (m_monitor_thread.joinable()) {
        m_monitor_thread.join();
    }
    LogManager::getLogger()->info("NodeStatusMonitor stopped.");
}

void NodeStatusMonitor::setNodeStatusChangeCallback(NodeStatusChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_status_change_callback = callback;
    LogManager::getLogger()->info("NodeStatusMonitor callback set.");
}

void NodeStatusMonitor::clearNodeStatusChangeCallback() {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_status_change_callback = nullptr;
    LogManager::getLogger()->info("NodeStatusMonitor callback cleared.");
}

void NodeStatusMonitor::run() {
    while (m_running) {
        checkNodeStatus();
        std::this_thread::sleep_for(m_check_interval);
    }
}

void NodeStatusMonitor::checkNodeStatus() {
    if (!m_node_storage || !m_alarm_manager) {
        return;
    }

    auto node_list = m_node_storage->getAllNodes();
    auto now = std::chrono::system_clock::now();

    for (const auto& node_data : node_list.nodes) {
        auto node = std::make_shared<NodeData>(node_data);
        auto time_since_heartbeat = std::chrono::duration_cast<std::chrono::seconds>(now - node->last_heartbeat);
        
        // 使用节点IP作为告警指纹的一部分，确保每个节点的离线告警是唯一的
        std::map<std::string, std::string> labels = {
            {"host_ip", node->host_ip},
            {"hostname", node->hostname}
        };
        std::string fingerprint = m_alarm_manager->calculateFingerprint("NodeOffline", labels);

        // 先判断节点当前应该的状态
        std::string expected_status = (time_since_heartbeat <= m_offline_threshold) ? "online" : "offline";
        
        // 如果状态发生变化
        if (node->status != expected_status) {
            std::string old_status = node->status;
            
            if (expected_status == "offline" && node->status == "online") {
                // 节点从在线变为离线，触发告警
                LogManager::getLogger()->warn("Node '{}' is offline. Last heartbeat {} seconds ago.", node->host_ip, time_since_heartbeat.count());                
                
                // 调用状态变化回调
                notifyStatusChange(node->host_ip, old_status, expected_status);
                
            } else if (expected_status == "online" && node->status == "offline") {
                // 节点从离线恢复在线，解决告警
                LogManager::getLogger()->info("Node '{}' is back online.", node->host_ip);
                
                // 调用状态变化回调
                notifyStatusChange(node->host_ip, old_status, expected_status);
            }
            
            // 更新节点状态
            node->status = expected_status;
        }
    }
}

void NodeStatusMonitor::notifyStatusChange(const std::string& host_ip, const std::string& old_status, const std::string& new_status) {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    
    if (m_status_change_callback) {
        try {
            // 在单独的线程中异步调用回调，避免阻塞监控线程
            std::thread callback_thread([this, host_ip, old_status, new_status]() {
                try {
                    m_status_change_callback(host_ip, old_status, new_status);
                } catch (const std::exception& e) {
                    LogManager::getLogger()->error("Exception in NodeStatusMonitor callback: {}", e.what());
                } catch (...) {
                    LogManager::getLogger()->error("Unknown exception in NodeStatusMonitor callback");
                }
            });
            
            // 分离线程，让其在后台运行
            callback_thread.detach();
            
            LogManager::getLogger()->debug("NodeStatusMonitor callback invoked for node {} ({}->{})", 
                                         host_ip, old_status, new_status);
        } catch (const std::exception& e) {
            LogManager::getLogger()->error("Failed to invoke NodeStatusMonitor callback: {}", e.what());
        }
    }
}