#pragma once

#include <string>
#include <memory>
#include <chrono>
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

private:
    std::string m_host;
    int m_port;
    std::string m_user;
    std::string m_password;
    std::string m_database;
    
    MYSQL* m_connection;
    
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
};