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

    auto nodes = m_node_storage->getAllNodes();
    auto now = std::chrono::system_clock::now();

    for (const auto& node : nodes) {
        auto time_since_heartbeat = std::chrono::duration_cast<std::chrono::seconds>(now - node->last_heartbeat);
        
        // 使用节点IP作为告警指纹的一部分，确保每个节点的离线告警是唯一的
        std::map<std::string, std::string> labels = {
            {"host_ip", node->host_ip},
            {"hostname", node->hostname}
        };
        std::string fingerprint = m_alarm_manager->calculateFingerprint("NodeOffline", labels);

        if (time_since_heartbeat > m_offline_threshold) {
            // 节点离线，触发告警
            LogManager::getLogger()->warn("Node '{}' is offline. Last heartbeat {} seconds ago.", node->host_ip, time_since_heartbeat.count());

            nlohmann::json labels_json = {
                {"alert_name", "NodeOffline"},
                {"host_ip", node->host_ip},
                {"hostname", node->hostname},
                {"severity", "critical"}
            };
            
            nlohmann::json annotations = {
                {"summary", "Node is offline"},
                {"description", "Node " + node->host_ip + " has not sent a heartbeat for more than " + std::to_string(m_offline_threshold.count()) + " seconds."}
            };
            
            // 创建或更新告警，状态为 "firing"
            m_alarm_manager->createOrUpdateAlarm(fingerprint, labels_json, annotations);

        } else {
            // 节点在线，如果存在相关的离线告警，则解决它
            m_alarm_manager->resolveAlarm(fingerprint);
        }
    }
} 