#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include <atomic>
#include <taos.h>
#include "json.hpp"
#include "node_model.h"
#include "tdengine_connection_pool.h"

// 查询结果结构
struct QueryResult {
    std::map<std::string, std::string> labels;   // 标签
    std::map<std::string, double> metrics;       // 指标名称到值的映射，如 'usage_percent' -> 85.5
    int64_t timestamp;  // 毫秒时间戳
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
        int64_t timestamp = 0;  // 毫秒时间戳
        bool has_data = false;
    } cpu;
    
    // Memory数据
    struct {
        int64_t total = 0;
        int64_t used = 0;
        int64_t free = 0;
        double usage_percent = 0.0;
        int64_t timestamp = 0;  // 毫秒时间戳
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
        int64_t timestamp = 0;  // 毫秒时间戳
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
        int64_t timestamp = 0;  // 毫秒时间戳
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
        int64_t timestamp = 0;  // 毫秒时间戳
    };
    std::vector<GpuData> gpus;

    // Container数据
    struct ContainerData {
        int container_count = 0;
        int paused_count = 0;
        int running_count = 0;
        int stopped_count = 0;
        int64_t timestamp = 0;  // 毫秒时间戳
    } container;

    // Sensor数据
    struct SensorData {
        int sequence = 0;
        int type = 0;
        std::string name;
        double value = 0.0;
        int alarm_type = 0;
        int64_t timestamp = 0;  // 毫秒时间戳
    };
    std::vector<SensorData> sensors;
    
    nlohmann::json to_json() const;
};

// 时序数据结构
struct TimeSeriesData {
    std::string metric_type;  // cpu, memory, disk, network, gpu
    std::vector<QueryResult> data_points;
};

// 节点时间段资源数据结构
struct NodeResourceRangeData {
    std::string host_ip;
    std::string time_range;
    std::vector<std::string> metrics_types;
    int64_t start_time = 0;  // 毫秒时间戳
    int64_t end_time = 0;    // 毫秒时间戳
    
    std::vector<TimeSeriesData> time_series;
    
    nlohmann::json to_json() const;
};

class ResourceStorage {
public:
    // 连接池注入构造函数 - 推荐使用
    ResourceStorage(std::shared_ptr<TDengineConnectionPool> connection_pool);
    ~ResourceStorage();

    bool createDatabase(const std::string& dbName);
    bool createResourceTable();
    // 插入资源数据
    bool insertResourceData(const std::string& hostIp, const node::ResourceInfo& resourceData);
    
    // 查询接口
    std::vector<QueryResult> executeQuerySQL(const std::string& sql);
    
    // 获取指定节点的所有资源数据
    NodeResourceData getNodeResourceData(const std::string& hostIp);
    
    // 获取指定节点在某个时间段内的资源数据
    NodeResourceRangeData getNodeResourceRangeData(const std::string& hostIp, 
                                                   const std::string& time_range,
                                                   const std::vector<std::string>& metrics);
    


private:
    TDenginePoolConfig m_pool_config;
    std::shared_ptr<TDengineConnectionPool> m_connection_pool;

    bool executeQuery(const std::string& sql);
    
    // 日志辅助方法
    void logInfo(const std::string& message) const;
    void logError(const std::string& message) const;
    void logDebug(const std::string& message) const;
    
    // 批量插入优化方法
    bool insertResourceDataBatch(const std::string& hostIp, const node::ResourceInfo& resourceData);
};