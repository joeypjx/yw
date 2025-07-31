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
      m_mysql(nullptr), m_connected(false),
      m_auto_reconnect_enabled(true),
      m_reconnect_interval_seconds(DEFAULT_RECONNECT_INTERVAL),
      m_max_reconnect_attempts(DEFAULT_MAX_RECONNECT_ATTEMPTS),
      m_current_reconnect_attempts(0),
      m_reconnect_in_progress(false),
      m_stop_reconnect_thread(false),
      m_connection_check_interval_ms(DEFAULT_CONNECTION_CHECK_INTERVAL),
      m_use_exponential_backoff(true),
      m_max_backoff_seconds(DEFAULT_MAX_BACKOFF_SECONDS) {
}

AlarmRuleStorage::~AlarmRuleStorage() {
    // 停止重连线程
    m_stop_reconnect_thread = true;
    if (m_reconnect_thread.joinable()) {
        m_reconnect_thread.join();
    }
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
    
    // 启动自动重连线程
    if (m_auto_reconnect_enabled && !m_reconnect_thread.joinable()) {
        m_stop_reconnect_thread = false;
        m_reconnect_thread = std::thread(&AlarmRuleStorage::reconnectLoop, this);
    }
    
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
                     " CHARACTER SET " + DEFAULT_CHARSET + " COLLATE " + DEFAULT_COLLATION;
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

    std::string sql = "CREATE TABLE IF NOT EXISTS alarm_rules ("
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

    bool table_created = executeQuery(sql);
        
    return table_created;
}

std::string AlarmRuleStorage::insertAlarmRule(const std::string& alert_name, 
                                            const nlohmann::json& expression,
                                            const std::string& for_duration,
                                            const std::string& severity,
                                            const std::string& summary,
                                            const std::string& description,
                                            const std::string& alert_type,
                                            bool enabled) {
    if (!m_connected) {
        LogManager::getLogger()->error("Not connected to MySQL");
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
        << "alert_type = '" << escapeString(alert_type) << "', "
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
    
    if (!checkConnection()) {
        LogManager::getLogger()->error("Not connected to MySQL");
        return rule;
    }

    std::string sql = "SELECT id, alert_name, expression_json, for_duration, severity, summary, description, alert_type, enabled, created_at, updated_at FROM alarm_rules WHERE id = '" + escapeString(id) + "'";
    
    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        LogManager::getLogger()->error("Query failed: {}", mysql_error(m_mysql));
        return rule;
    }

    MYSQL_RES* raw_result = mysql_store_result(m_mysql);
    if (raw_result == nullptr) {
        LogManager::getLogger()->error("Failed to get result: {}", mysql_error(m_mysql));
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
    
    if (!checkConnection()) {
        LogManager::getLogger()->error("Not connected to MySQL");
        return rules;
    }

    std::string sql = "SELECT id, alert_name, expression_json, for_duration, severity, summary, description, alert_type, enabled, created_at, updated_at FROM alarm_rules ORDER BY created_at DESC";
    
    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        LogManager::getLogger()->error("Query failed: {}", mysql_error(m_mysql));
        return rules;
    }

    MYSQL_RES* raw_result = mysql_store_result(m_mysql);
    if (raw_result == nullptr) {
        LogManager::getLogger()->error("Failed to get result: {}", mysql_error(m_mysql));
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
    
    if (!checkConnection()) {
        LogManager::getLogger()->error("Not connected to MySQL");
        return rules;
    }

    std::string sql = "SELECT id, alert_name, expression_json, for_duration, severity, summary, description, alert_type, enabled, created_at, updated_at FROM alarm_rules WHERE enabled = TRUE ORDER BY created_at DESC";
    
    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        LogManager::getLogger()->error("Query failed: {}", mysql_error(m_mysql));
        return rules;
    }

    MYSQL_RES* raw_result = mysql_store_result(m_mysql);
    if (raw_result == nullptr) {
        LogManager::getLogger()->error("Failed to get result: {}", mysql_error(m_mysql));
        return rules;
    }

    MySQLResultRAII result(raw_result);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result.get())) != nullptr) {
        rules.push_back(parseRowToAlarmRule(row));
    }

    return rules;
}

bool AlarmRuleStorage::executeQuery(const std::string& sql) {
    // 检查连接状态
    if (!checkConnection()) {
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
    if (!checkConnection()) {
        return str;
    }

    std::unique_ptr<char[]> escaped(new char[str.length() * 2 + 1]);
    mysql_real_escape_string(m_mysql, escaped.get(), str.c_str(), str.length());
    return std::string(escaped.get());
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
    
    if (!checkConnection()) {
        LogManager::getLogger()->error("Not connected to MySQL");
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
    if (mysql_query(m_mysql, data_query.str().c_str()) != 0) {
        LogManager::getLogger()->error("Data query failed: {}", mysql_error(m_mysql));
        return result;
    }
    
    MYSQL_RES* raw_data_result = mysql_store_result(m_mysql);
    if (raw_data_result == nullptr) {
        LogManager::getLogger()->error("Failed to get data result: {}", mysql_error(m_mysql));
        return result;
    }
    
    MySQLResultRAII data_result(raw_data_result);
    
    // 填充结果数据
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(data_result.get())) != nullptr) {
        result.rules.push_back(parseRowToAlarmRule(row));
    }
    
    // 获取总记录数
    if (mysql_query(m_mysql, "SELECT FOUND_ROWS()") != 0) {
        LogManager::getLogger()->error("FOUND_ROWS query failed: {}", mysql_error(m_mysql));
        return result;
    }
    
    MYSQL_RES* raw_count_result = mysql_store_result(m_mysql);
    if (raw_count_result == nullptr) {
        LogManager::getLogger()->error("Failed to get count result: {}", mysql_error(m_mysql));
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

// 自动重连相关方法实现
void AlarmRuleStorage::enableAutoReconnect(bool enable) {
    m_auto_reconnect_enabled = enable;
    if (enable && !m_reconnect_thread.joinable()) {
        m_stop_reconnect_thread = false;
        m_reconnect_thread = std::thread(&AlarmRuleStorage::reconnectLoop, this);
    }
}

void AlarmRuleStorage::setReconnectInterval(int seconds) {
    if (seconds > 0) {
        m_reconnect_interval_seconds = seconds;
    }
}

void AlarmRuleStorage::setMaxReconnectAttempts(int attempts) {
    if (attempts >= 0) {
        m_max_reconnect_attempts = attempts;
    }
}

bool AlarmRuleStorage::isAutoReconnectEnabled() const {
    return m_auto_reconnect_enabled;
}

int AlarmRuleStorage::getReconnectAttempts() const {
    return m_current_reconnect_attempts;
}

bool AlarmRuleStorage::tryReconnect() {
    std::lock_guard<std::mutex> lock(m_reconnect_mutex);
    
    if (m_reconnect_in_progress) {
        return false;
    }
    
    m_reconnect_in_progress = true;
    m_last_reconnect_attempt = std::chrono::steady_clock::now();
    
    LogManager::getLogger()->info("Attempting to reconnect to MySQL");
    
    // 断开现有连接
    if (m_mysql != nullptr) {
        mysql_close(m_mysql);
        m_mysql = nullptr;
    }
    m_connected = false;
    
    // 尝试重新连接
    m_mysql = mysql_init(nullptr);
    if (m_mysql == nullptr) {
        LogManager::getLogger()->error("Failed to initialize MySQL during reconnect");
        m_reconnect_in_progress = false;
        return false;
    }
    
    if (mysql_real_connect(m_mysql, m_host.c_str(), m_user.c_str(), m_password.c_str(), 
                          nullptr, m_port, nullptr, 0) == nullptr) {
        LogManager::getLogger()->error("Failed to reconnect to MySQL: {}", mysql_error(m_mysql));
        mysql_close(m_mysql);
        m_mysql = nullptr;
        m_reconnect_in_progress = false;
        return false;
    }
    
    // 重新选择数据库
    std::string sql = "USE " + m_database;
    if (mysql_query(m_mysql, sql.c_str()) != 0) {
        LogManager::getLogger()->error("Failed to select database during reconnect: {}", mysql_error(m_mysql));
        mysql_close(m_mysql);
        m_mysql = nullptr;
        m_reconnect_in_progress = false;
        return false;
    }
    
    m_connected = true;
    m_reconnect_in_progress = false;
    resetReconnectAttempts();
    
    LogManager::getLogger()->info("Successfully reconnected to MySQL");
    return true;
}

void AlarmRuleStorage::reconnectLoop() {
    while (!m_stop_reconnect_thread) {
        if (m_auto_reconnect_enabled && !m_connected && shouldAttemptReconnect()) {
            if (tryReconnect()) {
                // 重连成功，等待较长时间再检查
                std::this_thread::sleep_for(std::chrono::seconds(m_reconnect_interval_seconds));
            } else {
                // 重连失败，增加尝试次数
                m_current_reconnect_attempts++;
                
                // 如果达到最大尝试次数，停止自动重连
                if (m_current_reconnect_attempts >= m_max_reconnect_attempts) {
                    LogManager::getLogger()->error("Max reconnect attempts reached, stopping auto-reconnect");
                    m_auto_reconnect_enabled = false;
                    break;
                }
                
                // 使用指数退避计算等待时间
                int backoff_seconds = calculateBackoffInterval();
                std::this_thread::sleep_for(std::chrono::seconds(backoff_seconds));
            }
        } else {
            // 连接正常或不需要重连，使用较长的休眠间隔
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));  // 1秒
        }
    }
}

bool AlarmRuleStorage::checkConnection() {
    if (!m_connected || m_mysql == nullptr) {
        return false;
    }
    
    // 检查是否需要执行连接检测
    if (!shouldCheckConnection()) {
        return true;  // 假设连接正常，避免频繁ping
    }
    
    // 使用ping检查连接是否还活着
    if (mysql_ping(m_mysql) != 0) {
        LogManager::getLogger()->warn("MySQL connection lost, will attempt reconnect");
        m_connected = false;
        return false;
    }
    
    // 更新最后检查时间
    updateLastConnectionCheck();
    return true;
}

void AlarmRuleStorage::resetReconnectAttempts() {
    m_current_reconnect_attempts = 0;
}

bool AlarmRuleStorage::shouldAttemptReconnect() {
    // 检查是否在重连间隔内
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_attempt = std::chrono::duration_cast<std::chrono::seconds>(
        now - m_last_reconnect_attempt).count();
    
    return time_since_last_attempt >= m_reconnect_interval_seconds;
}

// 性能优化相关方法实现
void AlarmRuleStorage::setConnectionCheckInterval(int milliseconds) {
    if (milliseconds > 0) {
        m_connection_check_interval_ms = milliseconds;
    }
}

void AlarmRuleStorage::enableExponentialBackoff(bool enable) {
    m_use_exponential_backoff = enable;
}

void AlarmRuleStorage::setMaxBackoffSeconds(int seconds) {
    if (seconds > 0) {
        m_max_backoff_seconds = seconds;
    }
}

int AlarmRuleStorage::getConnectionCheckInterval() const {
    return m_connection_check_interval_ms;
}

bool AlarmRuleStorage::isExponentialBackoffEnabled() const {
    return m_use_exponential_backoff;
}

bool AlarmRuleStorage::shouldCheckConnection() {
    std::lock_guard<std::mutex> lock(m_connection_check_mutex);
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_check = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_last_connection_check).count();
    
    return time_since_last_check >= m_connection_check_interval_ms;
}

int AlarmRuleStorage::calculateBackoffInterval() {
    if (!m_use_exponential_backoff) {
        return m_reconnect_interval_seconds;
    }
    
    // 指数退避：基础间隔 * 2^(尝试次数-1)，但不超过最大退避时间
    int base_interval = m_reconnect_interval_seconds;
    int backoff_seconds = base_interval * (1 << (m_current_reconnect_attempts - 1));
    
    // 限制最大退避时间
    if (backoff_seconds > m_max_backoff_seconds) {
        backoff_seconds = m_max_backoff_seconds;
    }
    
    return backoff_seconds;
}

void AlarmRuleStorage::updateLastConnectionCheck() {
    std::lock_guard<std::mutex> lock(m_connection_check_mutex);
    m_last_connection_check = std::chrono::steady_clock::now();
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