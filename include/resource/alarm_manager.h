#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <mysql.h>
#include <mysqld_error.h>
#include <errmsg.h>
#include "json.hpp"
#include "mysql_connection_pool.h"

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

    // 构造函数 - 使用连接池配置
    AlarmManager(const MySQLPoolConfig& pool_config);
    // 兼容性构造函数 - 将单连接参数转换为连接池配置
    AlarmManager(const std::string& host, int port, const std::string& user, 
                 const std::string& password, const std::string& database);
    ~AlarmManager();

    // 数据库连接管理 - 使用连接池
    bool initialize();
    void shutdown();
    bool createDatabase();
    bool createEventTable();
    
    // 获取连接池统计信息
    MySQLConnectionPool::PoolStats getConnectionPoolStats() const;

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

    // 连接池配置方法 - 用于调整连接池参数
    void updateConnectionPoolConfig(const MySQLPoolConfig& config);
    bool isInitialized() const { return m_initialized; }

private:
    // 连接池配置和实例
    MySQLPoolConfig m_pool_config;
    std::shared_ptr<MySQLConnectionPool> m_connection_pool;
    std::atomic<bool> m_initialized;
    
    // 数据库操作辅助函数 - 使用连接池
    bool executeQuery(const std::string& query);
    MYSQL_RES* executeSelectQuery(const std::string& query);
    std::string escapeString(const std::string& str);
    std::string escapeStringWithConnection(const std::string& str, MySQLConnection* connection);
    
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
    
    // 连接池辅助方法
    MySQLPoolConfig createDefaultPoolConfig() const;
    
    // RAII包装器用于MySQL结果集
    class MySQLResultRAII {
    public:
        explicit MySQLResultRAII(MYSQL_RES* res) : result(res) {}
        ~MySQLResultRAII() { if (result) mysql_free_result(result); }
        MYSQL_RES* get() const { return result; }
        MYSQL_RES* release() { MYSQL_RES* r = result; result = nullptr; return r; }
    private:
        MYSQL_RES* result;
        MySQLResultRAII(const MySQLResultRAII&) = delete;
        MySQLResultRAII& operator=(const MySQLResultRAII&) = delete;
    };
    
    // 预编译语句帮助方法
    class PreparedStatementRAII {
    public:
        explicit PreparedStatementRAII(MYSQL* mysql, const std::string& query);
        ~PreparedStatementRAII();
        MYSQL_STMT* get() const { return stmt; }
        bool isValid() const { return stmt != nullptr; }
        bool bindParams(MYSQL_BIND* binds);
        bool execute();
        MYSQL_RES* getResult();
    private:
        MYSQL_STMT* stmt;
        PreparedStatementRAII(const PreparedStatementRAII&) = delete;
        PreparedStatementRAII& operator=(const PreparedStatementRAII&) = delete;
    };
    
    // 优化的查询方法
    AlarmEventRecord parseRowToAlarmEventRecord(MYSQL_ROW row);
    std::vector<AlarmEventRecord> executeSelectAlarmEvents(const std::string& query);
    
    // 错误处理和验证方法
    bool validateAlarmEvent(const AlarmEvent& event);
    bool validatePaginationParams(int& page, int& page_size);
    void logQueryError(const std::string& query, const std::string& error_msg);
    bool handleMySQLError(const std::string& operation);
    
    // 批量操作方法
    bool batchInsertAlarmEvents(const std::vector<AlarmEvent>& events);
    bool batchUpdateAlarmEventsToResolved(const std::vector<std::string>& fingerprints);
};