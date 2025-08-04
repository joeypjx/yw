#pragma once

#include "resource_storage.h"
#include "node_storage.h"
#include "bmc_storage.h"
#include "json.hpp"
#include <string>
#include <vector>
#include <memory>

// CPU指标结构
struct CPUMetrics
{
    int core_allocated = 0;
    int core_count = 0;
    double current = 0.0;
    double load_avg_15m = 0.0;
    double load_avg_1m = 0.0;
    double load_avg_5m = 0.0;
    double power = 0.0;
    double temperature = 0.0;
    long long timestamp = 0;
    double usage_percent = 0.0;
    double voltage = 0.0;
};

// 内存指标结构
struct MemoryMetrics
{
    long long free = 0;
    long long timestamp = 0;
    long long total = 0;
    double usage_percent = 0.0;
    long long used = 0;
};

// 单个磁盘信息结构
struct DiskInfo
{
    std::string device;
    long long free = 0;
    std::string mount_point;
    long long total = 0;
    double usage_percent = 0.0;
    long long used = 0;
    long long timestamp = 0;
};

// 磁盘指标结构
struct DiskMetrics
{
    int disk_count = 0;
    std::vector<DiskInfo> disks;
    long long timestamp = 0;
};

// 单个网络接口信息结构
struct NetworkInfo
{
    std::string interface;
    long long rx_bytes = 0;
    long long rx_errors = 0;
    long long rx_packets = 0;
    long long tx_bytes = 0;
    long long tx_errors = 0;
    long long tx_packets = 0;
    double tx_rate = 0.0;
    double rx_rate = 0.0;
    long long timestamp = 0;
};

// 网络指标结构
struct NetworkMetrics
{
    int network_count = 0;
    std::vector<NetworkInfo> networks;
    long long timestamp = 0;
};

// 单个GPU信息结构
struct GPUInfo
{
    double compute_usage = 0.0;
    double current = 0.0;
    int index = 0;
    long long mem_total = 0;
    double mem_usage = 0.0;
    long long mem_used = 0;
    std::string name;
    double power = 0.0;
    double temperature = 0.0;
    double voltage = 0.0;
    long long timestamp = 0;
};

// GPU指标结构
struct GPUMetrics
{
    int gpu_count = 0;
    std::vector<GPUInfo> gpus;
    long long timestamp = 0;
};

// 容器指标结构
struct ContainerMetrics
{
    int container_count = 0;
    int paused_count = 0;
    int running_count = 0;
    int stopped_count = 0;
    long long timestamp = 0;
};

// 单个传感器信息结构
struct SensorInfo
{
    int sequence = 0;
    std::string type;
    std::string name;
    double value = 0.0;
    std::string alarm_type;
    long long timestamp = 0;
};

// 传感器指标结构
struct SensorMetrics
{
    int sensor_count = 0;
    std::vector<SensorInfo> sensors;
    long long timestamp = 0;
};

// 节点指标数据结构
struct NodeMetricsData
{
    std::string board_type;
    int box_id;
    std::string box_type;
    std::string cpu_arch;
    int cpu_id;
    std::string cpu_type;
    long long updated_at = 0;
    std::string host_ip;
    std::string hostname;
    int id;
    CPUMetrics latest_cpu_metrics;
    DiskMetrics latest_disk_metrics;
    ContainerMetrics latest_container_metrics;
    GPUMetrics latest_gpu_metrics;
    MemoryMetrics latest_memory_metrics;
    NetworkMetrics latest_network_metrics;
    SensorMetrics latest_sensor_metrics;
    std::string os_type;
    std::string resource_type;
    int service_port = 0;
    int slot_id = 0;
    int srio_id = 0;
    std::string status;
};

// 节点指标数据列表结构
struct NodeMetricsDataList
{
    std::vector<NodeMetricsData> nodes_metrics;
};

struct Pagination
{
    int total_count = 0;
    int page = 1;
    int page_size = 20;
    int total_pages = 0;
    bool has_next = false;
    bool has_prev = false;
};

// 分页节点指标数据列表结构
struct NodeMetricsDataListPagination
{
    bool success = false;
    std::string error_message;
    NodeMetricsDataList data; // 节点指标数据
    Pagination pagination;
};

// 历史指标查询请求结构
struct HistoricalMetricsRequest
{
    std::string host_ip;
    std::string time_range;
    std::vector<std::string> metrics;
};

struct NodeMetricsRangeDataResult
{
    bool success = false;
    std::string error_message;
    NodeResourceRangeData data;
};

class ResourceManager
{
private:
    std::shared_ptr<ResourceStorage> m_resource_storage;
    std::shared_ptr<NodeStorage> m_node_storage;
    std::shared_ptr<BMCStorage> m_bmc_storage;

    // 私有方法
    std::pair<bool, std::string> validateRequest(const HistoricalMetricsRequest &request);

    // 辅助方法：构建单个节点的指标数据
    NodeMetricsData buildNodeMetricsData(const std::shared_ptr<NodeData> &node);

public:
    ResourceManager(std::shared_ptr<ResourceStorage> resource_storage,
                    std::shared_ptr<NodeStorage> node_storage,
                    std::shared_ptr<BMCStorage> bmc_storage);

    // 节点列表查询
    std::vector<std::shared_ptr<NodeData>> getNodesList();

    // 单个节点查询
    std::shared_ptr<NodeData> getNode(const std::string &host_ip);

    // 分页当前指标查询
    NodeMetricsDataListPagination getPaginatedCurrentMetrics(int page, int page_size);

    // 历史指标查询
    NodeMetricsRangeDataResult getHistoricalMetrics(const HistoricalMetricsRequest &request);

    // 历史BMC数据查询
    HistoricalBMCResponse getHistoricalBMC(const HistoricalBMCRequest &request);

    // 指标参数解析
    std::vector<std::string> parseMetricsParam(const std::string &metrics_param);
};

// JSON序列化支持
// 指标数据结构
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CPUMetrics, core_allocated, core_count, current, load_avg_15m, load_avg_1m, load_avg_5m, power, temperature, timestamp, usage_percent, voltage)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MemoryMetrics, free, timestamp, total, usage_percent, used)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DiskInfo, device, free, mount_point, total, usage_percent, used, timestamp)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DiskMetrics, disk_count, disks, timestamp)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NetworkInfo, interface, rx_bytes, rx_errors, rx_packets, tx_bytes, tx_errors, tx_packets, tx_rate, rx_rate, timestamp)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NetworkMetrics, network_count, networks, timestamp)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GPUInfo, compute_usage, current, index, mem_total, mem_usage, mem_used, name, power, temperature, voltage, timestamp)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GPUMetrics, gpu_count, gpus, timestamp)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ContainerMetrics, container_count, paused_count, running_count, stopped_count, timestamp)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SensorInfo, sequence, type, name, value, alarm_type, timestamp)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SensorMetrics, sensor_count, sensors, timestamp)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NodeMetricsData, board_type, box_id, box_type, cpu_arch, cpu_id, cpu_type, updated_at, host_ip, hostname, id, latest_cpu_metrics, latest_disk_metrics, latest_container_metrics, latest_gpu_metrics, latest_memory_metrics, latest_network_metrics, latest_sensor_metrics, os_type, resource_type, service_port, slot_id, srio_id, status)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NodeMetricsDataList, nodes_metrics)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Pagination, total_count, page, page_size, total_pages, has_next, has_prev)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NodeMetricsDataListPagination, success, error_message, data, pagination)

// 历史指标查询结果

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(QueryResult, labels, metrics, timestamp)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TimeSeriesData, metric_type, data_points)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NodeResourceRangeData, host_ip, time_range, metrics_types, start_time, end_time, time_series)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NodeMetricsRangeDataResult, success, error_message, data)