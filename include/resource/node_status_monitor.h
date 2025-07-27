#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include "node_storage.h"
#include "alarm_manager.h"

class NodeStatusMonitor {
public:
    NodeStatusMonitor(std::shared_ptr<NodeStorage> node_storage, std::shared_ptr<AlarmManager> alarm_manager);
    ~NodeStatusMonitor();

    // 禁用拷贝和移动
    NodeStatusMonitor(const NodeStatusMonitor&) = delete;
    NodeStatusMonitor& operator=(const NodeStatusMonitor&) = delete;

    void start();
    void stop();

private:
    void run();
    void checkNodeStatus();

    std::shared_ptr<NodeStorage> m_node_storage;
    std::shared_ptr<AlarmManager> m_alarm_manager;

    std::atomic<bool> m_running;
    std::thread m_monitor_thread;

    const std::chrono::seconds m_check_interval{1}; // 每秒检查一次
    const std::chrono::seconds m_offline_threshold{5}; // 5秒无心跳则认为离线
}; 