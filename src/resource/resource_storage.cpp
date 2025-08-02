#include "resource_storage.h"
#include "log_manager.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cctype>

namespace {
    // Helper function to clean strings for use as table names
    std::string cleanForTableName(const std::string& input) {
        std::string cleaned = input;
        std::replace(cleaned.begin(), cleaned.end(), '/', '_');
        std::replace(cleaned.begin(), cleaned.end(), '-', '_');
        std::replace(cleaned.begin(), cleaned.end(), '.', '_');
        std::replace(cleaned.begin(), cleaned.end(), ':', '_');
        std::replace(cleaned.begin(), cleaned.end(), ' ', '_');
        return cleaned;
    }
}

// 连接池注入构造函数 - 推荐使用
ResourceStorage::ResourceStorage(std::shared_ptr<TDengineConnectionPool> connection_pool)
    : m_connection_pool(connection_pool), m_initialized(false), m_owns_connection_pool(false) {
    if (!m_connection_pool) {
        logError("Injected connection pool is null");
    }
}

ResourceStorage::~ResourceStorage() {
    shutdown();
}

bool ResourceStorage::initialize() {
    if (m_initialized) {
        logInfo("ResourceStorage already initialized");
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
    logInfo("ResourceStorage initialized successfully with connection pool");
    return true;
}

void ResourceStorage::shutdown() {
    if (!m_initialized) {
        return;
    }
    
    // 只有当我们拥有连接池时才关闭它
    if (m_connection_pool && m_owns_connection_pool) {
        m_connection_pool->shutdown();
    }
    
    m_initialized = false;
    logInfo("ResourceStorage shutdown completed");
}

bool ResourceStorage::createDatabase(const std::string& dbName) {
    if (!m_initialized) {
        logError("ResourceStorage not initialized");
        return false;
    }
    
    std::string query = "CREATE DATABASE IF NOT EXISTS " + dbName;
    if (!executeQuery(query)) {
        return false;
    }
    
    // 切换到新创建的数据库
    query = "USE " + dbName;
    if (!executeQuery(query)) {
        return false;
    }
    
    // 更新连接池配置中的数据库名称
    m_pool_config.database = dbName;
    m_connection_pool->updateConfig(m_pool_config);
    
    logInfo("Database created and selected: " + dbName);
    return true;
}

bool ResourceStorage::createResourceTable() {
    if (!m_initialized) {
        logError("ResourceStorage not initialized");
        return false;
    }

    // 使用单个连接批量创建所有超级表，减少连接开销
    TDengineConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) {
        logError("Failed to get database connection from pool");
        return false;
    }
    
    TAOS* taos = guard->get();
    
    // 定义所有CREATE STABLE语句
    std::vector<std::pair<std::string, std::string>> tables = {
        {"cpu", "CREATE STABLE IF NOT EXISTS cpu ("
        "ts TIMESTAMP, "
        "usage_percent DOUBLE, "
        "load_avg_1m DOUBLE, "
        "load_avg_5m DOUBLE, "
        "load_avg_15m DOUBLE, "
        "core_count INT, "
        "core_allocated INT, "
        "temperature DOUBLE, "
        "voltage DOUBLE, "
        "current DOUBLE, "
        "power DOUBLE"
            ") TAGS (host_ip NCHAR(16))"},

        {"memory", "CREATE STABLE IF NOT EXISTS memory ("
        "ts TIMESTAMP, "
        "total BIGINT, "
        "used BIGINT, "
        "free BIGINT, "
        "usage_percent DOUBLE"
            ") TAGS (host_ip NCHAR(16))"},

        {"network", "CREATE STABLE IF NOT EXISTS network ("
        "ts TIMESTAMP, "
        "rx_bytes BIGINT, "
        "tx_bytes BIGINT, "
        "rx_packets BIGINT, "
        "tx_packets BIGINT, "
        "rx_errors BIGINT, "
        "tx_errors BIGINT, "
        "rx_rate BIGINT, "
        "tx_rate BIGINT"
            ") TAGS (host_ip NCHAR(16), interface NCHAR(32))"},

        {"disk", "CREATE STABLE IF NOT EXISTS disk ("
        "ts TIMESTAMP, "
        "total BIGINT, "
        "used BIGINT, "
        "free BIGINT, "
        "usage_percent DOUBLE"
            ") TAGS (host_ip NCHAR(16), device NCHAR(32), mount_point NCHAR(64))"},

        {"gpu", "CREATE STABLE IF NOT EXISTS gpu ("
        "ts TIMESTAMP, "
        "compute_usage DOUBLE, "
        "mem_usage DOUBLE, "
        "mem_used BIGINT, "
        "mem_total BIGINT, "
        "temperature DOUBLE, "
        "power DOUBLE"
            ") TAGS (host_ip NCHAR(16), gpu_index INT, gpu_name NCHAR(64))"},

        {"node", "CREATE STABLE IF NOT EXISTS node ("
        "ts TIMESTAMP, "
        "gpu_allocated INT, "
        "gpu_num INT"
            ") TAGS (host_ip NCHAR(16))"},

        {"container", "CREATE STABLE IF NOT EXISTS container ("
        "ts TIMESTAMP, "
        "container_count INT, "
        "paused_count INT, "
        "running_count INT, "
        "stopped_count INT"
            ") TAGS (host_ip NCHAR(16))"}
    };
    
    // 批量执行CREATE STABLE语句，使用同一个连接
    std::vector<std::string> failed_tables;
    for (const auto& table : tables) {
        logDebug("Creating stable table: " + table.first);
        
        TAOS_RES* result = taos_query(taos, table.second.c_str());
        if (taos_errno(result) != 0) {
            std::string error_msg = "Failed to create stable table " + table.first + ": " + std::string(taos_errstr(result));
            logError(error_msg);
            failed_tables.push_back(table.first);
        } else {
            logDebug("Successfully created stable table: " + table.first);
        }
        taos_free_result(result);
    }
    
    if (!failed_tables.empty()) {
        std::string failed_list;
        for (size_t i = 0; i < failed_tables.size(); ++i) {
            if (i > 0) failed_list += ", ";
            failed_list += failed_tables[i];
        }
        logError("Failed to create stable tables: " + failed_list);
        return false;
    }
    
    logInfo("All resource stable tables created successfully");
    return true;
}

bool ResourceStorage::executeQuery(const std::string& query) {
    if (!m_initialized) {
        logError("ResourceStorage not initialized");
        return false;
    }
    
    TDengineConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) {
        logError("Failed to get database connection from pool");
        return false;
    }
    
    logDebug("Executing query: " + query);
    
    TAOS* taos = guard->get();
    TAOS_RES* result = taos_query(taos, query.c_str());
    if (taos_errno(result) != 0) {
        logError("SQL execution failed: " + std::string(taos_errstr(result)));
        logError("SQL: " + query);
        taos_free_result(result);
        return false;
    }
    
    taos_free_result(result);
    return true;
}



bool ResourceStorage::insertResourceData(const std::string& hostIp, const node::ResourceInfo& resourceData) {
    if (!m_initialized) {
        logError("ResourceStorage not initialized");
        return false;
    }

    // 使用单个连接和批量INSERT语句
    TDengineConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) {
        logError("Failed to get database connection from pool");
        return false;
    }

    TAOS* taos = guard->get();
    std::string cleanTableName = cleanForTableName(hostIp);
    
    // 获取当前时间戳
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    // 1. 先批量创建所有需要的子表
    std::vector<std::string> createTableStatements;
    
    // CPU表
    createTableStatements.push_back("CREATE TABLE IF NOT EXISTS cpu_" + cleanTableName + " USING cpu TAGS ('" + hostIp + "')");
    
    // Memory表  
    createTableStatements.push_back("CREATE TABLE IF NOT EXISTS memory_" + cleanTableName + " USING memory TAGS ('" + hostIp + "')");
    
    // Node表
    createTableStatements.push_back("CREATE TABLE IF NOT EXISTS node_" + cleanTableName + " USING node TAGS ('" + hostIp + "')");
    
    // Container表
    createTableStatements.push_back("CREATE TABLE IF NOT EXISTS container_" + cleanTableName + " USING container TAGS ('" + hostIp + "')");

    // Network表（多个接口）
    for (const auto& interface : resourceData.resource.network) {
        std::string interfaceName = interface.interface;
        std::string interfaceTableName = cleanForTableName(interfaceName);
        std::string tableName = "network_" + cleanTableName + "_" + interfaceTableName;
        createTableStatements.push_back("CREATE TABLE IF NOT EXISTS " + tableName + " USING network TAGS ('" + hostIp + "', '" + interfaceName + "')");
    }

    // Disk表（多个磁盘）
    for (const auto& disk : resourceData.resource.disk) {
        std::string device = disk.device;
        std::string mountPoint = disk.mount_point;
        std::string deviceTableName = cleanForTableName(device);
        std::string tableName = "disk_" + cleanTableName + "_" + deviceTableName;
        createTableStatements.push_back("CREATE TABLE IF NOT EXISTS " + tableName + " USING disk TAGS ('" + hostIp + "', '" + device + "', '" + mountPoint + "')");
    }

    // GPU表（多个GPU）
    for (const auto& gpu : resourceData.resource.gpu) {
        int gpuIndex = gpu.index;
        std::string gpuName = gpu.name;
        std::string tableName = "gpu_" + cleanTableName + "_" + std::to_string(gpuIndex);
        createTableStatements.push_back("CREATE TABLE IF NOT EXISTS " + tableName + " USING gpu TAGS ('" + hostIp + "', " + std::to_string(gpuIndex) + ", '" + gpuName + "')");
    }

    // 执行所有CREATE TABLE语句
    for (const auto& createSql : createTableStatements) {
        TAOS_RES* result = taos_query(taos, createSql.c_str());
        if (taos_errno(result) != 0) {
            logError("Failed to create table: " + std::string(taos_errstr(result)));
            logError("SQL: " + createSql);
            taos_free_result(result);
            return false;
        }
        taos_free_result(result);
    }

    // 2. 构建批量INSERT语句（TDengine多表插入语法）
    std::ostringstream batchInsertSql;
    batchInsertSql << "INSERT INTO ";

    // CPU数据
    batchInsertSql << "cpu_" << cleanTableName << " VALUES ("
                   << timestamp << ", "
                   << resourceData.resource.cpu.usage_percent << ", "
                   << resourceData.resource.cpu.load_avg_1m << ", "
                   << resourceData.resource.cpu.load_avg_5m << ", "
                   << resourceData.resource.cpu.load_avg_15m << ", "
                   << resourceData.resource.cpu.core_count << ", "
                   << resourceData.resource.cpu.core_allocated << ", "
                   << resourceData.resource.cpu.temperature << ", "
                   << resourceData.resource.cpu.voltage << ", "
                   << resourceData.resource.cpu.current << ", "
                   << resourceData.resource.cpu.power << ") ";

    // Memory数据
    batchInsertSql << "memory_" << cleanTableName << " VALUES ("
                   << timestamp << ", "
                   << resourceData.resource.memory.total << ", "
                   << resourceData.resource.memory.used << ", "
                   << resourceData.resource.memory.free << ", "
                   << resourceData.resource.memory.usage_percent << ") ";

    // Node数据
    batchInsertSql << "node_" << cleanTableName << " VALUES ("
                   << timestamp << ", "
                   << resourceData.resource.gpu_allocated << ", "
                   << resourceData.resource.gpu_num << ") ";

    // Container数据
    int container_count = resourceData.component.size();
    int paused_count = 0, running_count = 0, stopped_count = 0;
    for (const auto& container : resourceData.component) {
        if (container.state == "RUNNING") running_count++;
        else if (container.state == "PAUSED") paused_count++;
        else if (container.state == "STOPPED") stopped_count++;
    }
    
    batchInsertSql << "container_" << cleanTableName << " VALUES ("
                   << timestamp << ", "
                   << container_count << ", "
                   << paused_count << ", "
                   << running_count << ", "
                   << stopped_count << ") ";

    // Network数据（多个接口）
    for (const auto& interface : resourceData.resource.network) {
        std::string interfaceTableName = cleanForTableName(interface.interface);
        std::string tableName = "network_" + cleanTableName + "_" + interfaceTableName;
        
        batchInsertSql << tableName << " VALUES ("
                       << timestamp << ", "
                       << interface.rx_bytes << ", "
                       << interface.tx_bytes << ", "
                       << interface.rx_packets << ", "
                       << interface.tx_packets << ", "
                       << interface.rx_errors << ", "
                       << interface.tx_errors << ", "
                       << interface.rx_rate << ", "
                       << interface.tx_rate << ") ";
    }

    // Disk数据（多个磁盘）
    for (const auto& disk : resourceData.resource.disk) {
        std::string deviceTableName = cleanForTableName(disk.device);
        std::string tableName = "disk_" + cleanTableName + "_" + deviceTableName;
        
        batchInsertSql << tableName << " VALUES ("
                       << timestamp << ", "
                       << disk.total << ", "
                       << disk.used << ", "
                       << disk.free << ", "
                       << disk.usage_percent << ") ";
    }

    // GPU数据（多个GPU）
    for (const auto& gpu : resourceData.resource.gpu) {
        std::string tableName = "gpu_" + cleanTableName + "_" + std::to_string(gpu.index);
        
        batchInsertSql << tableName << " VALUES ("
                       << timestamp << ", "
                       << gpu.compute_usage << ", "
                       << gpu.mem_usage << ", "
                       << gpu.mem_used << ", "
                       << gpu.mem_total << ", "
                       << gpu.temperature << ", "
                       << gpu.power << ") ";
    }

    // 执行批量插入
    std::string finalSql = batchInsertSql.str();
    logDebug("Executing batch insert: " + finalSql);
    
    TAOS_RES* result = taos_query(taos, finalSql.c_str());
    if (taos_errno(result) != 0) {
        logError("Batch insert failed: " + std::string(taos_errstr(result)));
        logError("SQL: " + finalSql);
        taos_free_result(result);
        return false;
    }
    
    taos_free_result(result);
    logDebug("Batch insert completed successfully for host: " + hostIp);
    return true;
}

std::vector<QueryResult> ResourceStorage::executeQuerySQL(const std::string& sql) {
    std::vector<QueryResult> results;
    
    if (!m_initialized) {
        logError("ResourceStorage not initialized");
        return results;
    }
    
    logDebug("Executing query: " + sql);
    
    TDengineConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) {
        logError("Failed to get database connection from pool");
        return results;
    }
    
    TAOS* taos = guard->get();
    TAOS_RES* res = taos_query(taos, sql.c_str());
    if (taos_errno(res) != 0) {
        logError("Query failed: " + std::string(taos_errstr(res)));
        logError("SQL: " + sql);
        taos_free_result(res);
        throw std::runtime_error("ResourceStorage: Query failed: " + std::string(taos_errstr(res)));
    }
    
    // 获取字段信息
    int field_count = taos_field_count(res);
    TAOS_FIELD* fields = taos_fetch_fields(res);
    
    if (field_count == 0) {
        taos_free_result(res);
        return results;
    }
    
    // 处理查询结果
    TAOS_ROW row;
    while ((row = taos_fetch_row(res))) {
        int* lengths = taos_fetch_lengths(res);
        
        QueryResult result;
        result.timestamp = std::chrono::system_clock::now();
        
        // 一次遍历：收集所有字段信息
        for (int i = 0; i < field_count; i++) {
            if (row[i] == nullptr) continue;
            
            std::string field_name = fields[i].name;
            
            if (field_name == "ts") {
                // 时间戳字段 - TDengine 通常返回毫秒精度
                if (fields[i].type == TSDB_DATA_TYPE_TIMESTAMP) {
                    int64_t timestamp = *(int64_t*)row[i];
                    // TDengine 时间戳通常是毫秒精度
                    result.timestamp = std::chrono::system_clock::from_time_t(timestamp / 1000) + 
                                     std::chrono::milliseconds(timestamp % 1000);
                }
            } else if (field_name == "host_ip" || field_name == "mount_point" || field_name == "device" || 
                      field_name == "interface" || field_name == "gpu_name" || field_name == "gpu_index" || 
                      field_name == "sensor_seq" || field_name == "sensor_type" || field_name == "sensor_name" ||
                      field_name == "value" || field_name == "table_type") {
                // 标签字段
                if (fields[i].type == TSDB_DATA_TYPE_NCHAR || fields[i].type == TSDB_DATA_TYPE_BINARY) {
                    result.labels[field_name] = std::string((char*)row[i], lengths[i]);
                } else if (fields[i].type == TSDB_DATA_TYPE_INT) {
                    result.labels[field_name] = std::to_string(*(int*)row[i]);
                } else if (fields[i].type == TSDB_DATA_TYPE_BIGINT) {
                    result.labels[field_name] = std::to_string(*(int64_t*)row[i]);
                } else if (fields[i].type == TSDB_DATA_TYPE_SMALLINT) {
                    result.labels[field_name] = std::to_string(*(int16_t*)row[i]);
                } else if (fields[i].type == TSDB_DATA_TYPE_TINYINT) {
                    result.labels[field_name] = std::to_string(*(int8_t*)row[i]);
                } else if (fields[i].type == TSDB_DATA_TYPE_FLOAT) {
                    result.labels[field_name] = std::to_string(*(float*)row[i]);
                } else if (fields[i].type == TSDB_DATA_TYPE_DOUBLE) {
                    result.labels[field_name] = std::to_string(*(double*)row[i]);
                }
            } else {
                // 数值字段 - 存储到 metrics map 中
                double value = 0.0;
                if (fields[i].type == TSDB_DATA_TYPE_FLOAT) {
                    value = *(float*)row[i];
                } else if (fields[i].type == TSDB_DATA_TYPE_DOUBLE) {
                    value = *(double*)row[i];
                } else if (fields[i].type == TSDB_DATA_TYPE_INT) {
                    value = *(int*)row[i];
                } else if (fields[i].type == TSDB_DATA_TYPE_BIGINT) {
                    value = *(int64_t*)row[i];
                } else if (fields[i].type == TSDB_DATA_TYPE_SMALLINT) {
                    value = *(int16_t*)row[i];
                } else if (fields[i].type == TSDB_DATA_TYPE_TINYINT) {
                    value = *(int8_t*)row[i];
                }
                result.metrics[field_name] = value;
            }
        }
        
        results.push_back(result);
    }
    
    taos_free_result(res);
    
    LogManager::getLogger()->debug("ResourceStorage: Query returned {} rows", results.size());
    return results;
}

NodeResourceData ResourceStorage::getNodeResourceData(const std::string& hostIp) {
    NodeResourceData nodeData;
    nodeData.host_ip = hostIp;
    
    if (!m_initialized) {
        logError("ResourceStorage not initialized");
        return nodeData;
    }
    
    try {
        // 合并所有查询为一个UNION ALL语句，使用table_type字段标识数据来源
        std::string combinedSql = 
            // CPU数据
            "SELECT 'cpu' as table_type, LAST_ROW(ts) as ts, "
            "LAST_ROW(usage_percent) as usage_percent, LAST_ROW(load_avg_1m) as load_avg_1m, "
            "LAST_ROW(load_avg_5m) as load_avg_5m, LAST_ROW(load_avg_15m) as load_avg_15m, "
            "LAST_ROW(core_count) as core_count, LAST_ROW(core_allocated) as core_allocated, "
            "LAST_ROW(temperature) as temperature, LAST_ROW(voltage) as voltage, "
            "LAST_ROW(current) as current, LAST_ROW(power) as power, "
            "NULL as device, NULL as mount_point, NULL as interface, "
            "NULL as gpu_index, NULL as gpu_name, NULL as sensor_seq, NULL as sensor_type, NULL as sensor_name, "
            "NULL as total, NULL as used, NULL as free, "
            "NULL as rx_bytes, NULL as tx_bytes, NULL as rx_packets, NULL as tx_packets, "
            "NULL as rx_errors, NULL as tx_errors, NULL as rx_rate, NULL as tx_rate, "
            "NULL as compute_usage, NULL as mem_usage, NULL as mem_used, NULL as mem_total, "
            "NULL as container_count, NULL as paused_count, NULL as running_count, NULL as stopped_count, "
            "NULL as sensor_value, NULL as alarm_type "
            "FROM cpu WHERE host_ip = '" + hostIp + "' "
            
            "UNION ALL "
            
            // Memory数据
            "SELECT 'memory' as table_type, LAST_ROW(ts) as ts, "
            "LAST_ROW(usage_percent) as usage_percent, NULL as load_avg_1m, "
            "NULL as load_avg_5m, NULL as load_avg_15m, "
            "NULL as core_count, NULL as core_allocated, "
            "NULL as temperature, NULL as voltage, "
            "NULL as current, NULL as power, "
            "NULL as device, NULL as mount_point, NULL as interface, "
            "NULL as gpu_index, NULL as gpu_name, NULL as sensor_seq, NULL as sensor_type, NULL as sensor_name, "
            "LAST_ROW(total) as total, LAST_ROW(used) as used, LAST_ROW(free) as free, "
            "NULL as rx_bytes, NULL as tx_bytes, NULL as rx_packets, NULL as tx_packets, "
            "NULL as rx_errors, NULL as tx_errors, NULL as rx_rate, NULL as tx_rate, "
            "NULL as compute_usage, NULL as mem_usage, NULL as mem_used, NULL as mem_total, "
            "NULL as container_count, NULL as paused_count, NULL as running_count, NULL as stopped_count, "
            "NULL as sensor_value, NULL as alarm_type "
            "FROM memory WHERE host_ip = '" + hostIp + "' "
            
            "UNION ALL "
            
            // Disk数据
            "SELECT 'disk' as table_type, LAST_ROW(ts) as ts, "
            "LAST_ROW(usage_percent) as usage_percent, NULL as load_avg_1m, "
            "NULL as load_avg_5m, NULL as load_avg_15m, "
            "NULL as core_count, NULL as core_allocated, "
            "NULL as temperature, NULL as voltage, "
            "NULL as current, NULL as power, "
            "device, mount_point, NULL as interface, "
            "NULL as gpu_index, NULL as gpu_name, NULL as sensor_seq, NULL as sensor_type, NULL as sensor_name, "
            "LAST_ROW(total) as total, LAST_ROW(used) as used, LAST_ROW(free) as free, "
            "NULL as rx_bytes, NULL as tx_bytes, NULL as rx_packets, NULL as tx_packets, "
            "NULL as rx_errors, NULL as tx_errors, NULL as rx_rate, NULL as tx_rate, "
            "NULL as compute_usage, NULL as mem_usage, NULL as mem_used, NULL as mem_total, "
            "NULL as container_count, NULL as paused_count, NULL as running_count, NULL as stopped_count, "
            "NULL as sensor_value, NULL as alarm_type "
            "FROM disk WHERE host_ip = '" + hostIp + "' GROUP BY device, mount_point "
            
            "UNION ALL "
            
            // Network数据
            "SELECT 'network' as table_type, LAST_ROW(ts) as ts, "
            "NULL as usage_percent, NULL as load_avg_1m, "
            "NULL as load_avg_5m, NULL as load_avg_15m, "
            "NULL as core_count, NULL as core_allocated, "
            "NULL as temperature, NULL as voltage, "
            "NULL as current, NULL as power, "
            "NULL as device, NULL as mount_point, interface, "
            "NULL as gpu_index, NULL as gpu_name, NULL as sensor_seq, NULL as sensor_type, NULL as sensor_name, "
            "NULL as total, NULL as used, NULL as free, "
            "LAST_ROW(rx_bytes) as rx_bytes, LAST_ROW(tx_bytes) as tx_bytes, LAST_ROW(rx_packets) as rx_packets, LAST_ROW(tx_packets) as tx_packets, "
            "LAST_ROW(rx_errors) as rx_errors, LAST_ROW(tx_errors) as tx_errors, LAST_ROW(rx_rate) as rx_rate, LAST_ROW(tx_rate) as tx_rate, "
            "NULL as compute_usage, NULL as mem_usage, NULL as mem_used, NULL as mem_total, "
            "NULL as container_count, NULL as paused_count, NULL as running_count, NULL as stopped_count, "
            "NULL as sensor_value, NULL as alarm_type "
            "FROM network WHERE host_ip = '" + hostIp + "' GROUP BY interface "
            
            "UNION ALL "
            
            // GPU数据
            "SELECT 'gpu' as table_type, LAST_ROW(ts) as ts, "
            "NULL as usage_percent, NULL as load_avg_1m, "
            "NULL as load_avg_5m, NULL as load_avg_15m, "
            "NULL as core_count, NULL as core_allocated, "
            "LAST_ROW(temperature) as temperature, NULL as voltage, "
            "NULL as current, LAST_ROW(power) as power, "
            "NULL as device, NULL as mount_point, NULL as interface, "
            "gpu_index, gpu_name, NULL as sensor_seq, NULL as sensor_type, NULL as sensor_name, "
            "NULL as total, NULL as used, NULL as free, "
            "NULL as rx_bytes, NULL as tx_bytes, NULL as rx_packets, NULL as tx_packets, "
            "NULL as rx_errors, NULL as tx_errors, NULL as rx_rate, NULL as tx_rate, "
            "LAST_ROW(compute_usage) as compute_usage, LAST_ROW(mem_usage) as mem_usage, LAST_ROW(mem_used) as mem_used, LAST_ROW(mem_total) as mem_total, "
            "NULL as container_count, NULL as paused_count, NULL as running_count, NULL as stopped_count, "
            "NULL as sensor_value, NULL as alarm_type "
            "FROM gpu WHERE host_ip = '" + hostIp + "' GROUP BY gpu_index, gpu_name "
            
            "UNION ALL "
            
            // Container数据
            "SELECT 'container' as table_type, LAST_ROW(ts) as ts, "
            "NULL as usage_percent, NULL as load_avg_1m, "
            "NULL as load_avg_5m, NULL as load_avg_15m, "
            "NULL as core_count, NULL as core_allocated, "
            "NULL as temperature, NULL as voltage, "
            "NULL as current, NULL as power, "
            "NULL as device, NULL as mount_point, NULL as interface, "
            "NULL as gpu_index, NULL as gpu_name, NULL as sensor_seq, NULL as sensor_type, NULL as sensor_name, "
            "NULL as total, NULL as used, NULL as free, "
            "NULL as rx_bytes, NULL as tx_bytes, NULL as rx_packets, NULL as tx_packets, "
            "NULL as rx_errors, NULL as tx_errors, NULL as rx_rate, NULL as tx_rate, "
            "NULL as compute_usage, NULL as mem_usage, NULL as mem_used, NULL as mem_total, "
            "LAST_ROW(container_count) as container_count, LAST_ROW(paused_count) as paused_count, LAST_ROW(running_count) as running_count, LAST_ROW(stopped_count) as stopped_count, "
            "NULL as sensor_value, NULL as alarm_type "
            "FROM container WHERE host_ip = '" + hostIp + "' "
            
            "UNION ALL "
            
            // Sensor数据
            "SELECT 'sensor' as table_type, LAST_ROW(ts) as ts, "
            "NULL as usage_percent, NULL as load_avg_1m, "
            "NULL as load_avg_5m, NULL as load_avg_15m, "
            "NULL as core_count, NULL as core_allocated, "
            "NULL as temperature, NULL as voltage, "
            "NULL as current, NULL as power, "
            "NULL as device, NULL as mount_point, NULL as interface, "
            "NULL as gpu_index, NULL as gpu_name, sensor_seq, sensor_type, sensor_name, "
            "NULL as total, NULL as used, NULL as free, "
            "NULL as rx_bytes, NULL as tx_bytes, NULL as rx_packets, NULL as tx_packets, "
            "NULL as rx_errors, NULL as tx_errors, NULL as rx_rate, NULL as tx_rate, "
            "NULL as compute_usage, NULL as mem_usage, NULL as mem_used, NULL as mem_total, "
            "NULL as container_count, NULL as paused_count, NULL as running_count, NULL as stopped_count, "
            "LAST_ROW(sensor_value) as sensor_value, LAST_ROW(alarm_type) as alarm_type "
            "FROM bmc_sensor_super WHERE host_ip = '" + hostIp + "' GROUP BY sensor_seq, sensor_type, sensor_name";
        
        auto allResults = executeQuerySQL(combinedSql);
        
        // 根据table_type处理不同类型的数据
        for (const auto& result : allResults) {
            std::string tableType = result.labels.count("table_type") ? result.labels.at("table_type") : "";
            
            if (tableType == "cpu") {
                nodeData.cpu.has_data = true;
                nodeData.cpu.timestamp = result.timestamp;
                const auto& metrics = result.metrics;
                
                nodeData.cpu.usage_percent = metrics.count("usage_percent") ? metrics.at("usage_percent") : 0.0;
                nodeData.cpu.load_avg_1m = metrics.count("load_avg_1m") ? metrics.at("load_avg_1m") : 0.0;
                nodeData.cpu.load_avg_5m = metrics.count("load_avg_5m") ? metrics.at("load_avg_5m") : 0.0;
                nodeData.cpu.load_avg_15m = metrics.count("load_avg_15m") ? metrics.at("load_avg_15m") : 0.0;
                nodeData.cpu.core_count = static_cast<int>(metrics.count("core_count") ? metrics.at("core_count") : 0);
                nodeData.cpu.core_allocated = static_cast<int>(metrics.count("core_allocated") ? metrics.at("core_allocated") : 0);
                nodeData.cpu.temperature = metrics.count("temperature") ? metrics.at("temperature") : 0.0;
                nodeData.cpu.voltage = metrics.count("voltage") ? metrics.at("voltage") : 0.0;
                nodeData.cpu.current = metrics.count("current") ? metrics.at("current") : 0.0;
                nodeData.cpu.power = metrics.count("power") ? metrics.at("power") : 0.0;
            }
            else if (tableType == "memory") {
                nodeData.memory.has_data = true;
                nodeData.memory.timestamp = result.timestamp;
                const auto& metrics = result.metrics;
                
                nodeData.memory.total = static_cast<int64_t>(metrics.count("total") ? metrics.at("total") : 0);
                nodeData.memory.used = static_cast<int64_t>(metrics.count("used") ? metrics.at("used") : 0);
                nodeData.memory.free = static_cast<int64_t>(metrics.count("free") ? metrics.at("free") : 0);
                nodeData.memory.usage_percent = metrics.count("usage_percent") ? metrics.at("usage_percent") : 0.0;
            }
            else if (tableType == "disk") {
            NodeResourceData::DiskData diskData;
            diskData.device = result.labels.count("device") ? result.labels.at("device") : "unknown";
            diskData.mount_point = result.labels.count("mount_point") ? result.labels.at("mount_point") : "/";
            diskData.total = static_cast<int64_t>(result.metrics.count("total") ? result.metrics.at("total") : 0);
            diskData.used = static_cast<int64_t>(result.metrics.count("used") ? result.metrics.at("used") : 0);
            diskData.free = static_cast<int64_t>(result.metrics.count("free") ? result.metrics.at("free") : 0);
            diskData.usage_percent = result.metrics.count("usage_percent") ? result.metrics.at("usage_percent") : 0.0;
            diskData.timestamp = result.timestamp;
            
            nodeData.disks.push_back(diskData);
        }
            else if (tableType == "network") {
            NodeResourceData::NetworkData networkData;
            networkData.interface = result.labels.count("interface") ? result.labels.at("interface") : "unknown";
            networkData.rx_bytes = static_cast<int64_t>(result.metrics.count("rx_bytes") ? result.metrics.at("rx_bytes") : 0);
            networkData.tx_bytes = static_cast<int64_t>(result.metrics.count("tx_bytes") ? result.metrics.at("tx_bytes") : 0);
            networkData.rx_packets = static_cast<int64_t>(result.metrics.count("rx_packets") ? result.metrics.at("rx_packets") : 0);
            networkData.tx_packets = static_cast<int64_t>(result.metrics.count("tx_packets") ? result.metrics.at("tx_packets") : 0);
            networkData.rx_errors = static_cast<int>(result.metrics.count("rx_errors") ? result.metrics.at("rx_errors") : 0);
            networkData.tx_errors = static_cast<int>(result.metrics.count("tx_errors") ? result.metrics.at("tx_errors") : 0);
            networkData.rx_rate = static_cast<int64_t>(result.metrics.count("rx_rate") ? result.metrics.at("rx_rate") : 0);
            networkData.tx_rate = static_cast<int64_t>(result.metrics.count("tx_rate") ? result.metrics.at("tx_rate") : 0);
            networkData.timestamp = result.timestamp;
            
            nodeData.networks.push_back(networkData);
        }
            else if (tableType == "gpu") {
            NodeResourceData::GpuData gpuData;
            gpuData.index = result.labels.count("gpu_index") ? std::stoi(result.labels.at("gpu_index")) : 0;
            gpuData.name = result.labels.count("gpu_name") ? result.labels.at("gpu_name") : "Unknown GPU";
            gpuData.compute_usage = result.metrics.count("compute_usage") ? result.metrics.at("compute_usage") : 0.0;
            gpuData.mem_usage = result.metrics.count("mem_usage") ? result.metrics.at("mem_usage") : 0.0;
            gpuData.mem_used = static_cast<int64_t>(result.metrics.count("mem_used") ? result.metrics.at("mem_used") : 0);
            gpuData.mem_total = static_cast<int64_t>(result.metrics.count("mem_total") ? result.metrics.at("mem_total") : 0);
            gpuData.temperature = result.metrics.count("temperature") ? result.metrics.at("temperature") : 0.0;
            gpuData.power = result.metrics.count("power") ? result.metrics.at("power") : 0.0;
            gpuData.timestamp = result.timestamp;
            
            nodeData.gpus.push_back(gpuData);
        }
            else if (tableType == "container") {
                nodeData.container.timestamp = result.timestamp;
                const auto& metrics = result.metrics;
                
                nodeData.container.container_count = static_cast<int>(metrics.count("container_count") ? metrics.at("container_count") : 0);
                nodeData.container.paused_count = static_cast<int>(metrics.count("paused_count") ? metrics.at("paused_count") : 0);
                nodeData.container.running_count = static_cast<int>(metrics.count("running_count") ? metrics.at("running_count") : 0);
                nodeData.container.stopped_count = static_cast<int>(metrics.count("stopped_count") ? metrics.at("stopped_count") : 0);
            }
            else if (tableType == "sensor") {
            NodeResourceData::SensorData sensorData;
                sensorData.sequence = result.labels.count("sensor_seq") ? std::stoi(result.labels.at("sensor_seq")) : 0;
                sensorData.type = result.labels.count("sensor_type") ? std::stoi(result.labels.at("sensor_type")) : 0;
                sensorData.name = result.labels.count("sensor_name") ? result.labels.at("sensor_name") : "Unknown Sensor";
            sensorData.value = result.metrics.count("sensor_value") ? result.metrics.at("sensor_value") : 0.0;
            sensorData.alarm_type = result.metrics.count("alarm_type") ? result.metrics.at("alarm_type") : 0;
            sensorData.timestamp = result.timestamp;
            
            nodeData.sensors.push_back(sensorData);
            }
        }
        
        LogManager::getLogger()->debug("ResourceStorage: Retrieved resource data for node {}: CPU={}, Memory={}, Disks={}, Networks={}, GPUs={}, Sensors={}", 
                                     hostIp, nodeData.cpu.has_data, nodeData.memory.has_data, 
                                     nodeData.disks.size(), nodeData.networks.size(), nodeData.gpus.size(), nodeData.sensors.size());
        
    } catch (const std::exception& e) {
        LogManager::getLogger()->error("ResourceStorage: Failed to get resource data for node {}: {}", hostIp, e.what());
    }
    
    return nodeData;
}

// 将NodeResourceData转换为json
nlohmann::json NodeResourceData::to_json() const {
    nlohmann::json j;
    
    j["host_ip"] = host_ip;
    
    // CPU data
    j["cpu"]["usage_percent"] = cpu.usage_percent;
    j["cpu"]["load_avg_1m"] = cpu.load_avg_1m;
    j["cpu"]["load_avg_5m"] = cpu.load_avg_5m;
    j["cpu"]["load_avg_15m"] = cpu.load_avg_15m;
    j["cpu"]["core_count"] = cpu.core_count;
    j["cpu"]["core_allocated"] = cpu.core_allocated;
    j["cpu"]["temperature"] = cpu.temperature;
    j["cpu"]["voltage"] = cpu.voltage;
    j["cpu"]["current"] = cpu.current;
    j["cpu"]["power"] = cpu.power;
    j["cpu"]["has_data"] = cpu.has_data;
    if (cpu.has_data) {
        j["cpu"]["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(cpu.timestamp.time_since_epoch()).count();
    }
    
    // Memory data
    j["memory"]["total"] = memory.total;
    j["memory"]["used"] = memory.used;
    j["memory"]["free"] = memory.free;
    j["memory"]["usage_percent"] = memory.usage_percent;
    j["memory"]["has_data"] = memory.has_data;
    if (memory.has_data) {
        j["memory"]["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(memory.timestamp.time_since_epoch()).count();
    }
    
    // Disk data
    j["disk"] = nlohmann::json::array();
    for (const auto& disk : disks) {
        nlohmann::json diskJson;
        diskJson["device"] = disk.device;
        diskJson["mount_point"] = disk.mount_point;
        diskJson["total"] = disk.total;
        diskJson["used"] = disk.used;
        diskJson["free"] = disk.free;
        diskJson["usage_percent"] = disk.usage_percent;
        diskJson["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(disk.timestamp.time_since_epoch()).count();
        j["disk"].push_back(diskJson);
    }
    
    // Network data
    j["network"] = nlohmann::json::array();
    for (const auto& network : networks) {
        nlohmann::json networkJson;
        networkJson["interface"] = network.interface;
        networkJson["rx_bytes"] = network.rx_bytes;
        networkJson["tx_bytes"] = network.tx_bytes;
        networkJson["rx_packets"] = network.rx_packets;
        networkJson["tx_packets"] = network.tx_packets;
        networkJson["rx_errors"] = network.rx_errors;
        networkJson["tx_errors"] = network.tx_errors;
        networkJson["rx_rate"] = network.rx_rate;
        networkJson["tx_rate"] = network.tx_rate;
        networkJson["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(network.timestamp.time_since_epoch()).count();
        j["network"].push_back(networkJson);
    }
    
    // GPU data
    j["gpu"] = nlohmann::json::array();
    for (const auto& gpu : gpus) {
        nlohmann::json gpuJson;
        gpuJson["index"] = gpu.index;
        gpuJson["name"] = gpu.name;
        gpuJson["compute_usage"] = gpu.compute_usage;
        gpuJson["mem_usage"] = gpu.mem_usage;
        gpuJson["mem_used"] = gpu.mem_used;
        gpuJson["mem_total"] = gpu.mem_total;
        gpuJson["temperature"] = gpu.temperature;
        gpuJson["power"] = gpu.power;
        gpuJson["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(gpu.timestamp.time_since_epoch()).count();
        j["gpu"].push_back(gpuJson);
    }

    // Container data   
    j["container"]["container_count"] = container.container_count;
    j["container"]["paused_count"] = container.paused_count;
    j["container"]["running_count"] = container.running_count;
    j["container"]["stopped_count"] = container.stopped_count;
    j["container"]["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(container.timestamp.time_since_epoch()).count();
    
    // Sensor data
    j["sensor"] = nlohmann::json::array();
    for (const auto& sensor : sensors) {
        nlohmann::json sensorJson;
        sensorJson["sequence"] = sensor.sequence;
        sensorJson["type"] = sensor.type;
        sensorJson["name"] = sensor.name;
        sensorJson["value"] = sensor.value;
        sensorJson["alarm_type"] = sensor.alarm_type;
        sensorJson["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(sensor.timestamp.time_since_epoch()).count();
        j["sensor"].push_back(sensorJson);
    }
    
    return j;
}

// 时间范围解析辅助函数
std::chrono::seconds parseTimeRange(const std::string& time_range) {
    if (time_range.empty()) {
        return std::chrono::seconds(3600); // 默认1小时
    }
    
    std::string number_str;
    char unit = 's';
    
    // 分离数字和单位
    for (char c : time_range) {
        if (std::isdigit(c)) {
            number_str += c;
        } else {
            unit = c;
            break;
        }
    }
    
    if (number_str.empty()) {
        return std::chrono::seconds(3600); // 默认1小时
    }
    
    int number = std::stoi(number_str);
    
    switch (unit) {
        case 's': return std::chrono::seconds(number);
        case 'm': return std::chrono::seconds(number * 60);
        case 'h': return std::chrono::seconds(number * 3600);
        case 'd': return std::chrono::seconds(number * 86400);
        default: return std::chrono::seconds(number); // 默认按秒处理
    }
}

NodeResourceRangeData ResourceStorage::getNodeResourceRangeData(const std::string& hostIp, 
                                                               const std::string& time_range,
                                                               const std::vector<std::string>& metrics) {
    NodeResourceRangeData rangeData;
    rangeData.host_ip = hostIp;
    rangeData.time_range = time_range;
    rangeData.metrics_types = metrics;
    
    if (!m_initialized) {
        logError("ResourceStorage not initialized");
        return rangeData;
    }
    
    // 记录查询时间范围
    rangeData.end_time = std::chrono::system_clock::now();
    auto duration = parseTimeRange(time_range);
    rangeData.start_time = rangeData.end_time - duration;
    
    // 清理hostIp用于表名
    std::string cleanHostIp = cleanForTableName(hostIp);
    
    try {
        // 如果没有指定指标类型，直接返回
        if (metrics.empty()) {
            return rangeData;
        }
        
        // 构建合并的UNION ALL查询
        std::ostringstream combinedSql;
        bool firstQuery = true;
        
        for (const std::string& metric : metrics) {
            if (!firstQuery) {
                combinedSql << " UNION ALL ";
            }
            firstQuery = false;
            
            if (metric == "cpu") {
                combinedSql << "SELECT 'cpu' as table_type, ts, "
                           << "usage_percent, load_avg_1m, load_avg_5m, load_avg_15m, "
                           << "core_count, core_allocated, temperature, voltage, current, power, "
                           << "NULL as device, NULL as mount_point, NULL as interface, "
                           << "NULL as gpu_index, NULL as gpu_name, NULL as fan_seq, NULL as sensor_seq, "
                           << "NULL as sensor_name, NULL as sensor_type, NULL as box_id, NULL as slot_id, "
                           << "NULL as total, NULL as used, NULL as free, "
                           << "NULL as rx_bytes, NULL as tx_bytes, NULL as rx_packets, NULL as tx_packets, "
                           << "NULL as rx_errors, NULL as tx_errors, NULL as rx_rate, NULL as tx_rate, "
                           << "NULL as compute_usage, NULL as mem_usage, NULL as mem_used, NULL as mem_total, "
                           << "NULL as container_count, NULL as paused_count, NULL as running_count, NULL as stopped_count, "
                           << "NULL as alarm_type, NULL as work_mode, NULL as speed, NULL as sensor_value "
                           << "FROM cpu WHERE host_ip = '" << hostIp << "' AND ts > NOW() - " << time_range;
                           
            } else if (metric == "memory") {
                combinedSql << "SELECT 'memory' as table_type, ts, "
                           << "usage_percent, NULL as load_avg_1m, NULL as load_avg_5m, NULL as load_avg_15m, "
                           << "NULL as core_count, NULL as core_allocated, NULL as temperature, NULL as voltage, NULL as current, NULL as power, "
                           << "NULL as device, NULL as mount_point, NULL as interface, "
                           << "NULL as gpu_index, NULL as gpu_name, NULL as fan_seq, NULL as sensor_seq, "
                           << "NULL as sensor_name, NULL as sensor_type, NULL as box_id, NULL as slot_id, "
                           << "total, used, free, "
                           << "NULL as rx_bytes, NULL as tx_bytes, NULL as rx_packets, NULL as tx_packets, "
                           << "NULL as rx_errors, NULL as tx_errors, NULL as rx_rate, NULL as tx_rate, "
                           << "NULL as compute_usage, NULL as mem_usage, NULL as mem_used, NULL as mem_total, "
                           << "NULL as container_count, NULL as paused_count, NULL as running_count, NULL as stopped_count, "
                           << "NULL as alarm_type, NULL as work_mode, NULL as speed, NULL as sensor_value "
                           << "FROM memory WHERE host_ip = '" << hostIp << "' AND ts > NOW() - " << time_range;
                           
            } else if (metric == "disk") {
                combinedSql << "SELECT 'disk' as table_type, ts, "
                           << "usage_percent, NULL as load_avg_1m, NULL as load_avg_5m, NULL as load_avg_15m, "
                           << "NULL as core_count, NULL as core_allocated, NULL as temperature, NULL as voltage, NULL as current, NULL as power, "
                           << "device, mount_point, NULL as interface, "
                           << "NULL as gpu_index, NULL as gpu_name, NULL as fan_seq, NULL as sensor_seq, "
                           << "NULL as sensor_name, NULL as sensor_type, NULL as box_id, NULL as slot_id, "
                           << "total, used, free, "
                           << "NULL as rx_bytes, NULL as tx_bytes, NULL as rx_packets, NULL as tx_packets, "
                           << "NULL as rx_errors, NULL as tx_errors, NULL as rx_rate, NULL as tx_rate, "
                           << "NULL as compute_usage, NULL as mem_usage, NULL as mem_used, NULL as mem_total, "
                           << "NULL as container_count, NULL as paused_count, NULL as running_count, NULL as stopped_count, "
                           << "NULL as alarm_type, NULL as work_mode, NULL as speed, NULL as sensor_value "
                           << "FROM disk WHERE host_ip = '" << hostIp << "' AND ts > NOW() - " << time_range;
                           
            } else if (metric == "network") {
                combinedSql << "SELECT 'network' as table_type, ts, "
                           << "NULL as usage_percent, NULL as load_avg_1m, NULL as load_avg_5m, NULL as load_avg_15m, "
                           << "NULL as core_count, NULL as core_allocated, NULL as temperature, NULL as voltage, NULL as current, NULL as power, "
                           << "NULL as device, NULL as mount_point, interface, "
                           << "NULL as gpu_index, NULL as gpu_name, NULL as fan_seq, NULL as sensor_seq, "
                           << "NULL as sensor_name, NULL as sensor_type, NULL as box_id, NULL as slot_id, "
                           << "NULL as total, NULL as used, NULL as free, "
                           << "rx_bytes, tx_bytes, rx_packets, tx_packets, "
                           << "rx_errors, tx_errors, rx_rate, tx_rate, "
                           << "NULL as compute_usage, NULL as mem_usage, NULL as mem_used, NULL as mem_total, "
                           << "NULL as container_count, NULL as paused_count, NULL as running_count, NULL as stopped_count, "
                           << "NULL as alarm_type, NULL as work_mode, NULL as speed, NULL as sensor_value "
                           << "FROM network WHERE host_ip = '" << hostIp << "' AND ts > NOW() - " << time_range;
                           
            } else if (metric == "gpu") {
                combinedSql << "SELECT 'gpu' as table_type, ts, "
                           << "NULL as usage_percent, NULL as load_avg_1m, NULL as load_avg_5m, NULL as load_avg_15m, "
                           << "NULL as core_count, NULL as core_allocated, temperature, NULL as voltage, NULL as current, power, "
                           << "NULL as device, NULL as mount_point, NULL as interface, "
                           << "gpu_index, gpu_name, NULL as fan_seq, NULL as sensor_seq, "
                           << "NULL as sensor_name, NULL as sensor_type, NULL as box_id, NULL as slot_id, "
                           << "NULL as total, NULL as used, NULL as free, "
                           << "NULL as rx_bytes, NULL as tx_bytes, NULL as rx_packets, NULL as tx_packets, "
                           << "NULL as rx_errors, NULL as tx_errors, NULL as rx_rate, NULL as tx_rate, "
                           << "compute_usage, mem_usage, mem_used, mem_total, "
                           << "NULL as container_count, NULL as paused_count, NULL as running_count, NULL as stopped_count, "
                           << "NULL as alarm_type, NULL as work_mode, NULL as speed, NULL as sensor_value "
                           << "FROM gpu WHERE host_ip = '" << hostIp << "' AND ts > NOW() - " << time_range;
                           
            } else if (metric == "container") {
                combinedSql << "SELECT 'container' as table_type, ts, "
                           << "NULL as usage_percent, NULL as load_avg_1m, NULL as load_avg_5m, NULL as load_avg_15m, "
                           << "NULL as core_count, NULL as core_allocated, NULL as temperature, NULL as voltage, NULL as current, NULL as power, "
                           << "NULL as device, NULL as mount_point, NULL as interface, "
                           << "NULL as gpu_index, NULL as gpu_name, NULL as fan_seq, NULL as sensor_seq, "
                           << "NULL as sensor_name, NULL as sensor_type, NULL as box_id, NULL as slot_id, "
                           << "NULL as total, NULL as used, NULL as free, "
                           << "NULL as rx_bytes, NULL as tx_bytes, NULL as rx_packets, NULL as tx_packets, "
                           << "NULL as rx_errors, NULL as tx_errors, NULL as rx_rate, NULL as tx_rate, "
                           << "NULL as compute_usage, NULL as mem_usage, NULL as mem_used, NULL as mem_total, "
                           << "container_count, paused_count, running_count, stopped_count, "
                           << "NULL as alarm_type, NULL as work_mode, NULL as speed, NULL as sensor_value "
                           << "FROM container WHERE host_ip = '" << hostIp << "' AND ts > NOW() - " << time_range;
            } else if (metric == "sensor") {
                combinedSql << "SELECT 'sensor' as table_type, ts, "
                           << "NULL as usage_percent, NULL as load_avg_1m, NULL as load_avg_5m, NULL as load_avg_15m, "
                           << "NULL as core_count, NULL as core_allocated, NULL as temperature, NULL as voltage, NULL as current, NULL as power, "
                           << "NULL as device, NULL as mount_point, NULL as interface, "
                           << "NULL as gpu_index, NULL as gpu_name, NULL as fan_seq, sensor_seq, "
                           << "sensor_name, sensor_type, box_id, slot_id, "
                           << "NULL as total, NULL as used, NULL as free, "
                           << "NULL as rx_bytes, NULL as tx_bytes, NULL as rx_packets, NULL as tx_packets, "
                           << "NULL as rx_errors, NULL as tx_errors, NULL as rx_rate, NULL as tx_rate, "
                           << "NULL as compute_usage, NULL as mem_usage, NULL as mem_used, NULL as mem_total, "
                           << "NULL as container_count, NULL as paused_count, NULL as running_count, NULL as stopped_count, "
                           << "alarm_type, NULL as work_mode, NULL as speed, sensor_value "
                           << "FROM bmc_sensor_super WHERE host_ip = '" << hostIp << "' AND ts > NOW() - " << time_range;
            }
        }
        
        combinedSql << " ORDER BY ts ASC";
        
        // 执行合并查询
        std::string finalSql = combinedSql.str();
        logDebug("执行范围数据合并查询: " + finalSql);
        auto allResults = executeQuerySQL(finalSql);
        
        // 按table_type分组处理结果
        std::map<std::string, std::vector<QueryResult>> groupedResults;
        for (const auto& result : allResults) {
            std::string tableType = result.labels.count("table_type") ? result.labels.at("table_type") : "";
            if (!tableType.empty()) {
                groupedResults[tableType].push_back(result);
            }
        }
        
        // 为每个请求的指标类型创建时间序列数据
        for (const std::string& metric : metrics) {
            if (groupedResults.count(metric) && !groupedResults[metric].empty()) {
                NodeResourceRangeData::TimeSeriesData timeSeriesData;
                timeSeriesData.metric_type = metric;
                timeSeriesData.data_points = groupedResults[metric];
                rangeData.time_series.push_back(timeSeriesData);
            }
        }
        
        LogManager::getLogger()->debug("ResourceStorage: Retrieved range data for node {} over {}: {} metric types with total {} data points", 
                                     hostIp, time_range, rangeData.time_series.size(),
                                     std::accumulate(rangeData.time_series.begin(), rangeData.time_series.end(), 0,
                                                   [](int sum, const auto& ts) { return sum + ts.data_points.size(); }));
        
    } catch (const std::exception& e) {
        LogManager::getLogger()->error("ResourceStorage: Failed to get range data for node {}: {}", hostIp, e.what());
    }
    
    return rangeData;
}

// NodeResourceRangeData的to_json实现
nlohmann::json NodeResourceRangeData::to_json() const {
    nlohmann::json j;
    
    // 从time_series中提取节点信息，如果可用的话
    int box_id = 0, cpu_id = 1, slot_id = 0;
    for (const auto& ts : time_series) {
        for (const auto& point : ts.data_points) {
            if (point.labels.count("box_id")) {
                box_id = std::stoi(point.labels.at("box_id"));
            }
            if (point.labels.count("cpu_id")) {
                cpu_id = std::stoi(point.labels.at("cpu_id"));
            }
            if (point.labels.count("slot_id")) {
                slot_id = std::stoi(point.labels.at("slot_id"));
            }
            break; // 只需要从第一个点获取这些信息
        }
        if (box_id != 0) break;
    }
    
    j["box_id"] = box_id;
    j["cpu_id"] = cpu_id;
    j["host_ip"] = host_ip;
    j["slot_id"] = slot_id;
    j["time_range"] = time_range;
    
    // 构建metrics对象
    nlohmann::json metrics;
    
    // 容器指标（暂时为空）
    metrics["container"] = nlohmann::json::object();
    
    for (const auto& ts : time_series) {
        if (ts.metric_type == "cpu") {
            // CPU数据是数组格式
            nlohmann::json cpu_array = nlohmann::json::array();
            for (const auto& point : ts.data_points) {
                nlohmann::json cpu_point;
                
                // 从metrics中提取所有字段
                for (const auto& metric : point.metrics) {
                    if (metric.first == "timestamp") {
                        cpu_point["timestamp"] = static_cast<int64_t>(metric.second);
                    } else {
                        cpu_point[metric.first] = metric.second;
                    }
                }
                
                cpu_array.push_back(cpu_point);
            }
            metrics["cpu"] = cpu_array;
            
        } else if (ts.metric_type == "memory") {
            // Memory数据是数组格式
            nlohmann::json memory_array = nlohmann::json::array();
            for (const auto& point : ts.data_points) {
                nlohmann::json memory_point;
                
                for (const auto& metric : point.metrics) {
                    if (metric.first == "timestamp") {
                        memory_point["timestamp"] = static_cast<int64_t>(metric.second);
                    } else {
                        memory_point[metric.first] = metric.second;
                    }
                }
                
                memory_array.push_back(memory_point);
            }
            metrics["memory"] = memory_array;
            
        } else if (ts.metric_type == "disk") {
            // Disk数据按设备分组
            nlohmann::json disk_groups;
            std::map<std::string, nlohmann::json> device_data;
            
            for (const auto& point : ts.data_points) {
                std::string device_key = point.labels.count("group_key") ? point.labels.at("group_key") : "unknown";
                
                nlohmann::json disk_point;
                for (const auto& metric : point.metrics) {
                    if (metric.first == "timestamp") {
                        disk_point["timestamp"] = static_cast<int64_t>(metric.second);
                    } else {
                        disk_point[metric.first] = metric.second;
                    }
                }
                
                // 添加标签字段
                if (point.labels.count("device")) {
                    disk_point["device"] = point.labels.at("device");
                }
                if (point.labels.count("mount_point")) {
                    disk_point["mount_point"] = point.labels.at("mount_point");
                }
                
                if (device_data.find(device_key) == device_data.end()) {
                    device_data[device_key] = nlohmann::json::array();
                }
                device_data[device_key].push_back(disk_point);
            }
            
            for (const auto& device : device_data) {
                disk_groups[device.first] = device.second;
            }
            metrics["disk"] = disk_groups;
            
        } else if (ts.metric_type == "network") {
            // Network数据按接口分组
            nlohmann::json network_groups;
            std::map<std::string, nlohmann::json> interface_data;
            
            for (const auto& point : ts.data_points) {
                std::string interface_key = point.labels.count("group_key") ? point.labels.at("group_key") : "unknown";
                
                nlohmann::json network_point;
                for (const auto& metric : point.metrics) {
                    if (metric.first == "timestamp") {
                        network_point["timestamp"] = static_cast<int64_t>(metric.second);
                    } else {
                        network_point[metric.first] = metric.second;
                    }
                }
                
                // 添加接口字段
                if (point.labels.count("interface")) {
                    network_point["interface"] = point.labels.at("interface");
                }
                
                if (interface_data.find(interface_key) == interface_data.end()) {
                    interface_data[interface_key] = nlohmann::json::array();
                }
                interface_data[interface_key].push_back(network_point);
            }
            
            for (const auto& interface : interface_data) {
                network_groups[interface.first] = interface.second;
            }
            metrics["network"] = network_groups;
            
        } else if (ts.metric_type == "gpu") {
            // GPU数据按GPU索引分组
            nlohmann::json gpu_groups;
            std::map<std::string, nlohmann::json> gpu_data;
            
            for (const auto& point : ts.data_points) {
                std::string gpu_key = point.labels.count("group_key") ? point.labels.at("group_key") : "gpu_0";
                
                nlohmann::json gpu_point;
                for (const auto& metric : point.metrics) {
                    if (metric.first == "timestamp") {
                        gpu_point["timestamp"] = static_cast<int64_t>(metric.second);
                    } else {
                        gpu_point[metric.first] = metric.second;
                    }
                }
                
                // 添加GPU相关字段
                if (point.labels.count("gpu_index")) {
                    gpu_point["index"] = std::stoi(point.labels.at("gpu_index"));
                }
                if (point.labels.count("gpu_name")) {
                    gpu_point["name"] = point.labels.at("gpu_name");
                }
                
                if (gpu_data.find(gpu_key) == gpu_data.end()) {
                    gpu_data[gpu_key] = nlohmann::json::array();
                }
                gpu_data[gpu_key].push_back(gpu_point);
            }
            
            for (const auto& gpu : gpu_data) {
                gpu_groups[gpu.first] = gpu.second;
            }
            metrics["gpu"] = gpu_groups;
        } else if (ts.metric_type == "container") {
            nlohmann::json container_array = nlohmann::json::array();

            for (const auto& point : ts.data_points) {
                nlohmann::json container_point;
                for (const auto& metric : point.metrics) {
                    if (metric.first == "timestamp") {
                        container_point["timestamp"] = static_cast<int64_t>(metric.second);
                    } else {
                        container_point[metric.first] = metric.second;
                    }
                }
                container_array.push_back(container_point);
            }

            metrics["container"] = container_array;
        } else if (ts.metric_type == "sensor") {
            // Sensor数据按传感器类型分组
            nlohmann::json sensor_groups;
            std::map<std::string, nlohmann::json> sensor_data;

            for (const auto& point : ts.data_points) {
                std::string sensor_key = point.labels.count("sensor_name") ? point.labels.at("sensor_name") : "sensor_0";

                nlohmann::json sensor_point;
                for (const auto& metric : point.metrics) {
                    if (metric.first == "timestamp") {
                        sensor_point["timestamp"] = static_cast<int64_t>(metric.second);
                    } else {
                        sensor_point[metric.first] = metric.second;
                    }
                }

                if (sensor_data.find(sensor_key) == sensor_data.end()) {
                    sensor_data[sensor_key] = nlohmann::json::array();
                }
                sensor_data[sensor_key].push_back(sensor_point);
            }
            
            for (const auto& sensor : sensor_data) {
                sensor_groups[sensor.first] = sensor.second;
            }
            metrics["sensor"] = sensor_groups;
        }
    }
    
    j["metrics"] = metrics;
    
    return j;
}



// 日志辅助方法
void ResourceStorage::logInfo(const std::string& message) const {
    LogManager::getLogger()->info("ResourceStorage: {}", message);
}

void ResourceStorage::logError(const std::string& message) const {
    LogManager::getLogger()->error("ResourceStorage: {}", message);
}

void ResourceStorage::logDebug(const std::string& message) const {
    LogManager::getLogger()->debug("ResourceStorage: {}", message);
}