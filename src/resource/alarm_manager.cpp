#include "../../include/resource/alarm_manager.h"
#include "../../include/resource/alarm_rule_engine.h"
#include "../../include/resource/log_manager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <uuid/uuid.h>
#include <cstring>
#include <algorithm>
#include <vector>

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

// // 新的连接池构造函数
// AlarmManager::AlarmManager(const MySQLPoolConfig& pool_config)
//     : m_pool_config(pool_config), m_initialized(false), m_owns_connection_pool(true) {
//     m_connection_pool = std::make_shared<MySQLConnectionPool>(m_pool_config);
// }

// // 兼容性构造函数 - 将旧参数转换为连接池配置
// AlarmManager::AlarmManager(const std::string& host, int port, const std::string& user,
//                            const std::string& password, const std::string& database)
//     : m_initialized(false), m_owns_connection_pool(true) {
//     m_pool_config = createDefaultPoolConfig();
//     m_pool_config.host = host;
//     m_pool_config.port = port;
//     m_pool_config.user = user;
//     m_pool_config.password = password;
//     m_pool_config.database = database;
    
//     m_connection_pool = std::make_shared<MySQLConnectionPool>(m_pool_config);
// }

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
    if (!m_initialized) {
        logError("AlarmManager not initialized");
        return false;
    }
    MySQLConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) {
        logError("Failed to get database connection from pool");
        return false;
    }

    std::string id = generateEventId();
    // 原始字符串，无需再转义，交由参数绑定
    std::string fingerprint = event.fingerprint;
    std::string status = event.status;
    nlohmann::json labels_json = event.labels;
    std::string labels_str = labels_json.dump();
    nlohmann::json annotations_json = event.annotations;
    std::string annotations_str = annotations_json.dump();
    std::string generator_url = event.generator_url;

    const char* sql = "INSERT INTO alarm_events (id, fingerprint, status, labels_json, annotations_json, starts_at, generator_url) VALUES (?, ?, ?, ?, ?, NOW(), ?)";
    MYSQL* mysql = guard->get();
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        logError("Failed to init statement for insertAlarmEvent");
        return false;
    }
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(strlen(sql))) != 0) {
        logQueryError(sql, mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    unsigned long id_len = static_cast<unsigned long>(id.size());
    unsigned long fp_len = static_cast<unsigned long>(fingerprint.size());
    unsigned long st_len = static_cast<unsigned long>(status.size());
    unsigned long labels_len = static_cast<unsigned long>(labels_str.size());
    unsigned long ann_len = static_cast<unsigned long>(annotations_str.size());
    unsigned long url_len = static_cast<unsigned long>(generator_url.size());

    MYSQL_BIND binds[6]{};
    memset(binds, 0, sizeof(binds));
    // id
    binds[0].buffer_type = MYSQL_TYPE_STRING;
    binds[0].buffer = const_cast<char*>(id.c_str());
    binds[0].buffer_length = id_len;
    binds[0].length = &id_len;
    // fingerprint
    binds[1].buffer_type = MYSQL_TYPE_STRING;
    binds[1].buffer = const_cast<char*>(fingerprint.c_str());
    binds[1].buffer_length = fp_len;
    binds[1].length = &fp_len;
    // status
    binds[2].buffer_type = MYSQL_TYPE_STRING;
    binds[2].buffer = const_cast<char*>(status.c_str());
    binds[2].buffer_length = st_len;
    binds[2].length = &st_len;
    // labels_json
    binds[3].buffer_type = MYSQL_TYPE_STRING;
    binds[3].buffer = const_cast<char*>(labels_str.c_str());
    binds[3].buffer_length = labels_len;
    binds[3].length = &labels_len;
    // annotations_json
    binds[4].buffer_type = MYSQL_TYPE_STRING;
    binds[4].buffer = const_cast<char*>(annotations_str.c_str());
    binds[4].buffer_length = ann_len;
    binds[4].length = &ann_len;
    // generator_url
    binds[5].buffer_type = MYSQL_TYPE_STRING;
    binds[5].buffer = const_cast<char*>(generator_url.c_str());
    binds[5].buffer_length = url_len;
    binds[5].length = &url_len;

    if (mysql_stmt_bind_param(stmt, binds) != 0) {
        logQueryError("bind params (insertAlarmEvent)", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    if (mysql_stmt_execute(stmt) != 0) {
        logQueryError("execute (insertAlarmEvent)", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    mysql_stmt_close(stmt);
    logInfo("Alarm event inserted successfully: " + event.fingerprint);
    return true;
}

bool AlarmManager::updateAlarmEventToResolved(const std::string& fingerprint, const AlarmEvent& /*event*/) {
    if (!m_initialized) {
        logError("AlarmManager not initialized");
        return false;
    }
    MySQLConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) {
        logError("Failed to get database connection from pool");
        return false;
    }
    const char* sql = "UPDATE alarm_events SET status = 'resolved', ends_at = NOW() WHERE fingerprint = ? AND status = 'firing'";
    MYSQL* mysql = guard->get();
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        logError("Failed to init statement for updateAlarmEventToResolved");
        return false;
    }
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(strlen(sql))) != 0) {
        logQueryError(sql, mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    std::string fp = fingerprint;
    unsigned long fp_len = static_cast<unsigned long>(fp.size());
    MYSQL_BIND bind{};
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = const_cast<char*>(fp.c_str());
    bind.buffer_length = fp_len;
    bind.length = &fp_len;
    if (mysql_stmt_bind_param(stmt, &bind) != 0) {
        logQueryError("bind params (updateAlarmEventToResolved)", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    if (mysql_stmt_execute(stmt) != 0) {
        logQueryError("execute (updateAlarmEventToResolved)", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    my_ulonglong affected = mysql_stmt_affected_rows(stmt);
    mysql_stmt_close(stmt);
    logInfo("Alarm event updated to resolved: " + fingerprint + ", affected_rows=" + std::to_string(affected));
    return affected > 0;
}

bool AlarmManager::alarmEventExists(const std::string& fingerprint) {
    if (!m_initialized) {
        logError("AlarmManager not initialized");
        return false;
    }
    MySQLConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) {
        logError("Failed to get database connection from pool");
        return false;
    }
    const char* sql = "SELECT COUNT(*) FROM alarm_events WHERE fingerprint = ? AND status = 'firing'";
    MYSQL* mysql = guard->get();
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        logError("Failed to init statement for alarmEventExists");
        return false;
    }
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(strlen(sql))) != 0) {
        logQueryError(sql, mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    std::string fp = fingerprint;
    unsigned long fp_len = static_cast<unsigned long>(fp.size());
    MYSQL_BIND param{};
    memset(&param, 0, sizeof(param));
    param.buffer_type = MYSQL_TYPE_STRING;
    param.buffer = const_cast<char*>(fp.c_str());
    param.buffer_length = fp_len;
    param.length = &fp_len;
    if (mysql_stmt_bind_param(stmt, &param) != 0) {
        logQueryError("bind params (alarmEventExists)", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    if (mysql_stmt_execute(stmt) != 0) {
        logQueryError("execute (alarmEventExists)", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    int count = 0;
    MYSQL_BIND result_bind{};
    memset(&result_bind, 0, sizeof(result_bind));
    result_bind.buffer_type = MYSQL_TYPE_LONG;
    result_bind.buffer = &count;
    unsigned long len = 0;
    result_bind.length = &len;
    if (mysql_stmt_bind_result(stmt, &result_bind) != 0) {
        logQueryError("bind result (alarmEventExists)", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    if (mysql_stmt_fetch(stmt) != 0) {
        logQueryError("fetch (alarmEventExists)", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }
    mysql_stmt_close(stmt);
    return count > 0;
}

std::vector<AlarmEventRecord> AlarmManager::getActiveAlarmEvents() {
    std::vector<AlarmEventRecord> events;
    if (!m_initialized) return events;
    MySQLConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) return events;
    const char* sql = "SELECT id, fingerprint, status, labels_json, annotations_json, starts_at, ends_at, generator_url, created_at, updated_at FROM alarm_events WHERE status = 'firing' ORDER BY starts_at DESC";
    MYSQL* mysql = guard->get();
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) return events;
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        return events;
    }
    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return events;
    }
    if (mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return events;
    }
    // 绑定结果缓冲
    const size_t COLS = 10;
    std::vector<char> id_buf(64), fp_buf(600), status_buf(16), labels_buf(65535), ann_buf(65535), starts_at_buf(32), ends_at_buf(32), gen_url_buf(2048), created_buf(32), updated_buf(32);
    unsigned long id_len=0, fp_len=0, status_len=0, labels_len=0, ann_len=0, starts_at_len=0, ends_at_len=0, gen_url_len=0, created_len=0, updated_len=0;
    MYSQL_BIND result[COLS]{};
    memset(result, 0, sizeof(result));
    auto bind_str = [&](int idx, char* buf, unsigned long buf_len, unsigned long* out_len){ result[idx].buffer_type = MYSQL_TYPE_STRING; result[idx].buffer = buf; result[idx].buffer_length = buf_len; result[idx].length = out_len; };
    bind_str(0, id_buf.data(), id_buf.size(), &id_len);
    bind_str(1, fp_buf.data(), fp_buf.size(), &fp_len);
    bind_str(2, status_buf.data(), status_buf.size(), &status_len);
    bind_str(3, labels_buf.data(), labels_buf.size(), &labels_len);
    bind_str(4, ann_buf.data(), ann_buf.size(), &ann_len);
    bind_str(5, starts_at_buf.data(), starts_at_buf.size(), &starts_at_len);
    bind_str(6, ends_at_buf.data(), ends_at_buf.size(), &ends_at_len);
    bind_str(7, gen_url_buf.data(), gen_url_buf.size(), &gen_url_len);
    bind_str(8, created_buf.data(), created_buf.size(), &created_len);
    bind_str(9, updated_buf.data(), updated_buf.size(), &updated_len);
    if (mysql_stmt_bind_result(stmt, result) != 0) {
        mysql_stmt_close(stmt);
        return events;
    }
    while (true) {
        int fetch_status = mysql_stmt_fetch(stmt);
        if (fetch_status == MYSQL_NO_DATA) break;
        if (fetch_status == 1 /* error */) break;
        AlarmEventRecord rec;
        rec.id.assign(id_buf.data(), id_len);
        rec.fingerprint.assign(fp_buf.data(), fp_len);
        rec.status.assign(status_buf.data(), status_len);
        rec.labels_json.assign(labels_buf.data(), labels_len);
        rec.annotations_json.assign(ann_buf.data(), ann_len);
        rec.starts_at.assign(starts_at_buf.data(), starts_at_len);
        rec.ends_at.assign(ends_at_buf.data(), ends_at_len);
        rec.generator_url.assign(gen_url_buf.data(), gen_url_len);
        rec.created_at.assign(created_buf.data(), created_len);
        rec.updated_at.assign(updated_buf.data(), updated_len);
        events.push_back(std::move(rec));
    }
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return events;
}

// getAlarmEventsByFingerprint 已不再使用，移除实现

std::vector<AlarmEventRecord> AlarmManager::getRecentAlarmEvents(int limit) {
    std::vector<AlarmEventRecord> events;
    if (!m_initialized) return events;
    MySQLConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) return events;
    const char* sql = "SELECT id, fingerprint, status, labels_json, annotations_json, starts_at, ends_at, generator_url, created_at, updated_at FROM alarm_events ORDER BY created_at DESC LIMIT ?";
    MYSQL* mysql = guard->get();
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) return events;
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(strlen(sql))) != 0) { mysql_stmt_close(stmt); return events; }
    int limit_val = limit;
    unsigned long limit_len = sizeof(limit_val);
    MYSQL_BIND param{}; memset(&param, 0, sizeof(param)); param.buffer_type = MYSQL_TYPE_LONG; param.buffer = &limit_val; param.length = &limit_len;
    if (mysql_stmt_bind_param(stmt, &param) != 0) { mysql_stmt_close(stmt); return events; }
    if (mysql_stmt_execute(stmt) != 0) { mysql_stmt_close(stmt); return events; }
    if (mysql_stmt_store_result(stmt) != 0) { mysql_stmt_close(stmt); return events; }
    // 绑定结果同上
    std::vector<char> id_buf(64), fp_buf(600), status_buf(16), labels_buf(65535), ann_buf(65535), starts_at_buf(32), ends_at_buf(32), gen_url_buf(2048), created_buf(32), updated_buf(32);
    unsigned long id_len=0, fp_len=0, status_len=0, labels_len=0, ann_len=0, starts_at_len=0, ends_at_len=0, gen_url_len=0, created_len=0, updated_len=0;
    MYSQL_BIND result[10]{}; memset(result, 0, sizeof(result));
    auto bind_str = [&](int idx, char* buf, unsigned long buf_len, unsigned long* out_len){ result[idx].buffer_type = MYSQL_TYPE_STRING; result[idx].buffer = buf; result[idx].buffer_length = buf_len; result[idx].length = out_len; };
    bind_str(0, id_buf.data(), id_buf.size(), &id_len);
    bind_str(1, fp_buf.data(), fp_buf.size(), &fp_len);
    bind_str(2, status_buf.data(), status_buf.size(), &status_len);
    bind_str(3, labels_buf.data(), labels_buf.size(), &labels_len);
    bind_str(4, ann_buf.data(), ann_buf.size(), &ann_len);
    bind_str(5, starts_at_buf.data(), starts_at_buf.size(), &starts_at_len);
    bind_str(6, ends_at_buf.data(), ends_at_buf.size(), &ends_at_len);
    bind_str(7, gen_url_buf.data(), gen_url_buf.size(), &gen_url_len);
    bind_str(8, created_buf.data(), created_buf.size(), &created_len);
    bind_str(9, updated_buf.data(), updated_buf.size(), &updated_len);
    if (mysql_stmt_bind_result(stmt, result) != 0) { mysql_stmt_close(stmt); return events; }
    while (true) {
        int fetch_status = mysql_stmt_fetch(stmt);
        if (fetch_status == MYSQL_NO_DATA) break;
        if (fetch_status == 1) break;
        AlarmEventRecord rec;
        rec.id.assign(id_buf.data(), id_len);
        rec.fingerprint.assign(fp_buf.data(), fp_len);
        rec.status.assign(status_buf.data(), status_len);
        rec.labels_json.assign(labels_buf.data(), labels_len);
        rec.annotations_json.assign(ann_buf.data(), ann_len);
        rec.starts_at.assign(starts_at_buf.data(), starts_at_len);
        rec.ends_at.assign(ends_at_buf.data(), ends_at_len);
        rec.generator_url.assign(gen_url_buf.data(), gen_url_len);
        rec.created_at.assign(created_buf.data(), created_len);
        rec.updated_at.assign(updated_buf.data(), updated_len);
        events.push_back(std::move(rec));
    }
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return events;
}

AlarmEventRecord AlarmManager::getAlarmEventById(const std::string& id) {
    AlarmEventRecord record;
    if (!m_initialized) return record;
    MySQLConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) return record;
    const char* sql = "SELECT id, fingerprint, status, labels_json, annotations_json, starts_at, ends_at, generator_url, created_at, updated_at FROM alarm_events WHERE id = ?";
    MYSQL* mysql = guard->get();
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) return record;
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(strlen(sql))) != 0) { mysql_stmt_close(stmt); return record; }
    std::string id_copy = id; unsigned long id_len = static_cast<unsigned long>(id_copy.size());
    MYSQL_BIND param{}; memset(&param, 0, sizeof(param)); param.buffer_type = MYSQL_TYPE_STRING; param.buffer = const_cast<char*>(id_copy.c_str()); param.length = &id_len; param.buffer_length = id_len;
    if (mysql_stmt_bind_param(stmt, &param) != 0) { mysql_stmt_close(stmt); return record; }
    if (mysql_stmt_execute(stmt) != 0) { mysql_stmt_close(stmt); return record; }
    if (mysql_stmt_store_result(stmt) != 0) { mysql_stmt_close(stmt); return record; }
    std::vector<char> id_buf(64), fp_buf(600), status_buf(16), labels_buf(65535), ann_buf(65535), starts_at_buf(32), ends_at_buf(32), gen_url_buf(2048), created_buf(32), updated_buf(32);
    unsigned long id_len2=0, fp_len=0, status_len=0, labels_len=0, ann_len=0, starts_at_len=0, ends_at_len=0, gen_url_len=0, created_len=0, updated_len=0;
    MYSQL_BIND result_binds[10]{}; memset(result_binds, 0, sizeof(result_binds));
    auto bind_str = [&](int idx, char* buf, unsigned long buf_len, unsigned long* out_len){ result_binds[idx].buffer_type = MYSQL_TYPE_STRING; result_binds[idx].buffer = buf; result_binds[idx].buffer_length = buf_len; result_binds[idx].length = out_len; };
    bind_str(0, id_buf.data(), id_buf.size(), &id_len2);
    bind_str(1, fp_buf.data(), fp_buf.size(), &fp_len);
    bind_str(2, status_buf.data(), status_buf.size(), &status_len);
    bind_str(3, labels_buf.data(), labels_buf.size(), &labels_len);
    bind_str(4, ann_buf.data(), ann_buf.size(), &ann_len);
    bind_str(5, starts_at_buf.data(), starts_at_buf.size(), &starts_at_len);
    bind_str(6, ends_at_buf.data(), ends_at_buf.size(), &ends_at_len);
    bind_str(7, gen_url_buf.data(), gen_url_buf.size(), &gen_url_len);
    bind_str(8, created_buf.data(), created_buf.size(), &created_len);
    bind_str(9, updated_buf.data(), updated_buf.size(), &updated_len);
    if (mysql_stmt_bind_result(stmt, result_binds) != 0) { mysql_stmt_close(stmt); return record; }
    if (mysql_stmt_fetch(stmt) == 0) {
        record.id.assign(id_buf.data(), id_len2);
        record.fingerprint.assign(fp_buf.data(), fp_len);
        record.status.assign(status_buf.data(), status_len);
        record.labels_json.assign(labels_buf.data(), labels_len);
        record.annotations_json.assign(ann_buf.data(), ann_len);
        record.starts_at.assign(starts_at_buf.data(), starts_at_len);
        record.ends_at.assign(ends_at_buf.data(), ends_at_len);
        record.generator_url.assign(gen_url_buf.data(), gen_url_len);
        record.created_at.assign(created_buf.data(), created_len);
        record.updated_at.assign(updated_buf.data(), updated_len);
    }
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return record;
}

int AlarmManager::getActiveAlarmCount() {
    if (!m_initialized) return 0;
    MySQLConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) return 0;
    const char* sql = "SELECT COUNT(*) FROM alarm_events WHERE status = 'firing'";
    MYSQL* mysql = guard->get();
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) return 0;
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(strlen(sql))) != 0) { mysql_stmt_close(stmt); return 0; }
    if (mysql_stmt_execute(stmt) != 0) { mysql_stmt_close(stmt); return 0; }
    int count = 0; unsigned long len=0; MYSQL_BIND rb{}; memset(&rb, 0, sizeof(rb)); rb.buffer_type = MYSQL_TYPE_LONG; rb.buffer = &count; rb.length = &len;
    if (mysql_stmt_bind_result(stmt, &rb) != 0) { mysql_stmt_close(stmt); return 0; }
    if (mysql_stmt_fetch(stmt) != 0) { mysql_stmt_close(stmt); return 0; }
    mysql_stmt_close(stmt);
    return count;
}

int AlarmManager::getTotalAlarmCount() {
    if (!m_initialized) return 0;
    MySQLConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) return 0;
    const char* sql = "SELECT COUNT(*) FROM alarm_events";
    MYSQL* mysql = guard->get();
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) return 0;
    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(strlen(sql))) != 0) { mysql_stmt_close(stmt); return 0; }
    if (mysql_stmt_execute(stmt) != 0) { mysql_stmt_close(stmt); return 0; }
    int count = 0; unsigned long len=0; MYSQL_BIND rb{}; memset(&rb, 0, sizeof(rb)); rb.buffer_type = MYSQL_TYPE_LONG; rb.buffer = &count; rb.length = &len;
    if (mysql_stmt_bind_result(stmt, &rb) != 0) { mysql_stmt_close(stmt); return 0; }
    if (mysql_stmt_fetch(stmt) != 0) { mysql_stmt_close(stmt); return 0; }
    mysql_stmt_close(stmt);
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
    if (!m_initialized) return result;
    MySQLConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) return result;
    MYSQL* mysql = guard->get();
    // COUNT 查询（根据是否有status准备不同SQL）
    MYSQL_STMT* count_stmt = mysql_stmt_init(mysql);
    if (!count_stmt) return result;
    std::string count_sql = status.empty() ?
        "SELECT COUNT(*) FROM alarm_events" :
        "SELECT COUNT(*) FROM alarm_events WHERE status = ?";
    if (mysql_stmt_prepare(count_stmt, count_sql.c_str(), static_cast<unsigned long>(count_sql.size())) != 0) {
        mysql_stmt_close(count_stmt);
        return result;
    }
    MYSQL_BIND count_param{}; unsigned long status_len=0; std::string status_copy;
    if (!status.empty()) {
        memset(&count_param, 0, sizeof(count_param));
        status_copy = status; status_len = static_cast<unsigned long>(status_copy.size());
        count_param.buffer_type = MYSQL_TYPE_STRING;
        count_param.buffer = const_cast<char*>(status_copy.c_str());
        count_param.buffer_length = status_len;
        count_param.length = &status_len;
        if (mysql_stmt_bind_param(count_stmt, &count_param) != 0) {
            mysql_stmt_close(count_stmt);
            return result;
        }
    }
    if (mysql_stmt_execute(count_stmt) != 0) { mysql_stmt_close(count_stmt); return result; }
    int total_count = 0; unsigned long len=0; MYSQL_BIND rb{}; memset(&rb, 0, sizeof(rb)); rb.buffer_type = MYSQL_TYPE_LONG; rb.buffer = &total_count; rb.length = &len;
    if (mysql_stmt_bind_result(count_stmt, &rb) != 0) { mysql_stmt_close(count_stmt); return result; }
    if (mysql_stmt_fetch(count_stmt) == 0) { result.total_count = total_count; }
    mysql_stmt_close(count_stmt);
    
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
    
    // 数据查询（prepared）
    MYSQL_STMT* data_stmt = mysql_stmt_init(mysql);
    if (!data_stmt) return result;
    std::string data_sql = status.empty() ?
        "SELECT id, fingerprint, status, labels_json, annotations_json, starts_at, ends_at, generator_url, created_at, updated_at FROM alarm_events ORDER BY created_at DESC LIMIT ? OFFSET ?" :
        "SELECT id, fingerprint, status, labels_json, annotations_json, starts_at, ends_at, generator_url, created_at, updated_at FROM alarm_events WHERE status = ? ORDER BY created_at DESC LIMIT ? OFFSET ?";
    if (mysql_stmt_prepare(data_stmt, data_sql.c_str(), static_cast<unsigned long>(data_sql.size())) != 0) { mysql_stmt_close(data_stmt); return result; }
    // 绑定参数
    MYSQL_BIND params[3]{}; memset(params, 0, sizeof(params));
    int param_count = status.empty() ? 2 : 3;
    int limit_val = page_size; unsigned long limit_len = sizeof(limit_val);
    int offset_val = offset; unsigned long offset_len = sizeof(offset_val);
    int idx = 0;
    if (!status.empty()) {
        params[idx].buffer_type = MYSQL_TYPE_STRING; params[idx].buffer = const_cast<char*>(status_copy.c_str()); params[idx].buffer_length = status_len; params[idx].length = &status_len; idx++;
    }
    params[idx].buffer_type = MYSQL_TYPE_LONG; params[idx].buffer = &limit_val; params[idx].length = &limit_len; idx++;
    params[idx].buffer_type = MYSQL_TYPE_LONG; params[idx].buffer = &offset_val; params[idx].length = &offset_len;
    if (mysql_stmt_bind_param(data_stmt, params) != 0) { mysql_stmt_close(data_stmt); return result; }
    if (mysql_stmt_execute(data_stmt) != 0) { mysql_stmt_close(data_stmt); return result; }
    if (mysql_stmt_store_result(data_stmt) != 0) { mysql_stmt_close(data_stmt); return result; }
    // 绑定结果缓冲
    std::vector<char> id_buf(64), fp_buf(600), status_buf(16), labels_buf(65535), ann_buf(65535), starts_at_buf(32), ends_at_buf(32), gen_url_buf(2048), created_buf(32), updated_buf(32);
    unsigned long id_len2=0, fp_len=0, status_len2=0, labels_len=0, ann_len=0, starts_at_len=0, ends_at_len=0, gen_url_len=0, created_len=0, updated_len=0;
    MYSQL_BIND result_binds[10]{}; memset(result_binds, 0, sizeof(result_binds));
    auto bind_str2 = [&](int bidx, char* buf, unsigned long buf_len, unsigned long* out_len){ result_binds[bidx].buffer_type = MYSQL_TYPE_STRING; result_binds[bidx].buffer = buf; result_binds[bidx].buffer_length = buf_len; result_binds[bidx].length = out_len; };
    bind_str2(0, id_buf.data(), id_buf.size(), &id_len2);
    bind_str2(1, fp_buf.data(), fp_buf.size(), &fp_len);
    bind_str2(2, status_buf.data(), status_buf.size(), &status_len2);
    bind_str2(3, labels_buf.data(), labels_buf.size(), &labels_len);
    bind_str2(4, ann_buf.data(), ann_buf.size(), &ann_len);
    bind_str2(5, starts_at_buf.data(), starts_at_buf.size(), &starts_at_len);
    bind_str2(6, ends_at_buf.data(), ends_at_buf.size(), &ends_at_len);
    bind_str2(7, gen_url_buf.data(), gen_url_buf.size(), &gen_url_len);
    bind_str2(8, created_buf.data(), created_buf.size(), &created_len);
    bind_str2(9, updated_buf.data(), updated_buf.size(), &updated_len);
    if (mysql_stmt_bind_result(data_stmt, result_binds) != 0) { mysql_stmt_close(data_stmt); return result; }
    while (true) {
        int fetch_status = mysql_stmt_fetch(data_stmt);
        if (fetch_status == MYSQL_NO_DATA) break;
        if (fetch_status == 1) break;
        AlarmEventRecord rec;
        rec.id.assign(id_buf.data(), id_len2);
        rec.fingerprint.assign(fp_buf.data(), fp_len);
        rec.status.assign(status_buf.data(), status_len2);
        rec.labels_json.assign(labels_buf.data(), labels_len);
        rec.annotations_json.assign(ann_buf.data(), ann_len);
        rec.starts_at.assign(starts_at_buf.data(), starts_at_len);
        rec.ends_at.assign(ends_at_buf.data(), ends_at_len);
        rec.generator_url.assign(gen_url_buf.data(), gen_url_len);
        rec.created_at.assign(created_buf.data(), created_len);
        rec.updated_at.assign(updated_buf.data(), updated_len);
        result.events.push_back(std::move(rec));
    }
    mysql_stmt_free_result(data_stmt);
    mysql_stmt_close(data_stmt);
    
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

// 旧 executeSelectQuery 已废弃（已改为PreparedStatement路径）

// std::string AlarmManager::escapeString(const std::string& str) {
//     if (!m_initialized) {
//         // 如果未初始化，返回简单转义的字符串
//         std::string escaped = str;
//         size_t pos = 0;
//         while ((pos = escaped.find("'", pos)) != std::string::npos) {
//             escaped.replace(pos, 1, "\\'");
//             pos += 2;
//         }
//         return escaped;
//     }
    
//     MySQLConnectionGuard guard(m_connection_pool);
//     if (!guard.isValid()) {
//         // 如果无法获取连接，使用简单转义
//         std::string escaped = str;
//         size_t pos = 0;
//         while ((pos = escaped.find("'", pos)) != std::string::npos) {
//             escaped.replace(pos, 1, "\\'");
//             pos += 2;
//         }
//         return escaped;
//     }
    
//     return escapeStringWithConnection(str, guard.get());
// }

// std::string AlarmManager::escapeStringWithConnection(const std::string& str, MySQLConnection* connection) {
//     if (!connection || !connection->get()) {
//         // 如果没有提供连接，返回简单转义的字符串
//         std::string escaped = str;
//         size_t pos = 0;
//         while ((pos = escaped.find("'", pos)) != std::string::npos) {
//             escaped.replace(pos, 1, "\\'");
//             pos += 2;
//         }
//         return escaped;
//     }
    
//     MYSQL* mysql = connection->get();
//     char* escaped = new char[str.length() * 2 + 1];
//     mysql_real_escape_string(mysql, escaped, str.c_str(), str.length());
    
//     std::string result(escaped);
//     delete[] escaped;
    
//     return result;
// }

std::string AlarmManager::generateEventId() {
    uuid_t uuid;
    uuid_generate(uuid);
    
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);
    
    return std::string(uuid_str);
}

// 新增的连接池相关方法
// MySQLConnectionPool::PoolStats AlarmManager::getConnectionPoolStats() const {
//     if (!m_connection_pool) {
//         return MySQLConnectionPool::PoolStats{};
//     }
//     return m_connection_pool->getStats();
// }

// void AlarmManager::updateConnectionPoolConfig(const MySQLPoolConfig& config) {
//     m_pool_config = config;
//     // 注意：实际应用中，这里可能需要重新创建连接池
//     logInfo("Connection pool configuration updated");
// }

// MySQLPoolConfig AlarmManager::createDefaultPoolConfig() const {
//     MySQLPoolConfig config;
//     config.host = "localhost";
//     config.port = 3306;
//     config.user = "root";
//     config.password = "";
//     config.database = "";
//     config.charset = "utf8mb4";
    
//     // 连接池配置
//     config.min_connections = 3;
//     config.max_connections = 10;
//     config.initial_connections = 5;
    
//     // 超时配置
//     config.connection_timeout = 30;
//     config.idle_timeout = 600;      // 10分钟
//     config.max_lifetime = 3600;     // 1小时
//     config.acquire_timeout = 10;
    
//     // 健康检查配置
//     config.health_check_interval = 60;
//     config.health_check_query = "SELECT 1";
    
//     return config;
// }

// std::string AlarmManager::getCurrentTimestamp() {
//     auto now = std::chrono::system_clock::now();
//     return formatTimestamp(now);
// }

// std::string AlarmManager::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
//     // 输出UTC时间，避免手工时区偏移
//     auto time_t = std::chrono::system_clock::to_time_t(tp);
//     std::ostringstream oss;
//     oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
//     return oss.str();
// }

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

// 旧API createOrUpdateAlarm/resolveAlarm 已由直接提交 AlarmEvent 取代

// 旧的连接管理方法已移除，改为使用连接池

// 旧基于字符串SQL读取的辅助已移除，保留PreparedStatement实现

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

// // 批量操作方法实现
// bool AlarmManager::batchInsertAlarmEvents(const std::vector<AlarmEvent>& events) {
//     if (events.empty()) {
//         logDebug("No events to insert in batch");
//         return true;
//     }
    
//     if (!m_initialized) {
//         logError("AlarmManager not initialized for batch insert");
//         return false;
//     }
    
//     // 验证所有事件
//     for (const auto& event : events) {
//         if (!validateAlarmEvent(event)) {
//             return false;
//         }
//     }
//     logInfo("Batch inserting " + std::to_string(events.size()) + " alarm events");
//     MySQLConnectionGuard guard(m_connection_pool);
//     if (!guard.isValid()) {
//         logError("Failed to get database connection from pool");
//         return false;
//     }
//     const char* sql = "INSERT INTO alarm_events (id, fingerprint, status, labels_json, annotations_json, starts_at, generator_url) VALUES (?, ?, ?, ?, ?, NOW(), ?)";
//     MYSQL* mysql = guard->get();
//     MYSQL_STMT* stmt = mysql_stmt_init(mysql);
//     if (!stmt) return false;
//     if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(strlen(sql))) != 0) { mysql_stmt_close(stmt); return false; }
//     for (const auto& ev : events) {
//         std::string id = generateEventId();
//         std::string fingerprint = ev.fingerprint;
//         std::string status = ev.status;
//         std::string labels_str = nlohmann::json(ev.labels).dump();
//         std::string annotations_str = nlohmann::json(ev.annotations).dump();
//         std::string generator_url = ev.generator_url;
//         unsigned long id_len = id.size(), fp_len = fingerprint.size(), st_len = status.size(), labels_len = labels_str.size(), ann_len = annotations_str.size(), url_len = generator_url.size();
//         MYSQL_BIND binds[6]{}; memset(binds, 0, sizeof(binds));
//         binds[0].buffer_type = MYSQL_TYPE_STRING; binds[0].buffer = const_cast<char*>(id.c_str()); binds[0].buffer_length = id_len; binds[0].length = &id_len;
//         binds[1].buffer_type = MYSQL_TYPE_STRING; binds[1].buffer = const_cast<char*>(fingerprint.c_str()); binds[1].buffer_length = fp_len; binds[1].length = &fp_len;
//         binds[2].buffer_type = MYSQL_TYPE_STRING; binds[2].buffer = const_cast<char*>(status.c_str()); binds[2].buffer_length = st_len; binds[2].length = &st_len;
//         binds[3].buffer_type = MYSQL_TYPE_STRING; binds[3].buffer = const_cast<char*>(labels_str.c_str()); binds[3].buffer_length = labels_len; binds[3].length = &labels_len;
//         binds[4].buffer_type = MYSQL_TYPE_STRING; binds[4].buffer = const_cast<char*>(annotations_str.c_str()); binds[4].buffer_length = ann_len; binds[4].length = &ann_len;
//         binds[5].buffer_type = MYSQL_TYPE_STRING; binds[5].buffer = const_cast<char*>(generator_url.c_str()); binds[5].buffer_length = url_len; binds[5].length = &url_len;
//         if (mysql_stmt_bind_param(stmt, binds) != 0) { mysql_stmt_close(stmt); return false; }
//         if (mysql_stmt_execute(stmt) != 0) { mysql_stmt_close(stmt); return false; }
//     }
//     mysql_stmt_close(stmt);
//     logInfo("Successfully batch inserted " + std::to_string(events.size()) + " alarm events");
//     return true;
// }

// bool AlarmManager::batchUpdateAlarmEventsToResolved(const std::vector<std::string>& fingerprints) {
//     if (fingerprints.empty()) {
//         logDebug("No fingerprints to update in batch");
//         return true;
//     }
    
//     if (!m_initialized) {
//         logError("AlarmManager not initialized for batch update");
//         return false;
//     }
    
//     logInfo("Batch resolving " + std::to_string(fingerprints.size()) + " alarm events");
//     MySQLConnectionGuard guard(m_connection_pool);
//     if (!guard.isValid()) { logError("Failed to get database connection from pool"); return false; }
//     const char* sql = "UPDATE alarm_events SET status = 'resolved', ends_at = NOW() WHERE status = 'firing' AND fingerprint = ?";
//     MYSQL* mysql = guard->get();
//     MYSQL_STMT* stmt = mysql_stmt_init(mysql);
//     if (!stmt) return false;
//     if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(strlen(sql))) != 0) { mysql_stmt_close(stmt); return false; }
//     my_ulonglong total_affected = 0;
//     for (const auto& fp_src : fingerprints) {
//         std::string fp = fp_src; unsigned long fp_len = static_cast<unsigned long>(fp.size());
//         MYSQL_BIND param{}; memset(&param, 0, sizeof(param)); param.buffer_type = MYSQL_TYPE_STRING; param.buffer = const_cast<char*>(fp.c_str()); param.buffer_length = fp_len; param.length = &fp_len;
//         if (mysql_stmt_bind_param(stmt, &param) != 0) { mysql_stmt_close(stmt); return false; }
//         if (mysql_stmt_execute(stmt) != 0) { mysql_stmt_close(stmt); return false; }
//         total_affected += mysql_stmt_affected_rows(stmt);
//     }
//     mysql_stmt_close(stmt);
//     logInfo("Batch update executed, affected_rows=" + std::to_string(total_affected));
//     return true;
// }