#include "component_status_monitor.h"
#include "log_manager.h"
#include <sstream>
#include <algorithm>

ComponentStatusMonitor::ComponentStatusMonitor(std::shared_ptr<NodeStorage> node_storage, 
                                             std::shared_ptr<AlarmManager> alarm_manager)
    : m_node_storage(node_storage), m_alarm_manager(alarm_manager), m_running(false),
      m_check_interval(std::chrono::seconds(30)),  // 默认30秒检查一次
      m_failed_threshold(std::chrono::seconds(60)) {  // 默认FAILED状态持续60秒后触发告警
    LogManager::getLogger()->info("ComponentStatusMonitor created.");
}

ComponentStatusMonitor::~ComponentStatusMonitor() {
    stop();
}

void ComponentStatusMonitor::start() {
    if (m_running) {
        return;
    }
    m_running = true;
    m_monitor_thread = std::thread(&ComponentStatusMonitor::run, this);
    LogManager::getLogger()->info("ComponentStatusMonitor started.");
}

void ComponentStatusMonitor::stop() {
    m_running = false;
    if (m_monitor_thread.joinable()) {
        m_monitor_thread.join();
    }
    LogManager::getLogger()->info("ComponentStatusMonitor stopped.");
}

void ComponentStatusMonitor::setComponentStatusChangeCallback(ComponentStatusChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_status_change_callback = callback;
    LogManager::getLogger()->info("ComponentStatusMonitor callback set.");
}

void ComponentStatusMonitor::clearComponentStatusChangeCallback() {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_status_change_callback = nullptr;
    LogManager::getLogger()->info("ComponentStatusMonitor callback cleared.");
}

void ComponentStatusMonitor::setCheckInterval(std::chrono::seconds interval) {
    m_check_interval = interval;
    LogManager::getLogger()->info("ComponentStatusMonitor check interval set to {} seconds.", interval.count());
}

void ComponentStatusMonitor::setFailedThreshold(std::chrono::seconds threshold) {
    m_failed_threshold = threshold;
    LogManager::getLogger()->info("ComponentStatusMonitor failed threshold set to {} seconds.", threshold.count());
}

void ComponentStatusMonitor::run() {
    while (m_running) {
        checkComponentStatus();
        std::this_thread::sleep_for(m_check_interval);
    }
}

std::string ComponentStatusMonitor::generateComponentKey(const std::string& host_ip, 
                                                       const std::string& instance_id, 
                                                       const std::string& uuid, 
                                                       int index) {
    std::ostringstream oss;
    oss << host_ip << ":" << instance_id << ":" << uuid << ":" << index;
    return oss.str();
}

void ComponentStatusMonitor::checkComponentStatus() {
    if (!m_node_storage || !m_alarm_manager) {
        return;
    }

    auto node_list = m_node_storage->getAllNodes();
    auto now = std::chrono::system_clock::now();
    
    std::lock_guard<std::mutex> history_lock(m_history_mutex);
    
    for (const auto& node : node_list) {
        // 检查节点的组件状态
        for (const auto& component : node->component) {
            std::string component_key = generateComponentKey(node->host_ip, 
                                                          component.instance_id, 
                                                          component.uuid, 
                                                          component.index);
            
            auto history_it = m_component_history.find(component_key);
            std::string current_state = component.state;
            
            if (history_it == m_component_history.end()) {
                // 新组件，记录初始状态
                ComponentStateHistory history;
                history.state = current_state;
                history.last_update = now;
                history.alarm_triggered = false;
                m_component_history[component_key] = history;
                
                LogManager::getLogger()->debug("New component detected: {} (state: {})", component_key, current_state);
                continue;
            }
            
            auto& history = history_it->second;
            std::string old_state = history.state;
            
            // 检查状态是否发生变化
            if (old_state != current_state) {
                LogManager::getLogger()->info("Component state changed: {} ({} -> {})", 
                                            component_key, old_state, current_state);
                
                // 通知状态变化
                notifyStatusChange(node->host_ip, component.instance_id, component.uuid, 
                                 component.index, old_state, current_state);
                
                // 更新历史记录
                history.state = current_state;
                history.last_update = now;
                history.alarm_triggered = false;  // 重置告警状态
            }
            
            // 检查FAILED状态是否需要触发告警
            if (current_state == "FAILED" && !history.alarm_triggered) {
                auto time_since_failed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - history.last_update).count();
                
                if (time_since_failed >= m_failed_threshold.count()) {
                    // 触发FAILED状态告警
                    std::map<std::string, std::string> labels = {
                        {"host_ip", node->host_ip},
                        {"instance_id", component.instance_id},
                        {"uuid", component.uuid},
                        {"index", std::to_string(component.index)},
                        {"component_name", component.name},
                        {"component_id", component.id}
                    };
                    
                    std::string fingerprint = m_alarm_manager->calculateFingerprint("ComponentFailed", labels);
                    
                    // 创建告警事件
                    AlarmEventRecord event;
                    event.fingerprint = fingerprint;
                    event.status = "firing";
                    event.labels_json = nlohmann::json(labels).dump();
                    
                    std::map<std::string, std::string> annotations = {
                        {"summary", "Component " + component.name + " is in FAILED state"},
                        {"description", "Component " + component.name + " (ID: " + component.id + 
                         ") on host " + node->host_ip + " has been in FAILED state for " + 
                         std::to_string(time_since_failed) + " seconds"},
                        {"severity", "critical"}
                    };
                    event.annotations_json = nlohmann::json(annotations).dump();
                    
                    event.starts_at = std::chrono::duration_cast<std::chrono::seconds>(
                        now.time_since_epoch()).count();
                    
                    // 添加到告警管理器
                    m_alarm_manager->addAlarmEvent(event);
                    
                    LogManager::getLogger()->warn("Component FAILED alarm triggered: {} (failed for {} seconds)", 
                                                component_key, time_since_failed);
                    
                    // 标记已触发告警
                    history.alarm_triggered = true;
                }
            } else if (current_state != "FAILED" && history.alarm_triggered) {
                // 如果状态从FAILED恢复，解决告警
                std::map<std::string, std::string> labels = {
                    {"host_ip", node->host_ip},
                    {"instance_id", component.instance_id},
                    {"uuid", component.uuid},
                    {"index", std::to_string(component.index)},
                    {"component_name", component.name},
                    {"component_id", component.id}
                };
                
                std::string fingerprint = m_alarm_manager->calculateFingerprint("ComponentFailed", labels);
                
                // 解决告警
                m_alarm_manager->resolveAlarmEvent(fingerprint);
                
                LogManager::getLogger()->info("Component FAILED alarm resolved: {} (state: {})", 
                                            component_key, current_state);
                
                // 重置告警状态
                history.alarm_triggered = false;
            }
        }
    }
    
    // 清理过期的历史记录（超过1小时没有更新的记录）
    auto one_hour_ago = now - std::chrono::hours(1);
    for (auto it = m_component_history.begin(); it != m_component_history.end();) {
        if (it->second.last_update < one_hour_ago) {
            it = m_component_history.erase(it);
        } else {
            ++it;
        }
    }
}

void ComponentStatusMonitor::notifyStatusChange(const std::string& host_ip, 
                                              const std::string& instance_id, 
                                              const std::string& uuid,
                                              int index,
                                              const std::string& old_state, 
                                              const std::string& new_state) {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    
    if (m_status_change_callback) {
        try {
            // 在单独的线程中异步调用回调，避免阻塞监控线程
            std::thread callback_thread([this, host_ip, instance_id, uuid, index, old_state, new_state]() {
                try {
                    m_status_change_callback(host_ip, instance_id, uuid, index, old_state, new_state);
                } catch (const std::exception& e) {
                    LogManager::getLogger()->error("Exception in ComponentStatusMonitor callback: {}", e.what());
                } catch (...) {
                    LogManager::getLogger()->error("Unknown exception in ComponentStatusMonitor callback");
                }
            });
            
            // 分离线程，让其在后台运行
            callback_thread.detach();
            
            LogManager::getLogger()->debug("ComponentStatusMonitor callback invoked for component {}:{}:{}:{} ({}->{})", 
                                         host_ip, instance_id, uuid, index, old_state, new_state);
        } catch (const std::exception& e) {
            LogManager::getLogger()->error("Failed to invoke ComponentStatusMonitor callback: {}", e.what());
        }
    }
} 