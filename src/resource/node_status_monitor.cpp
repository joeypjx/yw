#include "node_status_monitor.h"
#include "log_manager.h"
#include <chrono>
#include <thread>

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
    if (!m_node_storage) {
        return;
    }

    auto node_list = m_node_storage->getAllNodesReadonly();
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    for (const auto& node : node_list) {
        auto diff_ms = now - node->last_heartbeat;
        if (diff_ms < 0) {
            // 保护：时间异常，跳过
            continue;
        }
        auto time_since_heartbeat = diff_ms / 1000;  // 转换为秒

        // 先判断节点当前应该的状态
        std::string expected_status = (time_since_heartbeat <= m_offline_threshold.count()) ? "online" : "offline";
        
        // 如果状态发生变化
        if (node->status != expected_status) {
            std::string old_status = node->status;
            
            if (expected_status == "offline" && node->status == "online") {
                // 节点从在线变为离线，触发告警
                LogManager::getLogger()->warn("Node '{}' is offline. Last heartbeat {} seconds ago.", node->host_ip, time_since_heartbeat);                
                
                // 调用状态变化回调
                notifyStatusChange(node->host_ip, old_status, expected_status);
                
            } else if (expected_status == "online" && node->status == "offline") {
                // 节点从离线恢复在线，解决告警
                LogManager::getLogger()->info("Node '{}' is back online.", node->host_ip);
                
                // 调用状态变化回调
                notifyStatusChange(node->host_ip, old_status, expected_status);
            }
            
            // 更新节点状态（通过NodeStorage线程安全接口）
            m_node_storage->updateNodeStatus(node->host_ip, expected_status);
        }
    }
}

void NodeStatusMonitor::notifyStatusChange(const std::string& host_ip, const std::string& old_status, const std::string& new_status) {
    NodeStatusChangeCallback callback_copy;
    {
        std::lock_guard<std::mutex> lock(m_callback_mutex);
        callback_copy = m_status_change_callback;
    }
    if (!callback_copy) {
        return;
    }
    try {
        // 直接在当前监控线程调用复制的回调，避免分离线程带来的生命周期风险
        callback_copy(host_ip, old_status, new_status);
        LogManager::getLogger()->debug("NodeStatusMonitor callback invoked for node {} ({}->{})", 
                                       host_ip, old_status, new_status);
    } catch (const std::exception& e) {
        LogManager::getLogger()->error("Exception in NodeStatusMonitor callback: {}", e.what());
    } catch (...) {
        LogManager::getLogger()->error("Unknown exception in NodeStatusMonitor callback");
    }
}