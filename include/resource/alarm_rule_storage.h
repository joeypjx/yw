#pragma once

#include <string>
#include <memory>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <mysql.h>
#include "json.hpp"
#include <memory>
#include <random>

struct AlarmRule {
    std::string id;
    std::string alert_name;
    std::string expression_json;
    std::string for_duration;
    std::string severity;
    std::string summary;
    std::string description;
    std::string alert_type;  // "硬件状态", "业务链路", "系统故障"
    bool enabled;
    std::string created_at;
    std::string updated_at;
};

// 分页结果结构
struct PaginatedAlarmRules {
    std::vector<AlarmRule> rules;
    int total_count;                   // 总记录数
    int page;                          // 当前页码 (从1开始)
    int page_size;                     // 每页大小
    int total_pages;                   // 总页数
    bool has_next;                     // 是否有下一页
    bool has_prev;                     // 是否有上一页
};

class AlarmRuleStorage {
public:
    // 常量定义
    static constexpr int DEFAULT_PAGE_SIZE = 20;
    static constexpr int MAX_PAGE_SIZE = 1000;
    static constexpr int DEFAULT_RECONNECT_INTERVAL = 5;
    static constexpr int DEFAULT_MAX_RECONNECT_ATTEMPTS = 10;
    static constexpr int DEFAULT_CONNECTION_CHECK_INTERVAL = 5000;
    static constexpr int DEFAULT_MAX_BACKOFF_SECONDS = 60;
    static constexpr const char* DEFAULT_CHARSET = "utf8mb4";
    static constexpr const char* DEFAULT_COLLATION = "utf8mb4_unicode_ci";
    AlarmRuleStorage(const std::string& host, int port, const std::string& user, 
                    const std::string& password, const std::string& database);
    ~AlarmRuleStorage();

    bool connect();
    void disconnect();
    bool createDatabase();
    bool createTable();
    
    std::string insertAlarmRule(const std::string& alert_name, 
                               const nlohmann::json& expression,
                               const std::string& for_duration,
                               const std::string& severity,
                               const std::string& summary,
                               const std::string& description,
                               const std::string& alert_type = "硬件状态",
                               bool enabled = true);
    
    bool updateAlarmRule(const std::string& id, 
                        const std::string& alert_name, 
                        const nlohmann::json& expression,
                        const std::string& for_duration,
                        const std::string& severity,
                        const std::string& summary,
                        const std::string& description,
                        const std::string& alert_type = "硬件状态",
                        bool enabled = true);
    
    bool deleteAlarmRule(const std::string& id);
    AlarmRule getAlarmRule(const std::string& id);
    std::vector<AlarmRule> getAllAlarmRules();
    std::vector<AlarmRule> getEnabledAlarmRules();
    
    // 分页查询功能
    PaginatedAlarmRules getPaginatedAlarmRules(int page = 1, int page_size = 20, bool enabled_only = false);

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
    MYSQL* m_mysql;
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

    bool executeQuery(const std::string& sql);
    std::string escapeString(const std::string& str);
    std::string generateUUID();
    AlarmRule parseRowToAlarmRule(MYSQL_ROW row);
    
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
    
    // UUID生成器 - 线程安全的静态实例
    static std::mt19937& getRandomGenerator() {
        thread_local static std::random_device rd;
        thread_local static std::mt19937 gen(rd());
        return gen;
    }
    
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
};