#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <mysql.h>
#include "json.hpp"

// 告警事件 (从 alarm_rule_engine.h 复制定义)
struct AlarmEvent;

// 存储在数据库中的告警事件记录
struct AlarmEventRecord {
    std::string id;                    // 事件ID (UUID)
    std::string fingerprint;           // 告警指纹
    std::string status;                // 状态: "firing" 或 "resolved"
    std::string labels_json;           // 标签JSON字符串
    std::string annotations_json;      // 注解JSON字符串
    std::string starts_at;             // 开始时间 (ISO 8601格式)
    std::string ends_at;               // 结束时间 (ISO 8601格式，resolved时有值)
    std::string generator_url;         // 生成器URL
    std::string created_at;            // 记录创建时间
    std::string updated_at;            // 记录更新时间
};

// 分页结果结构
struct PaginatedAlarmEvents {
    std::vector<AlarmEventRecord> events;
    int total_count;                   // 总记录数
    int page;                          // 当前页码 (从1开始)
    int page_size;                     // 每页大小
    int total_pages;                   // 总页数
    bool has_next;                     // 是否有下一页
    bool has_prev;                     // 是否有上一页
};

// 简单告警管理器
// 功能：接收告警事件，存储到MySQL数据库
// 暂不实现：分组、静默、抑制等高级功能
class AlarmManager {
public:
    // 常量定义
    static constexpr int DEFAULT_RECONNECT_INTERVAL = 5;
    static constexpr int DEFAULT_MAX_RECONNECT_ATTEMPTS = 10;
    static constexpr int DEFAULT_CONNECTION_CHECK_INTERVAL = 5000;
    static constexpr int DEFAULT_MAX_BACKOFF_SECONDS = 60;

    AlarmManager(const std::string& host, int port, const std::string& user, 
                 const std::string& password, const std::string& database);
    ~AlarmManager();

    // 数据库连接管理
    bool connect();
    void disconnect();
    bool createDatabase();
    bool createEventTable();
    MYSQL* getConnection() const { return m_connection; }

    // 核心功能：处理告警事件
    bool processAlarmEvent(const AlarmEvent& event);
    
    // 查询功能
    std::vector<AlarmEventRecord> getActiveAlarmEvents();
    std::vector<AlarmEventRecord> getAlarmEventsByFingerprint(const std::string& fingerprint);
    std::vector<AlarmEventRecord> getRecentAlarmEvents(int limit = 100);
    AlarmEventRecord getAlarmEventById(const std::string& id);
    
    // 分页查询功能
    PaginatedAlarmEvents getPaginatedAlarmEvents(int page = 1, int page_size = 20, const std::string& status = "");
    
    // 统计功能
    int getActiveAlarmCount();
    int getTotalAlarmCount();
    
    // 节点状态监控相关方法
    std::string calculateFingerprint(const std::string& alert_name, const std::map<std::string, std::string>& labels);
    bool createOrUpdateAlarm(const std::string& fingerprint, const nlohmann::json& labels, const nlohmann::json& annotations);
    bool resolveAlarm(const std::string& fingerprint);

    // 自动重连相关方法
    void enableAutoReconnect(bool enable = true);
    void setReconnectInterval(int seconds);
    void setMaxReconnectAttempts(int attempts);
    bool isAutoReconnectEnabled() const;
    int getReconnectAttempts() const;
    
    // 性能优化相关方法
    void setConnectionCheckInterval(int milliseconds);
    void enableExponentialBackoff(bool enable = true);
    void setMaxBackoffSeconds(int seconds);
    int getConnectionCheckInterval() const;
    bool isExponentialBackoffEnabled() const;

private:
    std::string m_host;
    int m_port;
    std::string m_user;
    std::string m_password;
    std::string m_database;
    
    MYSQL* m_connection;
    std::atomic<bool> m_connected;

    // 自动重连相关成员变量
    std::atomic<bool> m_auto_reconnect_enabled;
    std::atomic<int> m_reconnect_interval_seconds;
    std::atomic<int> m_max_reconnect_attempts;
    std::atomic<int> m_current_reconnect_attempts;
    std::atomic<bool> m_reconnect_in_progress;
    std::chrono::steady_clock::time_point m_last_reconnect_attempt;
    std::mutex m_reconnect_mutex;
    std::thread m_reconnect_thread;
    std::atomic<bool> m_stop_reconnect_thread;
    
    // 性能优化相关成员变量
    std::chrono::steady_clock::time_point m_last_connection_check;
    std::mutex m_connection_check_mutex;
    std::atomic<int> m_connection_check_interval_ms;
    std::atomic<bool> m_use_exponential_backoff;
    std::atomic<int> m_max_backoff_seconds;
    
    // 数据库操作辅助函数
    bool executeQuery(const std::string& query);
    MYSQL_RES* executeSelectQuery(const std::string& query);
    std::string escapeString(const std::string& str);
    
    // 告警事件处理
    bool insertAlarmEvent(const AlarmEvent& event);
    bool updateAlarmEventToResolved(const std::string& fingerprint, const AlarmEvent& event);
    bool alarmEventExists(const std::string& fingerprint);
    
    // 工具函数
    std::string generateEventId();
    std::string getCurrentTimestamp();
    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);
    
    // 日志函数
    void logInfo(const std::string& message);
    void logError(const std::string& message);
    void logDebug(const std::string& message);
    
    // 自动重连相关私有方法
    bool tryReconnect();
    void reconnectLoop();
    bool checkConnection();
    void resetReconnectAttempts();
    bool shouldAttemptReconnect();
    
    // 性能优化相关私有方法
    bool shouldCheckConnection();
    int calculateBackoffInterval();
    void updateLastConnectionCheck();
};