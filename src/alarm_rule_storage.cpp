#include "alarm_rule_storage.h"
#include "log_manager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>

AlarmRuleStorage::AlarmRuleStorage(const std::string& host, int port, const std::string& user, 
                                 const std::string& password, const std::string& database)
    : m_host(host), m_port(port), m_user(user), m_password(password), m_database(database), 
      m_mysql(nullptr), m_connected(false) {
}

AlarmRuleStorage::~AlarmRuleStorage() {
    disconnect();
}

bool AlarmRuleStorage::connect() {
    if (m_connected) {
        return true;
    }

    m_mysql = mysql_init(nullptr);
    if (m_mysql == nullptr) {
        LogManager::getLogger()->error("Failed to initialize MySQL");
        return false;
    }

    if (mysql_real_connect(m_mysql, m_host.c_str(), m_user.c_str(), m_password.c_str(), 
                          nullptr, m_port, nullptr, 0) == nullptr) {
        LogManager::getLogger()->error("Failed to connect to MySQL: {}", mysql_error(m_mysql));
        mysql_close(m_mysql);
        m_mysql = nullptr;
        return false;
    }

    m_connected = true;
    return true;
}

void AlarmRuleStorage::disconnect() {
    if (m_mysql != nullptr) {
        mysql_close(m_mysql);
        m_mysql = nullptr;
    }
    m_connected = false;
}

bool AlarmRuleStorage::createDatabase() {
    if (!m_connected) {
        LogManager::getLogger()->error("Not connected to MySQL");
        return false;
    }

    std::string sql = "CREATE DATABASE IF NOT EXISTS " + m_database + 
                     " CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci";
    if (!executeQuery(sql)) {
        return false;
    }

    sql = "USE " + m_database;
    return executeQuery(sql);
}

bool AlarmRuleStorage::createTable() {
    if (!m_connected) {
        LogManager::getLogger()->error("Not connected to MySQL");
        return false;
    }

    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS alarm_rules (
            id VARCHAR(36) PRIMARY KEY,
            alert_name VARCHAR(255) NOT NULL UNIQUE,
            expression_json TEXT NOT NULL,
            for_duration VARCHAR(32) NOT NULL,
            severity ENUM('info', 'warning', 'critical') NOT NULL,
            summary TEXT NOT NULL,
            description TEXT NOT NULL,
            enabled BOOLEAN DEFAULT TRUE,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
            INDEX idx_alert_name (alert_name),
            INDEX idx_enabled (enabled),
            INDEX idx_severity (severity)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )";

    return executeQuery(sql);
}

std::string AlarmRuleStorage::insertAlarmRule(const std::string& alert_name, 
                                            const nlohmann::json& expression,
                                            const std::string& for_duration,
                                            const std::string& severity,
                                            const std::string& summary,
                                            const std::string& description,
                                            bool enabled) {
    if (!m_connected) {
        LogManager::getLogger()->error("Not connected to MySQL");
        return "";
    }

    std::string id = generateUUID();
    std::string expression_json = expression.dump();
    
    std::ostringstream oss;
    oss << "INSERT INTO alarm_rules (id, alert_name, expression_json, for_duration, severity, summary, description, enabled) VALUES ('"
        << escapeString(id) << "', '"
        << escapeString(alert_name) << "', '"
        << escapeString(expression_json) << "', '"
        << escapeString(for_duration) << "', '"
        << escapeString(severity) << "', '"
        << escapeString(summary) << "', '"
        << escapeString(description) << "', "
        << (enabled ? "TRUE" : "FALSE") << ")";

    if (executeQuery(oss.str())) {
        return id;
    }
    return "";
}

bool AlarmRuleStorage::updateAlarmRule(const std::string& id, 
                                     const std::string& alert_name, 
                                     const nlohmann::json& expression,
                                     const std::string& for_duration,
                                     const std::string& severity,
                                     const std::string& summary,
                                     const std::string& description,
                                     bool enabled) {
    if (!m_connected) {
        LogManager::getLogger()->error("Not connected to MySQL");
        return false;
    }

    std::string expression_json = expression.dump();
    
    std::ostringstream oss;
    oss << "UPDATE alarm_rules SET "
        << "alert_name = '" << escapeString(alert_name) << "', "
        << "expression_json = '" << escapeString(expression_json) << "', "
        << "for_duration = '" << escapeString(for_duration) << "', "
        << "severity = '" << escapeString(severity) << "', "
        << "summary = '" << escapeString(summary) << "', "
        << "description = '" << escapeString(description) << "', "
        << "enabled = " << (enabled ? "TRUE" : "FALSE") << " "
        << "WHERE id = '" << escapeString(id) << "'";

    return executeQuery(oss.str());
}

bool AlarmRuleStorage::deleteAlarmRule(const std::string& id) {
    if (!m_connected) {
        LogManager::getLogger()->error("Not connected to MySQL");
        return false;
    }

    std::string sql = "DELETE FROM alarm_rules WHERE id = '" + escapeString(id) + "'";
    return executeQuery(sql);
}

AlarmRule AlarmRuleStorage::getAlarmRule(const std::string& id) {
    AlarmRule rule;
    
    if (!m_connected) {
        LogManager::getLogger()->error("Not connected to MySQL");
        return rule;
    }

    std::string sql = "SELECT id, alert_name, expression_json, for_duration, severity, summary, description, enabled, created_at, updated_at FROM alarm_rules WHERE id = '" + escapeString(id) + "'";
    
    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        LogManager::getLogger()->error("Query failed: {}", mysql_error(m_mysql));
        return rule;
    }

    MYSQL_RES* result = mysql_store_result(m_mysql);
    if (result == nullptr) {
        LogManager::getLogger()->error("Failed to get result: {}", mysql_error(m_mysql));
        return rule;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (row != nullptr) {
        rule.id = row[0] ? row[0] : "";
        rule.alert_name = row[1] ? row[1] : "";
        rule.expression_json = row[2] ? row[2] : "";
        rule.for_duration = row[3] ? row[3] : "";
        rule.severity = row[4] ? row[4] : "";
        rule.summary = row[5] ? row[5] : "";
        rule.description = row[6] ? row[6] : "";
        rule.enabled = row[7] ? (std::string(row[7]) == "1") : false;
        rule.created_at = row[8] ? row[8] : "";
        rule.updated_at = row[9] ? row[9] : "";
    }

    mysql_free_result(result);
    return rule;
}

std::vector<AlarmRule> AlarmRuleStorage::getAllAlarmRules() {
    std::vector<AlarmRule> rules;
    
    if (!m_connected) {
        LogManager::getLogger()->error("Not connected to MySQL");
        return rules;
    }

    std::string sql = "SELECT id, alert_name, expression_json, for_duration, severity, summary, description, enabled, created_at, updated_at FROM alarm_rules ORDER BY created_at DESC";
    
    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        LogManager::getLogger()->error("Query failed: {}", mysql_error(m_mysql));
        return rules;
    }

    MYSQL_RES* result = mysql_store_result(m_mysql);
    if (result == nullptr) {
        LogManager::getLogger()->error("Failed to get result: {}", mysql_error(m_mysql));
        return rules;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        AlarmRule rule;
        rule.id = row[0] ? row[0] : "";
        rule.alert_name = row[1] ? row[1] : "";
        rule.expression_json = row[2] ? row[2] : "";
        rule.for_duration = row[3] ? row[3] : "";
        rule.severity = row[4] ? row[4] : "";
        rule.summary = row[5] ? row[5] : "";
        rule.description = row[6] ? row[6] : "";
        rule.enabled = row[7] ? (std::string(row[7]) == "1") : false;
        rule.created_at = row[8] ? row[8] : "";
        rule.updated_at = row[9] ? row[9] : "";
        rules.push_back(rule);
    }

    mysql_free_result(result);
    return rules;
}

std::vector<AlarmRule> AlarmRuleStorage::getEnabledAlarmRules() {
    std::vector<AlarmRule> rules;
    
    if (!m_connected) {
        LogManager::getLogger()->error("Not connected to MySQL");
        return rules;
    }

    std::string sql = "SELECT id, alert_name, expression_json, for_duration, severity, summary, description, enabled, created_at, updated_at FROM alarm_rules WHERE enabled = TRUE ORDER BY created_at DESC";
    
    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        LogManager::getLogger()->error("Query failed: {}", mysql_error(m_mysql));
        return rules;
    }

    MYSQL_RES* result = mysql_store_result(m_mysql);
    if (result == nullptr) {
        LogManager::getLogger()->error("Failed to get result: {}", mysql_error(m_mysql));
        return rules;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        AlarmRule rule;
        rule.id = row[0] ? row[0] : "";
        rule.alert_name = row[1] ? row[1] : "";
        rule.expression_json = row[2] ? row[2] : "";
        rule.for_duration = row[3] ? row[3] : "";
        rule.severity = row[4] ? row[4] : "";
        rule.summary = row[5] ? row[5] : "";
        rule.description = row[6] ? row[6] : "";
        rule.enabled = row[7] ? (std::string(row[7]) == "1") : false;
        rule.created_at = row[8] ? row[8] : "";
        rule.updated_at = row[9] ? row[9] : "";
        rules.push_back(rule);
    }

    mysql_free_result(result);
    return rules;
}

bool AlarmRuleStorage::executeQuery(const std::string& sql) {
    if (!m_connected) {
        LogManager::getLogger()->error("Not connected to MySQL");
        return false;
    }

    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        LogManager::getLogger()->error("Query failed: {}", mysql_error(m_mysql));
        LogManager::getLogger()->error("SQL: {}", sql);
        return false;
    }

    return true;
}

std::string AlarmRuleStorage::escapeString(const std::string& str) {
    if (!m_connected) {
        return str;
    }

    char* escaped = new char[str.length() * 2 + 1];
    mysql_real_escape_string(m_mysql, escaped, str.c_str(), str.length());
    std::string result(escaped);
    delete[] escaped;
    return result;
}

std::string AlarmRuleStorage::generateUUID() {
    // Simple UUID v4 generation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);

    std::ostringstream oss;
    oss << std::hex;
    for (int i = 0; i < 8; i++) {
        oss << dis(gen);
    }
    oss << "-";
    for (int i = 0; i < 4; i++) {
        oss << dis(gen);
    }
    oss << "-4";
    for (int i = 0; i < 3; i++) {
        oss << dis(gen);
    }
    oss << "-";
    oss << dis2(gen);
    for (int i = 0; i < 3; i++) {
        oss << dis(gen);
    }
    oss << "-";
    for (int i = 0; i < 12; i++) {
        oss << dis(gen);
    }
    return oss.str();
}