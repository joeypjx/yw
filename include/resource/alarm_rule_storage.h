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
#include "mysql_connection_pool.h"

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
    static constexpr const char* DEFAULT_CHARSET = "utf8mb4";
    static constexpr const char* DEFAULT_COLLATION = "utf8mb4_unicode_ci";
    
    // 连接池注入构造函数 - 推荐使用
    AlarmRuleStorage(std::shared_ptr<MySQLConnectionPool> connection_pool);
    
    // 新的连接池构造函数
    AlarmRuleStorage(const MySQLPoolConfig& pool_config);
    
    // 兼容性构造函数 - 将旧参数转换为连接池配置
    AlarmRuleStorage(const std::string& host, int port, const std::string& user, 
                    const std::string& password, const std::string& database);
    ~AlarmRuleStorage();

    bool initialize();
    void shutdown();
    bool createDatabase();
    bool createTable();
    bool isInitialized() const { return m_initialized; }
    
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

    // 连接池相关方法
    MySQLConnectionPool::PoolStats getConnectionPoolStats() const;
    void updateConnectionPoolConfig(const MySQLPoolConfig& config);

private:
    MySQLPoolConfig m_pool_config;
    std::shared_ptr<MySQLConnectionPool> m_connection_pool;
    std::atomic<bool> m_initialized;
    bool m_owns_connection_pool;  // 标记是否拥有连接池的所有权

    bool executeQuery(const std::string& sql);
    MYSQL_RES* executeSelectQuery(const std::string& sql);
    std::string escapeString(const std::string& str);
    std::string escapeStringWithConnection(const std::string& str, MySQLConnection* connection);
    std::string generateUUID();
    AlarmRule parseRowToAlarmRule(MYSQL_ROW row);
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
    
    // UUID生成器 - 线程安全的静态实例
    static std::mt19937& getRandomGenerator() {
        thread_local static std::random_device rd;
        thread_local static std::mt19937 gen(rd());
        return gen;
    }
    
    // 日志辅助方法
    void logInfo(const std::string& message) const;
    void logError(const std::string& message) const;
    void logDebug(const std::string& message) const;
    void logQueryError(const std::string& query, const std::string& error_msg) const;
    
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