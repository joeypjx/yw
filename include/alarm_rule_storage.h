#pragma once

#include <string>
#include <memory>
#include <vector>
#include <mysql.h>
#include "json.hpp"

struct AlarmRule {
    std::string id;
    std::string alert_name;
    std::string expression_json;
    std::string for_duration;
    std::string severity;
    std::string summary;
    std::string description;
    std::string alarm_type;  // "硬件状态", "业务链路", "系统故障"
    bool enabled;
    std::string created_at;
    std::string updated_at;
};

class AlarmRuleStorage {
public:
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
                               const std::string& alarm_type = "硬件状态",
                               bool enabled = true);
    
    bool updateAlarmRule(const std::string& id, 
                        const std::string& alert_name, 
                        const nlohmann::json& expression,
                        const std::string& for_duration,
                        const std::string& severity,
                        const std::string& summary,
                        const std::string& description,
                        const std::string& alarm_type = "硬件状态",
                        bool enabled = true);
    
    bool deleteAlarmRule(const std::string& id);
    AlarmRule getAlarmRule(const std::string& id);
    std::vector<AlarmRule> getAllAlarmRules();
    std::vector<AlarmRule> getEnabledAlarmRules();

private:
    std::string m_host;
    int m_port;
    std::string m_user;
    std::string m_password;
    std::string m_database;
    MYSQL* m_mysql;
    bool m_connected;

    bool executeQuery(const std::string& sql);
    std::string escapeString(const std::string& str);
    std::string generateUUID();
};