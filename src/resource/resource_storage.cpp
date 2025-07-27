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
        LogManager::getLogger()->error("Failed to connect to TDengine: {}", taos_errstr(m_taos));
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
        LogManager::getLogger()->error("Not connected to TDengine");
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
        LogManager::getLogger()->error("Not connected to TDengine");
        return false;
    }

    // Create CPU super table
    std::string sql = "CREATE STABLE IF NOT EXISTS cpu ("
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
    sql = "CREATE STABLE IF NOT EXISTS memory ("
        "ts TIMESTAMP, "
        "total BIGINT, "
        "used BIGINT, "
        "free BIGINT, "
        "usage_percent DOUBLE"
        ") TAGS (host_ip NCHAR(16))";
    if (!executeQuery(sql)) return false;

    // Create Network super table
    sql = "CREATE STABLE IF NOT EXISTS network ("
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
    sql = "CREATE STABLE IF NOT EXISTS disk ("
        "ts TIMESTAMP, "
        "total BIGINT, "
        "used BIGINT, "
        "free BIGINT, "
        "usage_percent DOUBLE"
        ") TAGS (host_ip NCHAR(16), device NCHAR(32), mount_point NCHAR(64))";
    if (!executeQuery(sql)) return false;

    // Create GPU super table
    sql = "CREATE STABLE IF NOT EXISTS gpu ("
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
    sql = "CREATE STABLE IF NOT EXISTS node ("
        "ts TIMESTAMP, "
        "gpu_allocated INT, "
        "gpu_num INT"
        ") TAGS (host_ip NCHAR(16))";
    if (!executeQuery(sql)) return false;

    return true;
}

bool ResourceStorage::executeQuery(const std::string& sql) {
    if (!m_connected) {
        LogManager::getLogger()->error("Not connected to TDengine");
        return false;
    }

    TAOS_RES* result = taos_query(m_taos, sql.c_str());
    if (taos_errno(result) != 0) {
        LogManager::getLogger()->error("SQL execution failed: {}", taos_errstr(result));
        LogManager::getLogger()->error("SQL: {}", sql);
        taos_free_result(result);
        return false;
    }

    taos_free_result(result);
    return true;
}

bool ResourceStorage::insertResourceData(const std::string& hostIp, const nlohmann::json& resourceData) {
    if (!m_connected) {
        LogManager::getLogger()->error("Not connected to TDengine");
        return false;
    }

    bool success = true;

    // LogManager::getLogger()->info("Inserting resource data for host: {}", resourceData.dump(4));

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
    std::string createTableSQL = "CREATE TABLE IF NOT EXISTS cpu_" + tableName + " USING cpu TAGS ('" + hostIp + "')";
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
    std::string createTableSQL = "CREATE TABLE IF NOT EXISTS memory_" + tableName + " USING memory TAGS ('" + hostIp + "')";
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
        LogManager::getLogger()->error("Network data should be an array");
        return false;
    }

    bool success = true;
    for (const auto& interface : networkData) {
        std::string interfaceName = interface.value("interface", "unknown");
        std::string hostTableName = cleanForTableName(hostIp);
        std::string interfaceTableName = cleanForTableName(interfaceName);
        std::string tableName = "network_" + hostTableName + "_" + interfaceTableName;
        
        // Create table if not exists
        std::string createTableSQL = "CREATE TABLE IF NOT EXISTS " + tableName + " USING network TAGS ('" + hostIp + "', '" + interfaceName + "')";
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
        LogManager::getLogger()->error("Disk data should be an array");
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
        std::string createTableSQL = "CREATE TABLE IF NOT EXISTS " + tableName + " USING disk TAGS ('" + hostIp + "', '" + device + "', '" + mountPoint + "')";
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
        LogManager::getLogger()->error("GPU data should be an array");
        return false;
    }

    bool success = true;
    for (const auto& gpu : gpuData) {
        int gpuIndex = gpu.value("index", 0);
        std::string gpuName = gpu.value("name", "unknown");
        std::string hostTableName = cleanForTableName(hostIp);
        std::string tableName = "gpu_" + hostTableName + "_" + std::to_string(gpuIndex);
        
        // Create table if not exists
        std::string createTableSQL = "CREATE TABLE IF NOT EXISTS " + tableName + " USING gpu TAGS ('" + hostIp + "', " + std::to_string(gpuIndex) + ", '" + gpuName + "')";
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
    std::string createTableSQL = "CREATE TABLE IF NOT EXISTS node_" + tableName + " USING node TAGS ('" + hostIp + "')";
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
        LogManager::getLogger()->error("ResourceStorage: Not connected to TDengine");
        return results;
    }
    
    LogManager::getLogger()->debug("ResourceStorage: Executing query: {}", sql);
    
    TAOS_RES* res = taos_query(m_taos, sql.c_str());
    if (taos_errno(res) != 0) {
        LogManager::getLogger()->error("ResourceStorage: Query failed: {}", taos_errstr(res));
        LogManager::getLogger()->error("ResourceStorage: SQL: {}", sql);
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
                      field_name == "value") {
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
    
    if (!m_connected || !m_taos) {
        LogManager::getLogger()->error("ResourceStorage: Not connected to TDengine");
        return nodeData;
    }
    
    // Helper function to clean host IP for table names
    auto cleanForTableName = [](const std::string& input) {
        std::string cleaned = input;
        std::replace(cleaned.begin(), cleaned.end(), '/', '_');
        std::replace(cleaned.begin(), cleaned.end(), '-', '_');
        std::replace(cleaned.begin(), cleaned.end(), '.', '_');
        std::replace(cleaned.begin(), cleaned.end(), ':', '_');
        std::replace(cleaned.begin(), cleaned.end(), ' ', '_');
        return cleaned;
    };
    
    std::string cleanHostIp = cleanForTableName(hostIp);
    
    try {
        // 获取CPU数据 - 使用LAST_ROW优化
        std::string cpuSql = "SELECT LAST_ROW(ts) as ts, LAST_ROW(usage_percent) as usage_percent, LAST_ROW(load_avg_1m) as load_avg_1m, LAST_ROW(load_avg_5m) as load_avg_5m, LAST_ROW(load_avg_15m) as load_avg_15m, LAST_ROW(core_count) as core_count, LAST_ROW(core_allocated) as core_allocated, LAST_ROW(temperature) as temperature, LAST_ROW(voltage) as voltage, LAST_ROW(current) as current, LAST_ROW(power) as power FROM cpu WHERE host_ip = '" + hostIp + "'";
        auto cpuResults = executeQuerySQL(cpuSql);
        
        if (!cpuResults.empty()) {
            nodeData.cpu.has_data = true;
            nodeData.cpu.timestamp = cpuResults[0].timestamp;
            
            // 现在每个 QueryResult 包含所有指标，直接使用第一个结果
            const auto& cpuMetrics = cpuResults[0].metrics;
            
            nodeData.cpu.usage_percent = cpuMetrics.count("usage_percent") ? cpuMetrics.at("usage_percent") : 0.0;
            nodeData.cpu.load_avg_1m = cpuMetrics.count("load_avg_1m") ? cpuMetrics.at("load_avg_1m") : 0.0;
            nodeData.cpu.load_avg_5m = cpuMetrics.count("load_avg_5m") ? cpuMetrics.at("load_avg_5m") : 0.0;
            nodeData.cpu.load_avg_15m = cpuMetrics.count("load_avg_15m") ? cpuMetrics.at("load_avg_15m") : 0.0;
            nodeData.cpu.core_count = static_cast<int>(cpuMetrics.count("core_count") ? cpuMetrics.at("core_count") : 0);
            nodeData.cpu.core_allocated = static_cast<int>(cpuMetrics.count("core_allocated") ? cpuMetrics.at("core_allocated") : 0);
            nodeData.cpu.temperature = cpuMetrics.count("temperature") ? cpuMetrics.at("temperature") : 0.0;
            nodeData.cpu.voltage = cpuMetrics.count("voltage") ? cpuMetrics.at("voltage") : 0.0;
            nodeData.cpu.current = cpuMetrics.count("current") ? cpuMetrics.at("current") : 0.0;
            nodeData.cpu.power = cpuMetrics.count("power") ? cpuMetrics.at("power") : 0.0;
        }
        
        // 获取Memory数据 - 使用LAST_ROW优化
        std::string memorySql = "SELECT LAST_ROW(ts) as ts, LAST_ROW(total) as total, LAST_ROW(used) as used, LAST_ROW(free) as free, LAST_ROW(usage_percent) as usage_percent FROM memory WHERE host_ip = '" + hostIp + "'";
        auto memoryResults = executeQuerySQL(memorySql);
        
        if (!memoryResults.empty()) {
            nodeData.memory.has_data = true;
            nodeData.memory.timestamp = memoryResults[0].timestamp;
            
            // 直接使用第一个结果的 metrics
            const auto& memoryMetrics = memoryResults[0].metrics;
            
            nodeData.memory.total = static_cast<int64_t>(memoryMetrics.count("total") ? memoryMetrics.at("total") : 0);
            nodeData.memory.used = static_cast<int64_t>(memoryMetrics.count("used") ? memoryMetrics.at("used") : 0);
            nodeData.memory.free = static_cast<int64_t>(memoryMetrics.count("free") ? memoryMetrics.at("free") : 0);
            nodeData.memory.usage_percent = memoryMetrics.count("usage_percent") ? memoryMetrics.at("usage_percent") : 0.0;
        }
        
        // 获取Disk数据 - 使用LAST_ROW和GROUP BY优化
        std::string diskSql = "SELECT LAST_ROW(ts) as ts, LAST_ROW(total) as total, LAST_ROW(used) as used, LAST_ROW(free) as free, LAST_ROW(usage_percent) as usage_percent, device, mount_point FROM disk WHERE host_ip = '" + hostIp + "' GROUP BY device, mount_point";
        auto diskResults = executeQuerySQL(diskSql);
        
        for (const auto& result : diskResults) {
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
        
        // 获取Network数据 - 使用LAST_ROW和GROUP BY优化
        std::string networkSql = "SELECT LAST_ROW(ts) as ts, LAST_ROW(rx_bytes) as rx_bytes, LAST_ROW(tx_bytes) as tx_bytes, LAST_ROW(rx_packets) as rx_packets, LAST_ROW(tx_packets) as tx_packets, LAST_ROW(rx_errors) as rx_errors, LAST_ROW(tx_errors) as tx_errors, LAST_ROW(rx_rate) as rx_rate, LAST_ROW(tx_rate) as tx_rate, interface FROM network WHERE host_ip = '" + hostIp + "' GROUP BY interface";
        auto networkResults = executeQuerySQL(networkSql);
        
        for (const auto& result : networkResults) {
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
        
        // 获取GPU数据 - 使用LAST_ROW和GROUP BY优化
        std::string gpuSql = "SELECT LAST_ROW(ts) as ts, LAST_ROW(compute_usage) as compute_usage, LAST_ROW(mem_usage) as mem_usage, LAST_ROW(mem_used) as mem_used, LAST_ROW(mem_total) as mem_total, LAST_ROW(temperature) as temperature, LAST_ROW(power) as power, gpu_index, gpu_name FROM gpu WHERE host_ip = '" + hostIp + "' GROUP BY gpu_index, gpu_name";
        auto gpuResults = executeQuerySQL(gpuSql);
        
        for (const auto& result : gpuResults) {
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
        
        LogManager::getLogger()->debug("ResourceStorage: Retrieved resource data for node {}: CPU={}, Memory={}, Disks={}, Networks={}, GPUs={}", 
                                     hostIp, nodeData.cpu.has_data, nodeData.memory.has_data, 
                                     nodeData.disks.size(), nodeData.networks.size(), nodeData.gpus.size());
        
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
    
    if (!m_connected || !m_taos) {
        LogManager::getLogger()->error("ResourceStorage: Not connected to TDengine");
        return rangeData;
    }
    
    // 记录查询时间范围
    rangeData.end_time = std::chrono::system_clock::now();
    auto duration = parseTimeRange(time_range);
    rangeData.start_time = rangeData.end_time - duration;
    
    // 清理hostIp用于表名
    std::string cleanHostIp = cleanForTableName(hostIp);
    
    try {
        for (const std::string& metric : metrics) {
            NodeResourceRangeData::TimeSeriesData timeSeriesData;
            timeSeriesData.metric_type = metric;
            
            if (metric == "cpu") {
                std::string sql = "SELECT * FROM cpu WHERE host_ip = '" + hostIp + "'" +
                                " AND ts > NOW() - " + time_range + 
                                " ORDER BY ts ASC";
                timeSeriesData.data_points = executeQuerySQL(sql);
                
            } else if (metric == "memory") {
                std::string sql = "SELECT * FROM memory WHERE host_ip = '" + hostIp + "'" +
                                " AND ts > NOW() - " + time_range + 
                                " ORDER BY ts ASC";
                timeSeriesData.data_points = executeQuerySQL(sql);
                
            } else if (metric == "disk") {
                std::string sql = "SELECT * FROM disk WHERE host_ip = '" + hostIp + "'" +
                                " AND ts > NOW() - " + time_range + 
                                " ORDER BY ts ASC";
                timeSeriesData.data_points = executeQuerySQL(sql);
                
            } else if (metric == "network") {
                std::string sql = "SELECT * FROM network WHERE host_ip = '" + hostIp + "'" +
                                " AND ts > NOW() - " + time_range + 
                                " ORDER BY ts ASC";
                timeSeriesData.data_points = executeQuerySQL(sql);
                
            } else if (metric == "gpu") {
                std::string sql = "SELECT * FROM gpu WHERE host_ip = '" + hostIp + "'" +
                                " AND ts > NOW() - " + time_range + 
                                " ORDER BY ts ASC";
                timeSeriesData.data_points = executeQuerySQL(sql);
            }
            
            if (!timeSeriesData.data_points.empty()) {
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
        }
    }
    
    j["metrics"] = metrics;
    
    return j;
}