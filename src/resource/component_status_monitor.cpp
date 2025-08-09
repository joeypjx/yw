#include "component_status_monitor.h"
#include "log_manager.h"
#include "alarm_manager.h"           // 实现文件包含，避免头文件传播依赖
#include "alarm_rule_engine.h"       // 为 AlarmEvent 提供类型定义
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

    auto node_list = m_node_storage->getAllNodesReadonly();
    auto now = std::chrono::steady_clock::now();
    
    for (const auto& node : node_list) {
        // 检查节点的组件状态
        for (const auto& component : node->component) {
            std::string component_key = generateComponentKey(node->host_ip, 
                                                          component.instance_id, 
                                                          component.uuid, 
                                                          component.index);
            
            // 读取与更新历史需要最小化持锁
            std::string current_state = component.state;
            bool should_notify_change = false;
            std::string old_state;
            // 仅用于判断与触发回调，不在此处写数据库
            {
                std::lock_guard<std::mutex> history_lock(m_history_mutex);
                auto it = m_component_history.find(component_key);
                if (it == m_component_history.end()) {
                    ComponentStateHistory hist{current_state, now, false};
                    m_component_history.emplace(component_key, hist);
                    LogManager::getLogger()->debug("New component detected: {} (state: {})", component_key, current_state);
                } else {
                    auto& hist = it->second;
                    old_state = hist.state;
                    if (old_state != current_state) {
                        should_notify_change = true;
                        // 更新历史
                        hist.state = current_state;
                        hist.last_update = now;
                        hist.alarm_triggered = (current_state == "FAILED");
                    }
                }
            }

            if (should_notify_change) {
                LogManager::getLogger()->debug("Component state changed: {} ({} -> {})", component_key, old_state, current_state);
                notifyStatusChange(node->host_ip, component.instance_id, component.uuid, component.index, old_state, current_state);
            }

            // DB写入逻辑迁移到上层回调，由AlarmSystem处理
        }
    }
    
    // 清理过期的历史记录（超过1小时没有更新的记录）
    {
        std::lock_guard<std::mutex> history_lock(m_history_mutex);
        auto one_hour_ago = now - std::chrono::hours(1);
        for (auto it = m_component_history.begin(); it != m_component_history.end();) {
            if (it->second.last_update < one_hour_ago) {
                it = m_component_history.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void ComponentStatusMonitor::notifyStatusChange(const std::string& host_ip, 
                                              const std::string& instance_id, 
                                              const std::string& uuid,
                                              int index,
                                              const std::string& old_state, 
                                              const std::string& new_state) {
    ComponentStatusChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_callback_mutex);
        cb = m_status_change_callback;
    }
    if (!cb) return;
    try {
        cb(host_ip, instance_id, uuid, index, old_state, new_state);
        LogManager::getLogger()->debug("ComponentStatusMonitor callback invoked for component {}:{}:{}:{} ({}->{})", 
                                       host_ip, instance_id, uuid, index, old_state, new_state);
    } catch (const std::exception& e) {
        LogManager::getLogger()->error("Exception in ComponentStatusMonitor callback: {}", e.what());
    } catch (...) {
        LogManager::getLogger()->error("Unknown exception in ComponentStatusMonitor callback");
    }
} 