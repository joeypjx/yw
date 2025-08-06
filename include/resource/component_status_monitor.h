#ifndef COMPONENT_STATUS_MONITOR_H
#define COMPONENT_STATUS_MONITOR_H

#include <memory>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>
#include <string>
#include <vector>
#include "node_storage.h"
#include "alarm_manager.h"

// 组件状态变化回调函数类型
using ComponentStatusChangeCallback = std::function<void(const std::string& host_ip, 
                                                       const std::string& instance_id, 
                                                       const std::string& uuid,
                                                       int index,
                                                       const std::string& old_state, 
                                                       const std::string& new_state)>;

class ComponentStatusMonitor {
private:
    std::shared_ptr<NodeStorage> m_node_storage;
    std::shared_ptr<AlarmManager> m_alarm_manager;
    
    std::thread m_monitor_thread;
    std::mutex m_callback_mutex;
    ComponentStatusChangeCallback m_status_change_callback;
    
    bool m_running;
    std::chrono::seconds m_check_interval;
    std::chrono::seconds m_failed_threshold;  // FAILED状态持续多久后触发告警
    
    // 记录每个组件的状态历史，避免重复告警
    struct ComponentStateHistory {
        std::string state;
        std::chrono::system_clock::time_point last_update;
        bool alarm_triggered;  // 是否已经触发过告警
    };
    
    std::mutex m_history_mutex;
    std::unordered_map<std::string, ComponentStateHistory> m_component_history;  // key: host_ip:instance_id:uuid:index
    
    void run();
    void checkComponentStatus();
    void notifyStatusChange(const std::string& host_ip, 
                           const std::string& instance_id, 
                           const std::string& uuid,
                           int index,
                           const std::string& old_state, 
                           const std::string& new_state);
    
    std::string generateComponentKey(const std::string& host_ip, 
                                   const std::string& instance_id, 
                                   const std::string& uuid, 
                                   int index);

public:
    ComponentStatusMonitor(std::shared_ptr<NodeStorage> node_storage, 
                          std::shared_ptr<AlarmManager> alarm_manager);
    ~ComponentStatusMonitor();
    
    void start();
    void stop();
    
    void setComponentStatusChangeCallback(ComponentStatusChangeCallback callback);
    void clearComponentStatusChangeCallback();
    
    // 设置检查间隔
    void setCheckInterval(std::chrono::seconds interval);
    
    // 设置FAILED状态阈值
    void setFailedThreshold(std::chrono::seconds threshold);
    
    // 获取当前配置
    std::chrono::seconds getCheckInterval() const { return m_check_interval; }
    std::chrono::seconds getFailedThreshold() const { return m_failed_threshold; }
};

#endif // COMPONENT_STATUS_MONITOR_H 