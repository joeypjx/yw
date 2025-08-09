#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <map>
#include "json.hpp"

// 前向声明以避免传播MySQL重依赖
class MySQLConnectionPool;

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
    // 常量定义 - 保留用于向后兼容，但实际使用连接池配置
    static constexpr int DEFAULT_RECONNECT_INTERVAL = 5;
    static constexpr int DEFAULT_MAX_RECONNECT_ATTEMPTS = 10;
    static constexpr int DEFAULT_CONNECTION_CHECK_INTERVAL = 5000;
    static constexpr int DEFAULT_MAX_BACKOFF_SECONDS = 60;

    // 连接池注入构造函数 - 推荐使用
    AlarmManager(std::shared_ptr<MySQLConnectionPool> connection_pool);
    ~AlarmManager();

    // 数据库连接管理 - 使用连接池
    bool initialize();
    void shutdown();
    bool createDatabase();
    bool createEventTable();
    
    // 获取连接池统计信息（去耦合的轻量结构）
    struct ConnectionPoolStats {
        int total_connections = 0;
        int active_connections = 0;
        int idle_connections = 0;
        int pending_requests = 0;
        int created_connections = 0;
        int destroyed_connections = 0;
        double average_wait_time = 0.0;
    };
    ConnectionPoolStats getConnectionPoolStats() const;

    // 核心功能：处理告警事件
    bool processAlarmEvent(const AlarmEvent& event);
    
    // 查询功能
    std::vector<AlarmEventRecord> getActiveAlarmEvents();
    std::vector<AlarmEventRecord> getRecentAlarmEvents(int limit = 100);
    AlarmEventRecord getAlarmEventById(const std::string& id);
    
    // 分页查询功能
    PaginatedAlarmEvents getPaginatedAlarmEvents(int page = 1, int page_size = 20, const std::string& status = "");
    
    // 统计功能
    int getActiveAlarmCount();
    int getTotalAlarmCount();
    
    // 节点状态监控相关方法
    std::string calculateFingerprint(const std::string& alert_name, const std::map<std::string, std::string>& labels);

private:
    std::shared_ptr<MySQLConnectionPool> m_connection_pool;
    std::atomic<bool> m_initialized;
    
    // 数据库操作辅助函数 - 使用连接池（实现细节隐藏在cpp中）
    bool executeQuery(const std::string& query);
    
    // 告警事件处理
    bool insertAlarmEvent(const AlarmEvent& event);
    bool updateAlarmEventToResolved(const std::string& fingerprint, const AlarmEvent& event);
    bool alarmEventExists(const std::string& fingerprint);
    
    // 工具函数
    std::string generateEventId();
    
    // 日志函数
    void logInfo(const std::string& message);
    void logError(const std::string& message);
    void logDebug(const std::string& message);
        
    // 错误处理和验证方法
    bool validateAlarmEvent(const AlarmEvent& event);
    bool validatePaginationParams(int& page, int& page_size);
    void logQueryError(const std::string& query, const std::string& error_msg);
    bool handleMySQLError(const std::string& operation);
};