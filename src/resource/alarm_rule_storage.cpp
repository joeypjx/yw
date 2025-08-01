#include "alarm_rule_storage.h"
#include "log_manager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>

// 新的连接池构造函数
AlarmRuleStorage::AlarmRuleStorage(const MySQLPoolConfig& pool_config)
    : m_pool_config(pool_config), m_initialized(false) {
    m_connection_pool = std::make_shared<MySQLConnectionPool>(m_pool_config);
}

// 兼容性构造函数 - 将旧参数转换为连接池配置
AlarmRuleStorage::AlarmRuleStorage(const std::string& host, int port, const std::string& user, 
                                 const std::string& password, const std::string& database)
    : m_initialized(false) {
    m_pool_config = createDefaultPoolConfig();
    m_pool_config.host = host;
    m_pool_config.port = port;
    m_pool_config.user = user;
    m_pool_config.password = password;
    m_pool_config.database = database;
    
    m_connection_pool = std::make_shared<MySQLConnectionPool>(m_pool_config);
}

AlarmRuleStorage::~AlarmRuleStorage() {
    shutdown();
}

bool AlarmRuleStorage::initialize() {
    if (m_initialized) {
        logInfo("AlarmRuleStorage already initialized");
        return true;
    }
    
    if (!m_connection_pool) {
        logError("Connection pool not created");
        return false;
    }
    
    if (!m_connection_pool->initialize()) {
        logError("Failed to initialize connection pool");
        return false;
    }
    
    m_initialized = true;
    logInfo("AlarmRuleStorage initialized successfully with connection pool");
    return true;
}

void AlarmRuleStorage::shutdown() {
    if (!m_initialized) {
        return;
    }
    
    if (m_connection_pool) {
        m_connection_pool->shutdown();
    }
    
    m_initialized = false;
    logInfo("AlarmRuleStorage shutdown completed");
}

bool AlarmRuleStorage::createDatabase() {
    if (!m_initialized) {
        logError("AlarmRuleStorage not initialized");
        return false;
    }
    
    std::string query = "CREATE DATABASE IF NOT EXISTS " + m_pool_config.database + 
                       " CHARACTER SET " + std::string(DEFAULT_CHARSET) + 
                       " COLLATE " + std::string(DEFAULT_COLLATION);
    
    if (!executeQuery(query)) {
        logError("Failed to create database: " + m_pool_config.database);
        return false;
    }
    
    // 注意: 数据库选择已经在连接池连接时完成
    logInfo("Database created: " + m_pool_config.database);
    return true;
}

bool AlarmRuleStorage::createTable() {
    if (!m_initialized) {
        logError("AlarmRuleStorage not initialized");
        return false;
    }
    
    std::string query = "CREATE TABLE IF NOT EXISTS alarm_rules ("
                       "id VARCHAR(36) PRIMARY KEY,"
                       "alert_name VARCHAR(255) NOT NULL UNIQUE,"
                       "expression_json TEXT NOT NULL,"
                       "for_duration VARCHAR(32) NOT NULL,"
                       "severity VARCHAR(32) NOT NULL,"
                       "summary TEXT NOT NULL,"
                       "description TEXT NOT NULL,"
                       "alert_type VARCHAR(255) NOT NULL,"
                       "enabled BOOLEAN DEFAULT TRUE,"
                       "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                       "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
                       "INDEX idx_alert_name (alert_name),"
                       "INDEX idx_enabled (enabled),"
                       "INDEX idx_severity (severity),"
                       "INDEX idx_alert_type (alert_type)"
                       ") ENGINE=InnoDB DEFAULT CHARSET=" + std::string(DEFAULT_CHARSET) + 
                       " COLLATE=" + std::string(DEFAULT_COLLATION);
    
    if (!executeQuery(query)) {
        logError("Failed to create alarm_rules table");
        return false;
    }
    
    logInfo("Alarm rules table created successfully");
    return true;
}

std::string AlarmRuleStorage::insertAlarmRule(const std::string& alert_name, 
                                            const nlohmann::json& expression,
                                            const std::string& for_duration,
                                            const std::string& severity,
                                            const std::string& summary,
                                            const std::string& description,
                                            const std::string& alert_type,
                                            bool enabled) {
    if (!m_initialized) {
        logError("AlarmRuleStorage not initialized");
        return "";
    }

    std::string id = generateUUID();
    std::string expression_json = expression.dump();
    
    std::ostringstream oss;
    oss << "INSERT INTO alarm_rules (id, alert_name, expression_json, for_duration, severity, summary, description, alert_type, enabled) VALUES ('"
        << escapeString(id) << "', '"
        << escapeString(alert_name) << "', '"
        << escapeString(expression_json) << "', '"
        << escapeString(for_duration) << "', '"
        << escapeString(severity) << "', '"
        << escapeString(summary) << "', '"
        << escapeString(description) << "', '"
        << escapeString(alert_type) << "', "
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
                                     const std::string& alert_type,
                                     bool enabled) {
    if (!m_initialized) {
        logError("AlarmRuleStorage not initialized");
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
        << "alert_type = '" << escapeString(alert_type) << "', "
        << "enabled = " << (enabled ? "TRUE" : "FALSE") << " "
        << "WHERE id = '" << escapeString(id) << "'";

    return executeQuery(oss.str());
}

bool AlarmRuleStorage::deleteAlarmRule(const std::string& id) {
    if (!m_initialized) {
        logError("AlarmRuleStorage not initialized");
        return false;
    }

    std::string sql = "DELETE FROM alarm_rules WHERE id = '" + escapeString(id) + "'";
    return executeQuery(sql);
}

AlarmRule AlarmRuleStorage::getAlarmRule(const std::string& id) {
    AlarmRule rule;
    
    if (!m_initialized) {
        logError("AlarmRuleStorage not initialized");
        return rule;
    }

    std::string sql = "SELECT id, alert_name, expression_json, for_duration, severity, summary, description, alert_type, enabled, created_at, updated_at FROM alarm_rules WHERE id = '" + escapeString(id) + "'";
    
    MYSQL_RES* raw_result = executeSelectQuery(sql);
    if (!raw_result) {
        return rule;
    }

    MySQLResultRAII result(raw_result);
    MYSQL_ROW row = mysql_fetch_row(result.get());
    if (row != nullptr) {
        rule = parseRowToAlarmRule(row);
    }

    return rule;
}

std::vector<AlarmRule> AlarmRuleStorage::getAllAlarmRules() {
    std::vector<AlarmRule> rules;
    
    if (!m_initialized) {
        logError("AlarmRuleStorage not initialized");
        return rules;
    }

    std::string sql = "SELECT id, alert_name, expression_json, for_duration, severity, summary, description, alert_type, enabled, created_at, updated_at FROM alarm_rules ORDER BY created_at DESC";
    
    MYSQL_RES* raw_result = executeSelectQuery(sql);
    if (!raw_result) {
        return rules;
    }

    MySQLResultRAII result(raw_result);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result.get())) != nullptr) {
        rules.push_back(parseRowToAlarmRule(row));
    }

    return rules;
}

std::vector<AlarmRule> AlarmRuleStorage::getEnabledAlarmRules() {
    std::vector<AlarmRule> rules;
    
    if (!m_initialized) {
        logError("AlarmRuleStorage not initialized");
        return rules;
    }

    std::string sql = "SELECT id, alert_name, expression_json, for_duration, severity, summary, description, alert_type, enabled, created_at, updated_at FROM alarm_rules WHERE enabled = TRUE ORDER BY created_at DESC";
    
    MYSQL_RES* raw_result = executeSelectQuery(sql);
    if (!raw_result) {
        return rules;
    }

    MySQLResultRAII result(raw_result);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result.get())) != nullptr) {
        rules.push_back(parseRowToAlarmRule(row));
    }

    return rules;
}

bool AlarmRuleStorage::executeQuery(const std::string& query) {
    if (!m_initialized) {
        logError("AlarmRuleStorage not initialized");
        return false;
    }
    
    MySQLConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) {
        logError("Failed to get database connection from pool");
        return false;
    }
    
    logDebug("Executing query: " + query);
    
    MYSQL* mysql = guard->get();
    if (mysql_query(mysql, query.c_str()) != 0) {
        logQueryError(query, mysql_error(mysql));
        return false;
    }
    
    return true;
}

MYSQL_RES* AlarmRuleStorage::executeSelectQuery(const std::string& query) {
    if (!m_initialized) {
        logError("AlarmRuleStorage not initialized");
        return nullptr;
    }
    
    MySQLConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) {
        logError("Failed to get database connection from pool");
        return nullptr;
    }
    
    logDebug("Executing query: " + query);
    
    MYSQL* mysql = guard->get();
    if (mysql_query(mysql, query.c_str()) != 0) {
        logQueryError(query, mysql_error(mysql));
        return nullptr;
    }
    
    MYSQL_RES* result = mysql_store_result(mysql);
    if (!result) {
        logError("Failed to store result: " + std::string(mysql_error(mysql)));
        return nullptr;
    }
    
    return result;
}

std::string AlarmRuleStorage::escapeString(const std::string& str) {
    if (!m_initialized) {
        // 如果未初始化，返回简单转义的字符串
        std::string escaped = str;
        size_t pos = 0;
        while ((pos = escaped.find("'", pos)) != std::string::npos) {
            escaped.replace(pos, 1, "\\'");
            pos += 2;
        }
        return escaped;
    }
    
    MySQLConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) {
        // 如果无法获取连接，使用简单转义
        std::string escaped = str;
        size_t pos = 0;
        while ((pos = escaped.find("'", pos)) != std::string::npos) {
            escaped.replace(pos, 1, "\\'");
            pos += 2;
        }
        return escaped;
    }
    
    return escapeStringWithConnection(str, guard.get());
}

std::string AlarmRuleStorage::escapeStringWithConnection(const std::string& str, MySQLConnection* connection) {
    if (!connection || !connection->get()) {
        // 如果没有提供连接，返回简单转义的字符串
        std::string escaped = str;
        size_t pos = 0;
        while ((pos = escaped.find("'", pos)) != std::string::npos) {
            escaped.replace(pos, 1, "\\'");
            pos += 2;
        }
        return escaped;
    }
    
    MYSQL* mysql = connection->get();
    char* escaped = new char[str.length() * 2 + 1];
    mysql_real_escape_string(mysql, escaped, str.c_str(), str.length());
    
    std::string result(escaped);
    delete[] escaped;
    
    return result;
}

AlarmRule AlarmRuleStorage::parseRowToAlarmRule(MYSQL_ROW row) {
    AlarmRule rule;
    if (row) {
        rule.id = row[0] ? row[0] : "";
        rule.alert_name = row[1] ? row[1] : "";
        rule.expression_json = row[2] ? row[2] : "";
        rule.for_duration = row[3] ? row[3] : "";
        rule.severity = row[4] ? row[4] : "";
        rule.summary = row[5] ? row[5] : "";
        rule.description = row[6] ? row[6] : "";
        rule.alert_type = row[7] ? row[7] : "";
        rule.enabled = row[8] ? (std::string(row[8]) == "1") : false;
        rule.created_at = row[9] ? row[9] : "";
        rule.updated_at = row[10] ? row[10] : "";
    }
    return rule;
}

std::string AlarmRuleStorage::generateUUID() {
    // Simple UUID v4 generation
    auto& gen = getRandomGenerator();
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

PaginatedAlarmRules AlarmRuleStorage::getPaginatedAlarmRules(int page, int page_size, bool enabled_only) {
    PaginatedAlarmRules result;
    result.page = page;
    result.page_size = page_size;
    result.total_count = 0;
    result.total_pages = 0;
    result.has_next = false;
    result.has_prev = false;
    
    if (!m_initialized) {
        logError("AlarmRuleStorage not initialized");
        return result;
    }
    
    // 验证参数
    if (page < 1) page = 1;
    if (page_size < 1) page_size = DEFAULT_PAGE_SIZE;
    if (page_size > MAX_PAGE_SIZE) page_size = MAX_PAGE_SIZE;
    
    result.page = page;
    result.page_size = page_size;
    
    // 计算OFFSET
    int offset = (page - 1) * page_size;
    
    // 构建带SQL_CALC_FOUND_ROWS的查询，一次查询获取数据和总数
    std::ostringstream data_query;
    data_query << "SELECT SQL_CALC_FOUND_ROWS id, alert_name, expression_json, for_duration, severity, summary, description, alert_type, enabled, created_at, updated_at "
               << "FROM alarm_rules";
    
    if (enabled_only) {
        data_query << " WHERE enabled = 1";
    }
    
    data_query << " ORDER BY created_at DESC LIMIT " << page_size << " OFFSET " << offset;
    
    // 执行数据查询
    MYSQL_RES* raw_data_result = executeSelectQuery(data_query.str());
    if (!raw_data_result) {
        return result;
    }
    
    MySQLResultRAII data_result(raw_data_result);
    
    // 填充结果数据
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(data_result.get())) != nullptr) {
        result.rules.push_back(parseRowToAlarmRule(row));
    }
    
    // 获取总记录数
    MYSQL_RES* raw_count_result = executeSelectQuery("SELECT FOUND_ROWS()");
    if (!raw_count_result) {
        return result;
    }
    
    MySQLResultRAII count_result(raw_count_result);
    MYSQL_ROW count_row = mysql_fetch_row(count_result.get());
    if (count_row && count_row[0]) {
        result.total_count = std::stoi(count_row[0]);
    }
    
    // 计算总页数
    result.total_pages = (result.total_count + page_size - 1) / page_size;
    
    // 设置分页状态
    result.has_prev = page > 1;
    result.has_next = page < result.total_pages;
    return result;
}

// 新增的连接池相关方法
MySQLConnectionPool::PoolStats AlarmRuleStorage::getConnectionPoolStats() const {
    if (!m_connection_pool) {
        return MySQLConnectionPool::PoolStats{};
    }
    return m_connection_pool->getStats();
}

void AlarmRuleStorage::updateConnectionPoolConfig(const MySQLPoolConfig& config) {
    m_pool_config = config;
    // 注意：实际应用中，这里可能需要重新创建连接池
    logInfo("Connection pool configuration updated");
}

MySQLPoolConfig AlarmRuleStorage::createDefaultPoolConfig() const {
    MySQLPoolConfig config;
    config.host = "localhost";
    config.port = 3306;
    config.user = "root";
    config.password = "";
    config.database = "";
    config.charset = "utf8mb4";
    
    // 连接池配置
    config.min_connections = 2;
    config.max_connections = 8;
    config.initial_connections = 3;
    
    // 超时配置
    config.connection_timeout = 30;
    config.idle_timeout = 600;      // 10分钟
    config.max_lifetime = 3600;     // 1小时
    config.acquire_timeout = 10;
    
    // 健康检查配置
    config.health_check_interval = 60;
    config.health_check_query = "SELECT 1";
    
    return config;
}

// 日志辅助方法
void AlarmRuleStorage::logInfo(const std::string& message) const {
    LogManager::getLogger()->info("AlarmRuleStorage: {}", message);
}

void AlarmRuleStorage::logError(const std::string& message) const {
    LogManager::getLogger()->error("AlarmRuleStorage: {}", message);
}

void AlarmRuleStorage::logDebug(const std::string& message) const {
    LogManager::getLogger()->debug("AlarmRuleStorage: {}", message);
}

void AlarmRuleStorage::logQueryError(const std::string& query, const std::string& error_msg) const {
    logError("Query failed: " + error_msg);
    logError("SQL: " + query);
}

// PreparedStatementRAII 实现
AlarmRuleStorage::PreparedStatementRAII::PreparedStatementRAII(MYSQL* mysql, const std::string& query) 
    : stmt(nullptr) {
    if (mysql) {
        stmt = mysql_stmt_init(mysql);
        if (stmt) {
            if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0) {
                mysql_stmt_close(stmt);
                stmt = nullptr;
            }
        }
    }
}

AlarmRuleStorage::PreparedStatementRAII::~PreparedStatementRAII() {
    if (stmt) {
        mysql_stmt_close(stmt);
    }
}

bool AlarmRuleStorage::PreparedStatementRAII::bindParams(MYSQL_BIND* binds) {
    if (!stmt) return false;
    return mysql_stmt_bind_param(stmt, binds) == 0;
}

bool AlarmRuleStorage::PreparedStatementRAII::execute() {
    if (!stmt) return false;
    return mysql_stmt_execute(stmt) == 0;
}

MYSQL_RES* AlarmRuleStorage::PreparedStatementRAII::getResult() {
    if (!stmt) return nullptr;
    return mysql_stmt_result_metadata(stmt);
}