#include "../../include/resource/alarm_manager.h"
#include "../../include/resource/alarm_rule_engine.h"
#include "../../include/resource/log_manager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <uuid/uuid.h>
#include <cstring>

// MySQL错误码兼容性定义
#ifndef CR_SERVER_GONE_ERROR
#define CR_SERVER_GONE_ERROR 2006
#endif

#ifndef CR_SERVER_LOST
#define CR_SERVER_LOST 2013
#endif

// 连接池注入构造函数 - 推荐使用
AlarmManager::AlarmManager(std::shared_ptr<MySQLConnectionPool> connection_pool)
    : m_connection_pool(connection_pool), m_initialized(false), m_owns_connection_pool(false) {
    if (!m_connection_pool) {
        logError("Injected connection pool is null");
    }
}

// 新的连接池构造函数
AlarmManager::AlarmManager(const MySQLPoolConfig& pool_config)
    : m_pool_config(pool_config), m_initialized(false), m_owns_connection_pool(true) {
    m_connection_pool = std::make_shared<MySQLConnectionPool>(m_pool_config);
}

// 兼容性构造函数 - 将旧参数转换为连接池配置
AlarmManager::AlarmManager(const std::string& host, int port, const std::string& user,
                           const std::string& password, const std::string& database)
    : m_initialized(false), m_owns_connection_pool(true) {
    m_pool_config = createDefaultPoolConfig();
    m_pool_config.host = host;
    m_pool_config.port = port;
    m_pool_config.user = user;
    m_pool_config.password = password;
    m_pool_config.database = database;
    
    m_connection_pool = std::make_shared<MySQLConnectionPool>(m_pool_config);
}

AlarmManager::~AlarmManager() {
    shutdown();
}

bool AlarmManager::initialize() {
    if (m_initialized) {
        logInfo("AlarmManager already initialized");
        return true;
    }
    
    if (!m_connection_pool) {
        logError("Connection pool not created");
        return false;
    }
    
    // 只有当我们拥有连接池时才需要初始化它
    if (m_owns_connection_pool) {
        if (!m_connection_pool->initialize()) {
            logError("Failed to initialize connection pool");
            return false;
        }
    }
    
    m_initialized = true;
    logInfo("AlarmManager initialized successfully with connection pool");
    return true;
}

void AlarmManager::shutdown() {
    if (!m_initialized) {
        return;
    }
    
    // 只有当我们拥有连接池时才关闭它
    if (m_connection_pool && m_owns_connection_pool) {
        m_connection_pool->shutdown();
    }
    
    m_initialized = false;
    logInfo("AlarmManager shutdown completed");
}

bool AlarmManager::createDatabase() {
    if (!m_initialized) {
        logError("AlarmManager not initialized");
        return false;
    }
    
    std::string query = "CREATE DATABASE IF NOT EXISTS " + m_pool_config.database + 
                       " CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci";
    
    if (!executeQuery(query)) {
        logError("Failed to create database: " + m_pool_config.database);
        return false;
    }
    
    // 注意: 数据库选择已经在连接池连接时完成
    logInfo("Database created: " + m_pool_config.database);
    return true;
}

bool AlarmManager::createEventTable() {
    if (!m_initialized) {
        logError("AlarmManager not initialized");
        return false;
    }
    
    std::string query = R"(
        CREATE TABLE IF NOT EXISTS alarm_events (
            id VARCHAR(36) PRIMARY KEY,
            fingerprint VARCHAR(512) NOT NULL,
            status ENUM('firing', 'resolved') NOT NULL,
            labels_json TEXT NOT NULL,
            annotations_json TEXT NOT NULL,
            starts_at DATETIME NOT NULL,
            ends_at DATETIME NULL,
            generator_url VARCHAR(1024),
            created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
            
            INDEX idx_fingerprint (fingerprint),
            INDEX idx_status (status),
            INDEX idx_starts_at (starts_at),
            INDEX idx_created_at (created_at)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )";
    
    if (!executeQuery(query)) {
        logError("Failed to create alarm_events table");
        return false;
    }
    
    logInfo("Alarm events table created successfully");
    return true;
}

bool AlarmManager::processAlarmEvent(const AlarmEvent& event) {
    if (!validateAlarmEvent(event)) {
        return false;
    }
    
    if (!m_initialized) {
        logError("AlarmManager not initialized");
        return false;
    }
    
    logInfo("Processing alarm event: " + event.fingerprint + " - " + event.status);
    
    if (event.status == "firing") {
        // 对于firing状态的告警，插入新记录
        return insertAlarmEvent(event);
    } else if (event.status == "resolved") {
        // 对于resolved状态的告警，更新现有记录
        return updateAlarmEventToResolved(event.fingerprint, event);
    } else {
        logError("Unknown alarm event status: " + event.status);
        return false;
    }
}

bool AlarmManager::insertAlarmEvent(const AlarmEvent& event) {
    std::string id = generateEventId();
    std::string fingerprint = escapeString(event.fingerprint);
    std::string status = escapeString(event.status);
    
    // 序列化标签为JSON
    nlohmann::json labels_json = event.labels;
    std::string labels_str = escapeString(labels_json.dump());
    
    // 序列化注解为JSON
    nlohmann::json annotations_json = event.annotations;
    std::string annotations_str = escapeString(annotations_json.dump());
    
    std::string starts_at = formatTimestamp(event.starts_at);
    std::string generator_url = escapeString(event.generator_url);
    
    std::ostringstream query;
    query << "INSERT INTO alarm_events (id, fingerprint, status, labels_json, annotations_json, "
          << "starts_at, generator_url) VALUES ('"
          << id << "', '"
          << fingerprint << "', '"
          << status << "', '"
          << labels_str << "', '"
          << annotations_str << "', '"
          << starts_at << "', '"
          << generator_url << "')";
    
    if (!executeQuery(query.str())) {
        logError("Failed to insert alarm event: " + event.fingerprint);
        return false;
    }
    
    logInfo("Alarm event inserted successfully: " + event.fingerprint);
    return true;
}

bool AlarmManager::updateAlarmEventToResolved(const std::string& fingerprint, const AlarmEvent& event) {
    std::string escaped_fingerprint = escapeString(fingerprint);
    std::string ends_at = formatTimestamp(event.ends_at);
    
    std::ostringstream query;
    query << "UPDATE alarm_events SET status = 'resolved', ends_at = '"
          << ends_at << "' WHERE fingerprint = '"
          << escaped_fingerprint << "' AND status = 'firing'";
    
    if (!executeQuery(query.str())) {
        logError("Failed to update alarm event to resolved: " + fingerprint);
        return false;
    }
    
    // 注意：使用连接池时，无法直接获取affected_rows
    // 在实际应用中，可以通过SELECT COUNT查询来验证更新结果
    
    logInfo("Alarm event updated to resolved: " + fingerprint);
    return true;
}

bool AlarmManager::alarmEventExists(const std::string& fingerprint) {
    std::string escaped_fingerprint = escapeString(fingerprint);
    
    std::ostringstream query;
    query << "SELECT COUNT(*) FROM alarm_events WHERE fingerprint = '"
          << escaped_fingerprint << "' AND status = 'firing'";
    
    MYSQL_RES* raw_result = executeSelectQuery(query.str());
    if (!raw_result) {
        return false;
    }
    
    MySQLResultRAII result(raw_result);
    MYSQL_ROW row = mysql_fetch_row(result.get());
    int count = row ? std::stoi(row[0]) : 0;
    
    return count > 0;
}

std::vector<AlarmEventRecord> AlarmManager::getActiveAlarmEvents() {
    std::string query = "SELECT id, fingerprint, status, labels_json, annotations_json, "
                       "starts_at, ends_at, generator_url, created_at, updated_at "
                       "FROM alarm_events WHERE status = 'firing' ORDER BY starts_at DESC";
    
    return executeSelectAlarmEvents(query);
}

std::vector<AlarmEventRecord> AlarmManager::getAlarmEventsByFingerprint(const std::string& fingerprint) {
    std::string escaped_fingerprint = escapeString(fingerprint);
    
    std::ostringstream query;
    query << "SELECT id, fingerprint, status, labels_json, annotations_json, "
          << "starts_at, ends_at, generator_url, created_at, updated_at "
          << "FROM alarm_events WHERE fingerprint = '"
          << escaped_fingerprint << "' ORDER BY starts_at DESC";
    
    return executeSelectAlarmEvents(query.str());
}

std::vector<AlarmEventRecord> AlarmManager::getRecentAlarmEvents(int limit) {
    std::ostringstream query;
    query << "SELECT id, fingerprint, status, labels_json, annotations_json, "
          << "starts_at, ends_at, generator_url, created_at, updated_at "
          << "FROM alarm_events ORDER BY created_at DESC LIMIT " << limit;
    
    return executeSelectAlarmEvents(query.str());
}

AlarmEventRecord AlarmManager::getAlarmEventById(const std::string& id) {
    AlarmEventRecord record;
    
    std::string escaped_id = escapeString(id);
    
    std::ostringstream query;
    query << "SELECT id, fingerprint, status, labels_json, annotations_json, "
          << "starts_at, ends_at, generator_url, created_at, updated_at "
          << "FROM alarm_events WHERE id = '" << escaped_id << "'";
    
    MYSQL_RES* raw_result = executeSelectQuery(query.str());
    if (!raw_result) {
        return record;
    }
    
    MySQLResultRAII result(raw_result);
    MYSQL_ROW row = mysql_fetch_row(result.get());
    if (row) {
        record = parseRowToAlarmEventRecord(row);
    }
    
    return record;
}

int AlarmManager::getActiveAlarmCount() {
    std::string query = "SELECT COUNT(*) FROM alarm_events WHERE status = 'firing'";
    
    MYSQL_RES* raw_result = executeSelectQuery(query);
    if (!raw_result) {
        return 0;
    }
    
    MySQLResultRAII result(raw_result);
    MYSQL_ROW row = mysql_fetch_row(result.get());
    int count = row ? std::stoi(row[0]) : 0;
    
    return count;
}

int AlarmManager::getTotalAlarmCount() {
    std::string query = "SELECT COUNT(*) FROM alarm_events";
    
    MYSQL_RES* raw_result = executeSelectQuery(query);
    if (!raw_result) {
        return 0;
    }
    
    MySQLResultRAII result(raw_result);
    MYSQL_ROW row = mysql_fetch_row(result.get());
    int count = row ? std::stoi(row[0]) : 0;
    
    return count;
}

PaginatedAlarmEvents AlarmManager::getPaginatedAlarmEvents(int page, int page_size, const std::string& status) {
    PaginatedAlarmEvents result;
    result.page = page;
    result.page_size = page_size;
    result.total_count = 0;
    result.total_pages = 0;
    result.has_next = false;
    result.has_prev = false;
    
    // 验证和修正参数
    validatePaginationParams(page, page_size);
    
    result.page = page;
    result.page_size = page_size;
    
    // 构建COUNT查询来获取总记录数
    std::ostringstream count_query;
    count_query << "SELECT COUNT(*) FROM alarm_events";
    if (!status.empty()) {
        std::string escaped_status = escapeString(status);
        count_query << " WHERE status = '" << escaped_status << "'";
    }
    
    // 获取总记录数
    MYSQL_RES* raw_count_result = executeSelectQuery(count_query.str());
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
    
    // 如果没有数据，直接返回
    if (result.total_count == 0) {
        return result;
    }
    
    // 计算OFFSET
    int offset = (page - 1) * page_size;
    
    // 构建数据查询
    std::ostringstream data_query;
    data_query << "SELECT id, fingerprint, status, labels_json, annotations_json, "
               << "starts_at, ends_at, generator_url, created_at, updated_at "
               << "FROM alarm_events";
    
    if (!status.empty()) {
        std::string escaped_status = escapeString(status);
        data_query << " WHERE status = '" << escaped_status << "'";
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
        result.events.push_back(parseRowToAlarmEventRecord(row));
    }
    
    return result;
}

bool AlarmManager::executeQuery(const std::string& query) {
    if (!m_initialized) {
        logError("AlarmManager not initialized");
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

MYSQL_RES* AlarmManager::executeSelectQuery(const std::string& query) {
    if (!m_initialized) {
        logError("AlarmManager not initialized");
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

std::string AlarmManager::escapeString(const std::string& str) {
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

std::string AlarmManager::escapeStringWithConnection(const std::string& str, MySQLConnection* connection) {
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

std::string AlarmManager::generateEventId() {
    uuid_t uuid;
    uuid_generate(uuid);
    
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);
    
    return std::string(uuid_str);
}

// 新增的连接池相关方法
MySQLConnectionPool::PoolStats AlarmManager::getConnectionPoolStats() const {
    if (!m_connection_pool) {
        return MySQLConnectionPool::PoolStats{};
    }
    return m_connection_pool->getStats();
}

void AlarmManager::updateConnectionPoolConfig(const MySQLPoolConfig& config) {
    m_pool_config = config;
    // 注意：实际应用中，这里可能需要重新创建连接池
    logInfo("Connection pool configuration updated");
}

MySQLPoolConfig AlarmManager::createDefaultPoolConfig() const {
    MySQLPoolConfig config;
    config.host = "localhost";
    config.port = 3306;
    config.user = "root";
    config.password = "";
    config.database = "";
    config.charset = "utf8mb4";
    
    // 连接池配置
    config.min_connections = 3;
    config.max_connections = 10;
    config.initial_connections = 5;
    
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

std::string AlarmManager::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    return formatTimestamp(now);
}

std::string AlarmManager::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    
    return oss.str();
}

void AlarmManager::logInfo(const std::string& message) {
    LogManager::getLogger()->info("AlarmManager: {}", message);
}

void AlarmManager::logError(const std::string& message) {
    LogManager::getLogger()->error("AlarmManager: {}", message);
}

void AlarmManager::logDebug(const std::string& message) {
    LogManager::getLogger()->debug("AlarmManager: {}", message);
}

std::string AlarmManager::calculateFingerprint(const std::string& alert_name, const std::map<std::string, std::string>& labels) {
    std::ostringstream fingerprint;
    fingerprint << "alertname=" << alert_name;
    
    std::vector<std::pair<std::string, std::string>> sorted_labels(labels.begin(), labels.end());
    std::sort(sorted_labels.begin(), sorted_labels.end());
    
    for (const auto& label : sorted_labels) {
        fingerprint << "," << label.first << "=" << label.second;
    }
    
    return fingerprint.str();
}

bool AlarmManager::createOrUpdateAlarm(const std::string& fingerprint, const nlohmann::json& labels, const nlohmann::json& annotations) {
    // 检查是否已存在该指纹的告警
    if (alarmEventExists(fingerprint)) {
        logDebug("Alarm already exists for fingerprint: " + fingerprint);
        return true;
    }
    
    // 创建新的告警事件
    AlarmEvent event;
    event.fingerprint = fingerprint;
    event.status = "firing";
    event.starts_at = std::chrono::system_clock::now();
    event.generator_url = "http://localhost:8080/alerts";
    
    // 转换JSON标签为map
    for (const auto& item : labels.items()) {
        event.labels[item.key()] = item.value().get<std::string>();
    }
    
    // 转换JSON注解为map
    for (const auto& item : annotations.items()) {
        event.annotations[item.key()] = item.value().get<std::string>();
    }
    
    return processAlarmEvent(event);
}

bool AlarmManager::resolveAlarm(const std::string& fingerprint) {
    // 检查是否存在该指纹的告警
    if (!alarmEventExists(fingerprint)) {
        logDebug("No alarm found for fingerprint: " + fingerprint);
        return true;
    }
    
    // 创建解决告警事件
    AlarmEvent event;
    event.fingerprint = fingerprint;
    event.status = "resolved";
    event.starts_at = std::chrono::system_clock::now();
    event.ends_at = std::chrono::system_clock::now();
    event.generator_url = "http://localhost:8080/alerts";
    
    // 获取现有告警的标签和注解
    auto existing_events = getAlarmEventsByFingerprint(fingerprint);
    if (!existing_events.empty()) {
        try {
            nlohmann::json labels_json = nlohmann::json::parse(existing_events[0].labels_json);
            nlohmann::json annotations_json = nlohmann::json::parse(existing_events[0].annotations_json);
            
            for (const auto& item : labels_json.items()) {
                event.labels[item.key()] = item.value().get<std::string>();
            }
            
            for (const auto& item : annotations_json.items()) {
                event.annotations[item.key()] = item.value().get<std::string>();
            }
        } catch (const std::exception& e) {
            logError("Failed to parse existing alarm labels/annotations: " + std::string(e.what()));
        }
    }
    
    return processAlarmEvent(event);
}

// 旧的连接管理方法已移除，改为使用连接池

// 优化的解析方法
AlarmEventRecord AlarmManager::parseRowToAlarmEventRecord(MYSQL_ROW row) {
    AlarmEventRecord record;
    if (row) {
        record.id = row[0] ? row[0] : "";
        record.fingerprint = row[1] ? row[1] : "";
        record.status = row[2] ? row[2] : "";
        record.labels_json = row[3] ? row[3] : "";
        record.annotations_json = row[4] ? row[4] : "";
        record.starts_at = row[5] ? row[5] : "";
        record.ends_at = row[6] ? row[6] : "";
        record.generator_url = row[7] ? row[7] : "";
        record.created_at = row[8] ? row[8] : "";
        record.updated_at = row[9] ? row[9] : "";
    }
    return record;
}

// 优化的查询执行方法
std::vector<AlarmEventRecord> AlarmManager::executeSelectAlarmEvents(const std::string& query) {
    std::vector<AlarmEventRecord> events;
    
    MYSQL_RES* raw_result = executeSelectQuery(query);
    if (!raw_result) {
        return events;
    }
    
    MySQLResultRAII result(raw_result);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result.get())) != nullptr) {
        events.push_back(parseRowToAlarmEventRecord(row));
    }
    
    return events;
}

// 错误处理和验证方法实现
bool AlarmManager::validateAlarmEvent(const AlarmEvent& event) {
    if (event.fingerprint.empty()) {
        logError("Invalid alarm event: fingerprint is empty");
        return false;
    }
    
    if (event.status != "firing" && event.status != "resolved") {
        logError("Invalid alarm event: status must be 'firing' or 'resolved', got: " + event.status);
        return false;
    }
    
    return true;
}

bool AlarmManager::validatePaginationParams(int& page, int& page_size) {
    bool modified = false;
    
    if (page < 1) {
        logError("Invalid page number: " + std::to_string(page) + ", setting to 1");
        page = 1;
        modified = true;
    }
    
    if (page_size < 1) {
        logError("Invalid page size: " + std::to_string(page_size) + ", setting to 20");
        page_size = 20;
        modified = true;
    }
    
    if (page_size > 1000) {
        logError("Page size too large: " + std::to_string(page_size) + ", limiting to 1000");
        page_size = 1000;
        modified = true;
    }
    
    return !modified;
}

void AlarmManager::logQueryError(const std::string& query, const std::string& error_msg) {
    logError("Query failed: " + error_msg);
    logError("SQL: " + query);
}

bool AlarmManager::handleMySQLError(const std::string& operation) {
    // 连接池模式下，错误处理由连接池自动管理
    logError(operation + " failed: Connection pool will handle connection issues automatically");
    return false;
}

// 批量操作方法实现
bool AlarmManager::batchInsertAlarmEvents(const std::vector<AlarmEvent>& events) {
    if (events.empty()) {
        logDebug("No events to insert in batch");
        return true;
    }
    
    if (!m_initialized) {
        logError("AlarmManager not initialized for batch insert");
        return false;
    }
    
    // 验证所有事件
    for (const auto& event : events) {
        if (!validateAlarmEvent(event)) {
            return false;
        }
    }
    
    logInfo("Batch inserting " + std::to_string(events.size()) + " alarm events");
    
    // 构建批量插入查询
    std::ostringstream query;
    query << "INSERT INTO alarm_events (id, fingerprint, status, labels_json, annotations_json, "
          << "starts_at, generator_url) VALUES ";
    
    bool first = true;
    for (const auto& event : events) {
        if (!first) {
            query << ", ";
        }
        first = false;
        
        std::string id = generateEventId();
        std::string fingerprint = escapeString(event.fingerprint);
        std::string status = escapeString(event.status);
        
        nlohmann::json labels_json = event.labels;
        std::string labels_str = escapeString(labels_json.dump());
        
        nlohmann::json annotations_json = event.annotations;
        std::string annotations_str = escapeString(annotations_json.dump());
        
        std::string starts_at = formatTimestamp(event.starts_at);
        std::string generator_url = escapeString(event.generator_url);
        
        query << "('" << id << "', '"
              << fingerprint << "', '"
              << status << "', '"
              << labels_str << "', '"
              << annotations_str << "', '"
              << starts_at << "', '"
              << generator_url << "')";
    }
    
    if (!executeQuery(query.str())) {
        logError("Failed to batch insert alarm events");
        return false;
    }
    
    logInfo("Successfully batch inserted " + std::to_string(events.size()) + " alarm events");
    return true;
}

bool AlarmManager::batchUpdateAlarmEventsToResolved(const std::vector<std::string>& fingerprints) {
    if (fingerprints.empty()) {
        logDebug("No fingerprints to update in batch");
        return true;
    }
    
    if (!m_initialized) {
        logError("AlarmManager not initialized for batch update");
        return false;
    }
    
    logInfo("Batch resolving " + std::to_string(fingerprints.size()) + " alarm events");
    
    // 构建批量更新查询
    std::ostringstream query;
    query << "UPDATE alarm_events SET status = 'resolved', ends_at = NOW() WHERE status = 'firing' AND fingerprint IN (";
    
    bool first = true;
    for (const auto& fingerprint : fingerprints) {
        if (!first) {
            query << ", ";
        }
        first = false;
        
        std::string escaped_fingerprint = escapeString(fingerprint);
        query << "'" << escaped_fingerprint << "'";
    }
    
    query << ")";
    
    if (!executeQuery(query.str())) {
        logError("Failed to batch update alarm events to resolved");
        return false;
    }
    
    // 注意：使用连接池时，无法直接获取affected_rows
    // 在实际应用中，可以通过SELECT COUNT查询来验证更新结果
    logInfo("Batch update query executed successfully");
    
    return true;
}