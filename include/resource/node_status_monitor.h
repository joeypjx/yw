#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <mutex>
#include "node_storage.h"

// 前向声明，避免传播重型依赖
class AlarmManager;

class NodeStatusMonitor {
public:
    // 节点状态变化回调函数类型
    // 参数: host_ip, old_status, new_status
    using NodeStatusChangeCallback = std::function<void(const std::string&, const std::string&, const std::string&)>;
    
    NodeStatusMonitor(std::shared_ptr<NodeStorage> node_storage, std::shared_ptr<AlarmManager> alarm_manager);
    ~NodeStatusMonitor();

    // 禁用拷贝和移动
    NodeStatusMonitor(const NodeStatusMonitor&) = delete;
    NodeStatusMonitor& operator=(const NodeStatusMonitor&) = delete;

    void start();
    void stop();
    
    // 设置节点状态变化回调函数
    void setNodeStatusChangeCallback(NodeStatusChangeCallback callback);
    
    // 清除回调函数
    void clearNodeStatusChangeCallback();

private:
    void run();
    void checkNodeStatus();
    void notifyStatusChange(const std::string& host_ip, const std::string& old_status, const std::string& new_status);
    std::shared_ptr<AlarmManager> m_alarm_manager;
    std::shared_ptr<NodeStorage> m_node_storage;

    std::atomic<bool> m_running;
    std::thread m_monitor_thread;
    
    // 节点状态变化回调函数
    NodeStatusChangeCallback m_status_change_callback;
    std::mutex m_callback_mutex; // 保护回调函数的互斥锁
}; 