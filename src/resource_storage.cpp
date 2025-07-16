#include "resource_storage.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>

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

ResourceStorage::ResourceStorage(const std::string& host, const std::string& user, const std::string& password)
    : m_host(host), m_user(user), m_password(password), m_taos(nullptr), m_connected(false) {
}

ResourceStorage::~ResourceStorage() {
    disconnect();
}

bool ResourceStorage::connect() {
    if (m_connected) {
        return true;
    }

    m_taos = taos_connect(m_host.c_str(), m_user.c_str(), m_password.c_str(), nullptr, 0);
    if (m_taos == nullptr) {
        std::cerr << "Failed to connect to TDengine: " << taos_errstr(m_taos) << std::endl;
        return false;
    }

    m_connected = true;
    return true;
}

void ResourceStorage::disconnect() {
    if (m_taos != nullptr) {
        taos_close(m_taos);
        m_taos = nullptr;
    }
    m_connected = false;
}

bool ResourceStorage::createDatabase(const std::string& dbName) {
    if (!m_connected) {
        std::cerr << "Not connected to TDengine" << std::endl;
        return false;
    }

    std::string sql = "CREATE DATABASE IF NOT EXISTS " + dbName;
    if (!executeQuery(sql)) {
        return false;
    }

    sql = "USE " + dbName;
    return executeQuery(sql);
}

bool ResourceStorage::createResourceTable() {
    if (!m_connected) {
        std::cerr << "Not connected to TDengine" << std::endl;
        return false;
    }

    // Create CPU super table
    std::string sql = "CREATE STABLE IF NOT EXISTS cpu_metrics ("
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
        ") TAGS (host_ip NCHAR(16))";
    if (!executeQuery(sql)) return false;

    // Create Memory super table
    sql = "CREATE STABLE IF NOT EXISTS memory_metrics ("
        "ts TIMESTAMP, "
        "total BIGINT, "
        "used BIGINT, "
        "free BIGINT, "
        "usage_percent DOUBLE"
        ") TAGS (host_ip NCHAR(16))";
    if (!executeQuery(sql)) return false;

    // Create Network super table
    sql = "CREATE STABLE IF NOT EXISTS network_metrics ("
        "ts TIMESTAMP, "
        "rx_bytes BIGINT, "
        "tx_bytes BIGINT, "
        "rx_packets BIGINT, "
        "tx_packets BIGINT, "
        "rx_errors INT, "
        "tx_errors INT, "
        "rx_rate BIGINT, "
        "tx_rate BIGINT"
        ") TAGS (host_ip NCHAR(16), interface NCHAR(32))";
    if (!executeQuery(sql)) return false;

    // Create Disk super table
    sql = "CREATE STABLE IF NOT EXISTS disk_metrics ("
        "ts TIMESTAMP, "
        "total BIGINT, "
        "used BIGINT, "
        "free BIGINT, "
        "usage_percent DOUBLE"
        ") TAGS (host_ip NCHAR(16), device NCHAR(32), mount_point NCHAR(64))";
    if (!executeQuery(sql)) return false;

    // Create GPU super table
    sql = "CREATE STABLE IF NOT EXISTS gpu_metrics ("
        "ts TIMESTAMP, "
        "compute_usage DOUBLE, "
        "mem_usage DOUBLE, "
        "mem_used BIGINT, "
        "mem_total BIGINT, "
        "temperature DOUBLE, "
        "power DOUBLE"
        ") TAGS (host_ip NCHAR(16), gpu_index INT, gpu_name NCHAR(64))";
    if (!executeQuery(sql)) return false;

    // Create Node super table
    sql = "CREATE STABLE IF NOT EXISTS node_metrics ("
        "ts TIMESTAMP, "
        "gpu_allocated INT, "
        "gpu_num INT"
        ") TAGS (host_ip NCHAR(16))";
    if (!executeQuery(sql)) return false;

    return true;
}

bool ResourceStorage::executeQuery(const std::string& sql) {
    if (!m_connected) {
        std::cerr << "Not connected to TDengine" << std::endl;
        return false;
    }

    TAOS_RES* result = taos_query(m_taos, sql.c_str());
    if (taos_errno(result) != 0) {
        std::cerr << "SQL execution failed: " << taos_errstr(result) << std::endl;
        std::cerr << "SQL: " << sql << std::endl;
        taos_free_result(result);
        return false;
    }

    taos_free_result(result);
    return true;
}

std::string ResourceStorage::generateCreateTableSQL() {
    std::ostringstream oss;
    
    // Create CPU super table
    oss << "CREATE STABLE IF NOT EXISTS cpu_metrics ("
        << "ts TIMESTAMP, "
        << "usage_percent DOUBLE, "
        << "load_avg_1m DOUBLE, "
        << "load_avg_5m DOUBLE, "
        << "load_avg_15m DOUBLE, "
        << "core_count INT, "
        << "core_allocated INT, "
        << "temperature DOUBLE, "
        << "voltage DOUBLE, "
        << "current DOUBLE, "
        << "power DOUBLE"
        << ") TAGS (host_ip NCHAR(16)); ";

    // Create Memory super table
    oss << "CREATE STABLE IF NOT EXISTS memory_metrics ("
        << "ts TIMESTAMP, "
        << "total BIGINT, "
        << "used BIGINT, "
        << "free BIGINT, "
        << "usage_percent DOUBLE"
        << ") TAGS (host_ip NCHAR(16)); ";

    // Create Network super table
    oss << "CREATE STABLE IF NOT EXISTS network_metrics ("
        << "ts TIMESTAMP, "
        << "rx_bytes BIGINT, "
        << "tx_bytes BIGINT, "
        << "rx_packets BIGINT, "
        << "tx_packets BIGINT, "
        << "rx_errors INT, "
        << "tx_errors INT, "
        << "rx_rate BIGINT, "
        << "tx_rate BIGINT"
        << ") TAGS (host_ip NCHAR(16), interface NCHAR(32)); ";

    // Create Disk super table
    oss << "CREATE STABLE IF NOT EXISTS disk_metrics ("
        << "ts TIMESTAMP, "
        << "total BIGINT, "
        << "used BIGINT, "
        << "free BIGINT, "
        << "usage_percent DOUBLE"
        << ") TAGS (host_ip NCHAR(16), device NCHAR(32), mount_point NCHAR(64)); ";

    // Create GPU super table
    oss << "CREATE STABLE IF NOT EXISTS gpu_metrics ("
        << "ts TIMESTAMP, "
        << "compute_usage DOUBLE, "
        << "mem_usage DOUBLE, "
        << "mem_used BIGINT, "
        << "mem_total BIGINT, "
        << "temperature DOUBLE, "
        << "power DOUBLE"
        << ") TAGS (host_ip NCHAR(16), gpu_index INT, gpu_name NCHAR(64)); ";

    // Create Node super table
    oss << "CREATE STABLE IF NOT EXISTS node_metrics ("
        << "ts TIMESTAMP, "
        << "gpu_allocated INT, "
        << "gpu_num INT"
        << ") TAGS (host_ip NCHAR(16)); ";

    return oss.str();
}

bool ResourceStorage::insertResourceData(const std::string& hostIp, const nlohmann::json& resourceData) {
    if (!m_connected) {
        std::cerr << "Not connected to TDengine" << std::endl;
        return false;
    }

    bool success = true;

    // Insert CPU data
    if (resourceData.contains("cpu")) {
        success &= insertCpuData(hostIp, resourceData["cpu"]);
    }

    // Insert Memory data
    if (resourceData.contains("memory")) {
        success &= insertMemoryData(hostIp, resourceData["memory"]);
    }

    // Insert Network data
    if (resourceData.contains("network")) {
        success &= insertNetworkData(hostIp, resourceData["network"]);
    }

    // Insert Disk data
    if (resourceData.contains("disk")) {
        success &= insertDiskData(hostIp, resourceData["disk"]);
    }

    // Insert GPU data
    if (resourceData.contains("gpu")) {
        success &= insertGpuData(hostIp, resourceData["gpu"]);
    }

    // Insert Node data
    success &= insertNodeData(hostIp, resourceData);

    return success;
}

bool ResourceStorage::insertCpuData(const std::string& hostIp, const nlohmann::json& cpuData) {
    // Create table if not exists (clean host IP for valid table names)
    std::string tableName = cleanForTableName(hostIp);
    std::string createTableSQL = "CREATE TABLE IF NOT EXISTS cpu_" + tableName + " USING cpu_metrics TAGS ('" + hostIp + "')";
    if (!executeQuery(createTableSQL)) {
        return false;
    }

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::ostringstream oss;
    oss << "INSERT INTO cpu_" << tableName << " VALUES ("
        << timestamp << ", "
        << cpuData.value("usage_percent", 0.0) << ", "
        << cpuData.value("load_avg_1m", 0.0) << ", "
        << cpuData.value("load_avg_5m", 0.0) << ", "
        << cpuData.value("load_avg_15m", 0.0) << ", "
        << cpuData.value("core_count", 0) << ", "
        << cpuData.value("core_allocated", 0) << ", "
        << cpuData.value("temperature", 0.0) << ", "
        << cpuData.value("voltage", 0.0) << ", "
        << cpuData.value("current", 0.0) << ", "
        << cpuData.value("power", 0.0) << ")";

    return executeQuery(oss.str());
}

bool ResourceStorage::insertMemoryData(const std::string& hostIp, const nlohmann::json& memoryData) {
    // Create table if not exists (clean host IP for valid table names)
    std::string tableName = cleanForTableName(hostIp);
    std::string createTableSQL = "CREATE TABLE IF NOT EXISTS memory_" + tableName + " USING memory_metrics TAGS ('" + hostIp + "')";
    if (!executeQuery(createTableSQL)) {
        return false;
    }

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::ostringstream oss;
    oss << "INSERT INTO memory_" << tableName << " VALUES ("
        << timestamp << ", "
        << memoryData.value("total", 0) << ", "
        << memoryData.value("used", 0) << ", "
        << memoryData.value("free", 0) << ", "
        << memoryData.value("usage_percent", 0.0) << ")";

    return executeQuery(oss.str());
}

bool ResourceStorage::insertNetworkData(const std::string& hostIp, const nlohmann::json& networkData) {
    if (!networkData.is_array()) {
        std::cerr << "Network data should be an array" << std::endl;
        return false;
    }

    bool success = true;
    for (const auto& interface : networkData) {
        std::string interfaceName = interface.value("interface", "unknown");
        std::string hostTableName = cleanForTableName(hostIp);
        std::string interfaceTableName = cleanForTableName(interfaceName);
        std::string tableName = "network_" + hostTableName + "_" + interfaceTableName;
        
        // Create table if not exists
        std::string createTableSQL = "CREATE TABLE IF NOT EXISTS " + tableName + " USING network_metrics TAGS ('" + hostIp + "', '" + interfaceName + "')";
        if (!executeQuery(createTableSQL)) {
            success = false;
            continue;
        }

        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        std::ostringstream oss;
        oss << "INSERT INTO " << tableName << " VALUES ("
            << timestamp << ", "
            << interface.value("rx_bytes", 0) << ", "
            << interface.value("tx_bytes", 0) << ", "
            << interface.value("rx_packets", 0) << ", "
            << interface.value("tx_packets", 0) << ", "
            << interface.value("rx_errors", 0) << ", "
            << interface.value("tx_errors", 0) << ", "
            << interface.value("rx_rate", 0) << ", "
            << interface.value("tx_rate", 0) << ")";

        if (!executeQuery(oss.str())) {
            success = false;
        }
    }

    return success;
}

bool ResourceStorage::insertDiskData(const std::string& hostIp, const nlohmann::json& diskData) {
    if (!diskData.is_array()) {
        std::cerr << "Disk data should be an array" << std::endl;
        return false;
    }

    bool success = true;
    for (const auto& disk : diskData) {
        std::string device = disk.value("device", "unknown");
        std::string mountPoint = disk.value("mount_point", "unknown");
        std::string hostTableName = cleanForTableName(hostIp);
        std::string deviceTableName = cleanForTableName(device);
        std::string tableName = "disk_" + hostTableName + "_" + deviceTableName;
        
        // Create table if not exists
        std::string createTableSQL = "CREATE TABLE IF NOT EXISTS " + tableName + " USING disk_metrics TAGS ('" + hostIp + "', '" + device + "', '" + mountPoint + "')";
        if (!executeQuery(createTableSQL)) {
            success = false;
            continue;
        }

        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        std::ostringstream oss;
        oss << "INSERT INTO " << tableName << " VALUES ("
            << timestamp << ", "
            << disk.value("total", 0) << ", "
            << disk.value("used", 0) << ", "
            << disk.value("free", 0) << ", "
            << disk.value("usage_percent", 0.0) << ")";

        if (!executeQuery(oss.str())) {
            success = false;
        }
    }

    return success;
}

bool ResourceStorage::insertGpuData(const std::string& hostIp, const nlohmann::json& gpuData) {
    if (!gpuData.is_array()) {
        std::cerr << "GPU data should be an array" << std::endl;
        return false;
    }

    bool success = true;
    for (const auto& gpu : gpuData) {
        int gpuIndex = gpu.value("index", 0);
        std::string gpuName = gpu.value("name", "unknown");
        std::string hostTableName = cleanForTableName(hostIp);
        std::string tableName = "gpu_" + hostTableName + "_" + std::to_string(gpuIndex);
        
        // Create table if not exists
        std::string createTableSQL = "CREATE TABLE IF NOT EXISTS " + tableName + " USING gpu_metrics TAGS ('" + hostIp + "', " + std::to_string(gpuIndex) + ", '" + gpuName + "')";
        if (!executeQuery(createTableSQL)) {
            success = false;
            continue;
        }

        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        std::ostringstream oss;
        oss << "INSERT INTO " << tableName << " VALUES ("
            << timestamp << ", "
            << gpu.value("compute_usage", 0.0) << ", "
            << gpu.value("mem_usage", 0.0) << ", "
            << gpu.value("mem_used", 0) << ", "
            << gpu.value("mem_total", 0) << ", "
            << gpu.value("temperature", 0.0) << ", "
            << gpu.value("power", 0.0) << ")";

        if (!executeQuery(oss.str())) {
            success = false;
        }
    }

    return success;
}

bool ResourceStorage::insertNodeData(const std::string& hostIp, const nlohmann::json& resourceData) {
    // Create table if not exists (clean host IP for valid table names)
    std::string tableName = cleanForTableName(hostIp);
    std::string createTableSQL = "CREATE TABLE IF NOT EXISTS node_" + tableName + " USING node_metrics TAGS ('" + hostIp + "')";
    if (!executeQuery(createTableSQL)) {
        return false;
    }

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::ostringstream oss;
    oss << "INSERT INTO node_" << tableName << " VALUES ("
        << timestamp << ", "
        << resourceData.value("gpu_allocated", 0) << ", "
        << resourceData.value("gpu_num", 0) << ")";

    return executeQuery(oss.str());
}

std::vector<QueryResult> ResourceStorage::executeQuerySQL(const std::string& sql) {
    std::vector<QueryResult> results;
    
    if (!m_connected || !m_taos) {
        std::cerr << "ResourceStorage: Not connected to TDengine" << std::endl;
        return results;
    }
    
    std::cout << "[DEBUG] ResourceStorage: Executing query: " << sql << std::endl;
    
    TAOS_RES* res = taos_query(m_taos, sql.c_str());
    if (taos_errno(res) != 0) {
        std::cerr << "ResourceStorage: Query failed: " << taos_errstr(res) << std::endl;
        std::cerr << "ResourceStorage: SQL: " << sql << std::endl;
        taos_free_result(res);
        return results;
    }
    
    // 获取字段信息
    int field_count = taos_field_count(res);
    TAOS_FIELD* fields = taos_fetch_fields(res);
    
    if (field_count == 0) {
        taos_free_result(res);
        return results;
    }
    
    // 处理���询结果
    TAOS_ROW row;
    while ((row = taos_fetch_row(res))) {
        QueryResult result;
        int* lengths = taos_fetch_lengths(res);
        
        // 默认设置时间戳为当前时间
        result.timestamp = std::chrono::system_clock::now();
        result.value = 0.0;
        
        for (int i = 0; i < field_count; i++) {
            if (row[i] == nullptr) continue;
            
            std::string field_name = fields[i].name;
            
            // 处理不同类型的字段
            if (field_name == "ts") {
                // 时间戳字段
                if (fields[i].type == TSDB_DATA_TYPE_TIMESTAMP) {
                    int64_t timestamp = *(int64_t*)row[i];
                    result.timestamp = std::chrono::system_clock::from_time_t(timestamp / 1000);
                }
            } else if (field_name == "value" || field_name == "usage_percent" || 
                       field_name == "compute_usage" || field_name == "mem_usage" ||
                       field_name == "load_avg_1m" || field_name == "free") {
                // 数值字段
                if (fields[i].type == TSDB_DATA_TYPE_FLOAT) {
                    result.value = *(float*)row[i];
                } else if (fields[i].type == TSDB_DATA_TYPE_DOUBLE) {
                    result.value = *(double*)row[i];
                } else if (fields[i].type == TSDB_DATA_TYPE_INT) {
                    result.value = *(int*)row[i];
                } else if (fields[i].type == TSDB_DATA_TYPE_BIGINT) {
                    result.value = *(int64_t*)row[i];
                } else if (fields[i].type == TSDB_DATA_TYPE_SMALLINT) {
                    result.value = *(int16_t*)row[i];
                } else if (fields[i].type == TSDB_DATA_TYPE_TINYINT) {
                    result.value = *(int8_t*)row[i];
                }
            } else {
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
            }
        }
        
        results.push_back(result);
    }
    
    taos_free_result(res);
    
    std::cout << "[DEBUG] ResourceStorage: Query returned " << results.size() << " rows" << std::endl;
    return results;
}