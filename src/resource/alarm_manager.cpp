#include "alarm_manager.h"
#include "alarm_rule_engine.h"
#include "log_manager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <uuid/uuid.h>
#include <cstring>

AlarmManager::AlarmManager(const std::string& host, int port, const std::string& user,
                           const std::string& password, const std::string& database)
    : m_host(host), m_port(port), m_user(user), m_password(password), m_database(database), m_connection(nullptr) {
}

AlarmManager::~AlarmManager() {
    disconnect();
}

bool AlarmManager::connect() {
    if (m_connection) {
        return true;
    }
    
    m_connection = mysql_init(nullptr);
    if (!m_connection) {
        logError("Failed to initialize MySQL connection");
        return false;
    }
    
    // 设置连接选项
    bool reconnect = true;
    mysql_options(m_connection, MYSQL_OPT_RECONNECT, &reconnect);
    
    // 连接到数据库
    if (!mysql_real_connect(m_connection, m_host.c_str(), m_user.c_str(), 
                           m_password.c_str(), m_database.c_str(), m_port, nullptr, 0)) {
        logError("Failed to connect to MySQL: " + std::string(mysql_error(m_connection)));
        mysql_close(m_connection);
        m_connection = nullptr;
        return false;
    }
    
    logInfo("Connected to MySQL server successfully");
    return true;
}

void AlarmManager::disconnect() {
    if (m_connection) {
        mysql_close(m_connection);
        m_connection = nullptr;
        logInfo("Disconnected from MySQL server");
    }
}

bool AlarmManager::createDatabase() {
    if (!m_connection) {
        logError("No database connection");
        return false;
    }
    
    std::string query = "CREATE DATABASE IF NOT EXISTS " + m_database + 
                       " CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci";
    
    if (!executeQuery(query)) {
        logError("Failed to create database: " + m_database);
        return false;
    }
    
    // 选择数据库
    if (mysql_select_db(m_connection, m_database.c_str()) != 0) {
        logError("Failed to select database: " + std::string(mysql_error(m_connection)));
        return false;
    }
    
    logInfo("Database created and selected: " + m_database);
    return true;
}

bool AlarmManager::createEventTable() {
    if (!m_connection) {
        logError("No database connection");
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
    if (!m_connection) {
        logError("No database connection");
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
    
    // 检查是否有行被更新
    if (mysql_affected_rows(m_connection) == 0) {
        logError("No firing alarm found for fingerprint: " + fingerprint);
        return false;
    }
    
    logInfo("Alarm event updated to resolved: " + fingerprint);
    return true;
}

bool AlarmManager::alarmEventExists(const std::string& fingerprint) {
    std::string escaped_fingerprint = escapeString(fingerprint);
    
    std::ostringstream query;
    query << "SELECT COUNT(*) FROM alarm_events WHERE fingerprint = '"
          << escaped_fingerprint << "' AND status = 'firing'";
    
    MYSQL_RES* result = executeSelectQuery(query.str());
    if (!result) {
        return false;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    int count = row ? std::stoi(row[0]) : 0;
    
    mysql_free_result(result);
    return count > 0;
}

std::vector<AlarmEventRecord> AlarmManager::getActiveAlarmEvents() {
    std::vector<AlarmEventRecord> events;
    
    std::string query = "SELECT id, fingerprint, status, labels_json, annotations_json, "
                       "starts_at, ends_at, generator_url, created_at, updated_at "
                       "FROM alarm_events WHERE status = 'firing' ORDER BY starts_at DESC";
    
    MYSQL_RES* result = executeSelectQuery(query);
    if (!result) {
        return events;
    }
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        AlarmEventRecord record;
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
        
        events.push_back(record);
    }
    
    mysql_free_result(result);
    return events;
}

std::vector<AlarmEventRecord> AlarmManager::getAlarmEventsByFingerprint(const std::string& fingerprint) {
    std::vector<AlarmEventRecord> events;
    
    std::string escaped_fingerprint = escapeString(fingerprint);
    
    std::ostringstream query;
    query << "SELECT id, fingerprint, status, labels_json, annotations_json, "
          << "starts_at, ends_at, generator_url, created_at, updated_at "
          << "FROM alarm_events WHERE fingerprint = '"
          << escaped_fingerprint << "' ORDER BY starts_at DESC";
    
    MYSQL_RES* result = executeSelectQuery(query.str());
    if (!result) {
        return events;
    }
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        AlarmEventRecord record;
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
        
        events.push_back(record);
    }
    
    mysql_free_result(result);
    return events;
}

std::vector<AlarmEventRecord> AlarmManager::getRecentAlarmEvents(int limit) {
    std::vector<AlarmEventRecord> events;
    
    std::ostringstream query;
    query << "SELECT id, fingerprint, status, labels_json, annotations_json, "
          << "starts_at, ends_at, generator_url, created_at, updated_at "
          << "FROM alarm_events ORDER BY created_at DESC LIMIT " << limit;
    
    MYSQL_RES* result = executeSelectQuery(query.str());
    if (!result) {
        return events;
    }
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        AlarmEventRecord record;
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
        
        events.push_back(record);
    }
    
    mysql_free_result(result);
    return events;
}

AlarmEventRecord AlarmManager::getAlarmEventById(const std::string& id) {
    AlarmEventRecord record;
    
    std::string escaped_id = escapeString(id);
    
    std::ostringstream query;
    query << "SELECT id, fingerprint, status, labels_json, annotations_json, "
          << "starts_at, ends_at, generator_url, created_at, updated_at "
          << "FROM alarm_events WHERE id = '" << escaped_id << "'";
    
    MYSQL_RES* result = executeSelectQuery(query.str());
    if (!result) {
        return record;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
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
    
    mysql_free_result(result);
    return record;
}

int AlarmManager::getActiveAlarmCount() {
    std::string query = "SELECT COUNT(*) FROM alarm_events WHERE status = 'firing'";
    
    MYSQL_RES* result = executeSelectQuery(query);
    if (!result) {
        return 0;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    int count = row ? std::stoi(row[0]) : 0;
    
    mysql_free_result(result);
    return count;
}

int AlarmManager::getTotalAlarmCount() {
    std::string query = "SELECT COUNT(*) FROM alarm_events";
    
    MYSQL_RES* result = executeSelectQuery(query);
    if (!result) {
        return 0;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    int count = row ? std::stoi(row[0]) : 0;
    
    mysql_free_result(result);
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
    
    // 验证参数
    if (page < 1) page = 1;
    if (page_size < 1) page_size = 20;
    if (page_size > 1000) page_size = 1000; // 限制最大页面大小
    
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
    MYSQL_RES* count_result = executeSelectQuery(count_query.str());
    if (!count_result) {
        return result;
    }
    
    MYSQL_ROW count_row = mysql_fetch_row(count_result);
    if (count_row && count_row[0]) {
        result.total_count = std::stoi(count_row[0]);
    }
    mysql_free_result(count_result);
    
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
    MYSQL_RES* data_result = executeSelectQuery(data_query.str());
    if (!data_result) {
        return result;
    }
    
    // 填充结果数据
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(data_result)) != nullptr) {
        AlarmEventRecord record;
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
        
        result.events.push_back(record);
    }
    
    mysql_free_result(data_result);
    return result;
}

bool AlarmManager::executeQuery(const std::string& query) {
    if (!m_connection) {
        logError("No database connection");
        return false;
    }
    
    logDebug("Executing query: " + query);
    
    if (mysql_query(m_connection, query.c_str()) != 0) {
        logError("Query failed: " + std::string(mysql_error(m_connection)));
        logError("SQL: " + query);
        return false;
    }
    
    return true;
}

MYSQL_RES* AlarmManager::executeSelectQuery(const std::string& query) {
    if (!executeQuery(query)) {
        return nullptr;
    }
    
    MYSQL_RES* result = mysql_store_result(m_connection);
    if (!result) {
        logError("Failed to store result: " + std::string(mysql_error(m_connection)));
        return nullptr;
    }
    
    return result;
}

std::string AlarmManager::escapeString(const std::string& str) {
    if (!m_connection) {
        return str;
    }
    
    char* escaped = new char[str.length() * 2 + 1];
    mysql_real_escape_string(m_connection, escaped, str.c_str(), str.length());
    
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