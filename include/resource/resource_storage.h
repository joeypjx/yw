#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include <taos.h>
#include "json.hpp"

// 查询结果结构
struct QueryResult {
    std::map<std::string, std::string> labels;   // 标签
    std::map<std::string, double> metrics;       // 指标名称到值的映射，如 'usage_percent' -> 85.5
    std::chrono::system_clock::time_point timestamp;
};

// 节点资源数据结构
struct NodeResourceData {
    std::string host_ip;
    
    // CPU数据
    struct {
        double usage_percent = 0.0;
        double load_avg_1m = 0.0;
        double load_avg_5m = 0.0;
        double load_avg_15m = 0.0;
        int core_count = 0;
        int core_allocated = 0;
        double temperature = 0.0;
        double voltage = 0.0;
        double current = 0.0;
        double power = 0.0;
        std::chrono::system_clock::time_point timestamp;
        bool has_data = false;
    } cpu;
    
    // Memory数据
    struct {
        int64_t total = 0;
        int64_t used = 0;
        int64_t free = 0;
        double usage_percent = 0.0;
        std::chrono::system_clock::time_point timestamp;
        bool has_data = false;
    } memory;
    
    // Disk数据
    struct DiskData {
        std::string device;
        std::string mount_point;
        int64_t total = 0;
        int64_t used = 0;
        int64_t free = 0;
        double usage_percent = 0.0;
        std::chrono::system_clock::time_point timestamp;
    };
    std::vector<DiskData> disks;
    
    // Network数据
    struct NetworkData {
        std::string interface;
        int64_t rx_bytes = 0;
        int64_t tx_bytes = 0;
        int64_t rx_packets = 0;
        int64_t tx_packets = 0;
        int rx_errors = 0;
        int tx_errors = 0;
        int64_t rx_rate = 0;
        int64_t tx_rate = 0;
        std::chrono::system_clock::time_point timestamp;
    };
    std::vector<NetworkData> networks;
    
    // GPU数据
    struct GpuData {
        int index = 0;
        std::string name;
        double compute_usage = 0.0;
        double mem_usage = 0.0;
        int64_t mem_used = 0;
        int64_t mem_total = 0;
        double temperature = 0.0;
        double power = 0.0;
        std::chrono::system_clock::time_point timestamp;
    };
    std::vector<GpuData> gpus;
    
    nlohmann::json to_json() const;
};

// 节点时间段资源数据结构
struct NodeResourceRangeData {
    std::string host_ip;
    std::string time_range;
    std::vector<std::string> metrics_types;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    
    // 时序数据，每个指标类型对应一个数据序列
    struct TimeSeriesData {
        std::string metric_type;  // cpu, memory, disk, network, gpu
        std::vector<QueryResult> data_points;
    };
    std::vector<TimeSeriesData> time_series;
    
    nlohmann::json to_json() const;
};

class ResourceStorage {
public:
    ResourceStorage(const std::string& host, const std::string& user, const std::string& password);
    ~ResourceStorage();

    bool connect();
    void disconnect();
    bool createDatabase(const std::string& dbName);
    bool createResourceTable();
    bool insertResourceData(const std::string& hostIp, const nlohmann::json& resourceData);
    
    // 查询接口
    std::vector<QueryResult> executeQuerySQL(const std::string& sql);
    
    // 获取指定节点的所有资源数据
    NodeResourceData getNodeResourceData(const std::string& hostIp);
    
    // 获取指定节点在某个时间段内的资源数据
    NodeResourceRangeData getNodeResourceRangeData(const std::string& hostIp, 
                                                   const std::string& time_range,
                                                   const std::vector<std::string>& metrics);

private:
    std::string m_host;
    std::string m_user;
    std::string m_password;
    TAOS* m_taos;
    bool m_connected;

    bool executeQuery(const std::string& sql);
    
    bool insertCpuData(const std::string& hostIp, const nlohmann::json& cpuData);
    bool insertMemoryData(const std::string& hostIp, const nlohmann::json& memoryData);
    bool insertNetworkData(const std::string& hostIp, const nlohmann::json& networkData);
    bool insertDiskData(const std::string& hostIp, const nlohmann::json& diskData);
    bool insertGpuData(const std::string& hostIp, const nlohmann::json& gpuData);
    bool insertNodeData(const std::string& hostIp, const nlohmann::json& resourceData);
};