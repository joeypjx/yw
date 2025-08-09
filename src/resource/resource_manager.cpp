#include "resource_manager.h"
#include "log_manager.h"
#include <sstream>
#include <algorithm>

using json = nlohmann::json;

ResourceManager::ResourceManager(std::shared_ptr<ResourceStorage> resource_storage, 
                                 std::shared_ptr<NodeStorage> node_storage,
                                 std::shared_ptr<BMCStorage> bmc_storage)
    : m_resource_storage(resource_storage), m_node_storage(node_storage), m_bmc_storage(bmc_storage) {
}

NodeMetricsRangeDataResult ResourceManager::getHistoricalMetrics(const HistoricalMetricsRequest& request) {
    NodeMetricsRangeDataResult response;
    
    // 验证请求参数
    auto validation_result = validateRequest(request);
    if (!validation_result.first) {
        response.success = false;
        response.error_message = validation_result.second;
        return response;
    }
    
    if (!m_resource_storage || !m_node_storage) {
        response.success = false;
        response.error_message = "Storage components not available";
        LogManager::getLogger()->error("ResourceManager: Storage components not available");
        return response;
    }
    
    try {
        // 调用ResourceStorage方法获取历史数据
        auto rangeData = m_resource_storage->getNodeResourceRangeData(
            request.host_ip, 
            request.time_range, 
            request.metrics
        );
        
        // 获取节点信息用于补充字段
        auto node_list = m_node_storage->getAllNodes();
        int box_id = 0;
        int cpu_id = 1;
        int slot_id = 0;
        
        for (const auto& node_data : node_list) {
            if (node_data->host_ip == request.host_ip) {
                box_id = node_data->box_id;
                cpu_id = node_data->cpu_id;
                slot_id = node_data->slot_id;
                break;
            }
        }
        
        // 构建符合API文档格式的响应数据
        NodeResourceRangeData transformedData;
        transformedData.host_ip = request.host_ip;
        transformedData.time_range = request.time_range;
        transformedData.metrics_types = request.metrics;
        transformedData.start_time = rangeData.start_time;
        transformedData.end_time = rangeData.end_time;
        
        // 处理时间序列数据，转换为API格式
        for (const auto& ts : rangeData.time_series) {
            TimeSeriesData newTS;
            newTS.metric_type = ts.metric_type;
            
            if (ts.metric_type == "cpu" || ts.metric_type == "memory" || ts.metric_type == "container") {
                // CPU和Memory是单一数据源，直接转换
                for (const auto& point : ts.data_points) {
                    QueryResult newPoint = point;
                    // 转换时间戳为秒
                    newPoint.metrics["timestamp"] = static_cast<double>(point.timestamp / 1000);
                    newTS.data_points.push_back(newPoint);
                }
            } else if (ts.metric_type == "disk" || ts.metric_type == "network" || ts.metric_type == "gpu" || ts.metric_type == "sensor") {
                // 这些资源类型需要按设备/接口分组
                std::map<std::string, std::vector<QueryResult>> grouped_data;
                
                for (const auto& point : ts.data_points) {
                    std::string key;
                    if (ts.metric_type == "disk") {
                        key = point.labels.count("device") ? point.labels.at("device") : "unknown";
                        // 清理设备名用作JSON key
                        std::replace(key.begin(), key.end(), '/', '_');
                        std::replace(key.begin(), key.end(), '-', '_');
                        key = "_" + key;  // 添加前缀符合API格式
                    } else if (ts.metric_type == "network") {
                        key = point.labels.count("interface") ? point.labels.at("interface") : "unknown";
                    } else if (ts.metric_type == "gpu") {
                        key = "gpu_" + (point.labels.count("gpu_index") ? point.labels.at("gpu_index") : "0");
                    } else if (ts.metric_type == "sensor") {
                        key = "sensor_" + (point.labels.count("name") ? point.labels.at("name") : "unknown");
                    }
                    
                    QueryResult newPoint = point;
                    // 转换时间戳为秒
                    newPoint.metrics["timestamp"] = static_cast<double>(point.timestamp / 1000);
                    
                    grouped_data[key].push_back(newPoint);
                }
                
                // 将分组数据存储到标签中，以便to_json方法正确处理
                for (const auto& group : grouped_data) {
                    for (const auto& point : group.second) {
                        QueryResult labeledPoint = point;
                        labeledPoint.labels["group_key"] = group.first;
                        labeledPoint.labels["metric_type"] = ts.metric_type;
                        newTS.data_points.push_back(labeledPoint);
                    }
                }
            }
            
            transformedData.time_series.push_back(newTS);
        }
        
        response.data = transformedData;
        response.success = true;
        
        LogManager::getLogger()->debug("ResourceManager: Successfully transformed historical metrics for node {} over {}: {} metric types", 
                                     request.host_ip, request.time_range, request.metrics.size());
        
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = "Failed to retrieve historical metrics: " + std::string(e.what());
        LogManager::getLogger()->error("ResourceManager: Exception in getHistoricalMetrics: {}", e.what());
    }
    
    return response;
}

HistoricalBMCResponse ResourceManager::getHistoricalBMC(const HistoricalBMCRequest& request) {
    HistoricalBMCResponse response;
    
    if (!m_bmc_storage || !m_node_storage) {
        response.success = false;
        response.error_message = "Storage components not available";
        LogManager::getLogger()->error("ResourceManager: Storage components not available");
        return response;
    }

    try {
        auto rangeData = m_bmc_storage->getBMCRangeData(
            request.box_id, 
            request.time_range, 
            request.metrics
        );

        response.data = rangeData;
        response.success = true;

        LogManager::getLogger()->debug("ResourceManager: Successfully retrieved historical bmc for box_id: {}", request.box_id);

    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = "Failed to retrieve historical bmc: " + std::string(e.what());
        LogManager::getLogger()->error("ResourceManager: Exception in getHistoricalBMC: {}", e.what());
    }
    
    return response;
}

std::vector<std::string> ResourceManager::parseMetricsParam(const std::string& metrics_param) {
    std::vector<std::string> metrics;
    
    if (metrics_param.empty()) {
        // 默认获取所有指标类型
        return {"cpu", "memory", "disk", "network", "gpu", "container", "sensor"};
    }
    
    // 分割逗号分隔的字符串
    std::stringstream ss(metrics_param);
    std::string metric;
    while (std::getline(ss, metric, ',')) {
        // 去除前后空格
        metric.erase(0, metric.find_first_not_of(" \t"));
        metric.erase(metric.find_last_not_of(" \t") + 1);
        if (!metric.empty()) {
            metrics.push_back(metric);
        }
    }
    
    return metrics;
}

std::pair<bool, std::string> ResourceManager::validateRequest(const HistoricalMetricsRequest& request) {
    // 验证host_ip
    if (request.host_ip.empty()) {
        return {false, "'host_ip' parameter is required"};
    }
    
    // 验证metrics
    if (request.metrics.empty()) {
        return {false, "At least one metric type is required"};
    }
    
    // 验证指标类型是否有效
    const std::vector<std::string> valid_metrics = {"cpu", "memory", "disk", "network", "gpu", "container", "sensor"};
    for (const auto& metric : request.metrics) {
        if (std::find(valid_metrics.begin(), valid_metrics.end(), metric) == valid_metrics.end()) {
            return {false, "Invalid metric type: " + metric + ". Valid types are: cpu, memory, disk, network, gpu, container, sensor"};
        }
    }
    
    // time_range在这里不做详细验证，由TDengine处理
    
    return {true, ""};
}

NodeMetricsData ResourceManager::buildNodeMetricsData(const std::shared_ptr<NodeData>& node) {
    std::string host_ip = node->host_ip;
    auto current_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    auto steady_now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // 使用 getNodeResourceData 方法获取所有资源数据
    auto resourceData = m_resource_storage->getNodeResourceData(host_ip);

    // 构建CPU指标
    CPUMetrics cpu_metrics;
    cpu_metrics.core_allocated = resourceData.cpu.core_allocated;
    cpu_metrics.core_count = resourceData.cpu.core_count;
    cpu_metrics.current = resourceData.cpu.current;
    cpu_metrics.load_avg_15m = resourceData.cpu.load_avg_15m;
    cpu_metrics.load_avg_1m = resourceData.cpu.load_avg_1m;
    cpu_metrics.load_avg_5m = resourceData.cpu.load_avg_5m;
    cpu_metrics.power = resourceData.cpu.power;
    cpu_metrics.temperature = resourceData.cpu.temperature;
    cpu_metrics.timestamp = resourceData.cpu.has_data ? 
        resourceData.cpu.timestamp / 1000 : current_timestamp;
    cpu_metrics.usage_percent = resourceData.cpu.usage_percent;
    cpu_metrics.voltage = resourceData.cpu.voltage;
    
    // 构建Memory指标
    MemoryMetrics memory_metrics;
    memory_metrics.free = resourceData.memory.free;
    memory_metrics.timestamp = resourceData.memory.has_data ? 
        resourceData.memory.timestamp / 1000 : current_timestamp;
    memory_metrics.total = resourceData.memory.total;
    memory_metrics.usage_percent = resourceData.memory.usage_percent;
    memory_metrics.used = resourceData.memory.used;
    
    // 构建Disk指标
    DiskMetrics disk_metrics;
    disk_metrics.disk_count = static_cast<int>(resourceData.disks.size());
    disk_metrics.timestamp = current_timestamp;
    if (!resourceData.disks.empty() && resourceData.disks[0].timestamp > 0) {
        disk_metrics.timestamp = resourceData.disks[0].timestamp / 1000;
    }
    
    for (const auto& disk : resourceData.disks) {
        DiskInfo disk_info;
        disk_info.device = disk.device;
        disk_info.free = disk.free;
        disk_info.mount_point = disk.mount_point;
        disk_info.total = disk.total;
        disk_info.usage_percent = disk.usage_percent;
        disk_info.used = disk.used;
        disk_info.timestamp = disk.timestamp / 1000;
        disk_metrics.disks.push_back(disk_info);
    }
    
    // 构建Network指标
    NetworkMetrics network_metrics;
    network_metrics.network_count = static_cast<int>(resourceData.networks.size());
    network_metrics.timestamp = current_timestamp;
    if (!resourceData.networks.empty() && resourceData.networks[0].timestamp > 0) {
        network_metrics.timestamp = resourceData.networks[0].timestamp / 1000;
    }
    
    for (const auto& network : resourceData.networks) {
        NetworkInfo network_info;
        network_info.interface = network.interface;
        network_info.rx_bytes = network.rx_bytes;
        network_info.rx_errors = network.rx_errors;
        network_info.rx_packets = network.rx_packets;
        network_info.tx_bytes = network.tx_bytes;
        network_info.tx_errors = network.tx_errors;
        network_info.tx_packets = network.tx_packets;
        network_info.tx_rate = network.tx_rate;
        network_info.rx_rate = network.rx_rate;
        network_info.timestamp = network.timestamp / 1000;
        network_metrics.networks.push_back(network_info);
    }
    
    // 构建GPU指标
    GPUMetrics gpu_metrics;
    gpu_metrics.gpu_count = static_cast<int>(resourceData.gpus.size());
    gpu_metrics.timestamp = current_timestamp;
    if (!resourceData.gpus.empty() && resourceData.gpus[0].timestamp > 0) {
        gpu_metrics.timestamp = resourceData.gpus[0].timestamp / 1000;
    }
    
    for (const auto& gpu : resourceData.gpus) {
        GPUInfo gpu_info;
        gpu_info.compute_usage = gpu.compute_usage;
        gpu_info.current = 0.0; // Not available in current schema
        gpu_info.index = gpu.index;
        gpu_info.mem_total = gpu.mem_total;
        gpu_info.mem_usage = gpu.mem_usage;
        gpu_info.mem_used = gpu.mem_used;
        gpu_info.name = gpu.name;
        gpu_info.power = gpu.power;
        gpu_info.temperature = gpu.temperature;
        gpu_info.voltage = 0.0; // Not available in current schema
        gpu_info.timestamp = gpu.timestamp / 1000;
        gpu_metrics.gpus.push_back(gpu_info);
    }
    
    // Container指标 
    ContainerMetrics container_metrics;
    container_metrics.container_count = resourceData.container.container_count;
    container_metrics.paused_count = resourceData.container.paused_count;
    container_metrics.running_count = resourceData.container.running_count;
    container_metrics.stopped_count = resourceData.container.stopped_count;
    container_metrics.timestamp = resourceData.container.timestamp / 1000;
    
    // Sensor指标
    SensorMetrics sensor_metrics;
    sensor_metrics.sensor_count = static_cast<int>(resourceData.sensors.size());
    sensor_metrics.timestamp = current_timestamp;
    if (!resourceData.sensors.empty() && resourceData.sensors[0].timestamp > 0) {
        sensor_metrics.timestamp = resourceData.sensors[0].timestamp / 1000;
    }
    
    for (const auto& sensor : resourceData.sensors) {
        SensorInfo sensor_info;
        sensor_info.sequence = sensor.sequence;
        sensor_info.type = sensor.type;
        sensor_info.name = sensor.name;
        sensor_info.value = sensor.value;
        sensor_info.alarm_type = sensor.alarm_type;
        sensor_info.timestamp = sensor.timestamp / 1000;
        sensor_metrics.sensors.push_back(sensor_info);
    }

    // 计算节点状态：使用与NodeStorage一致的steady_clock时间源进行差值计算
    auto delta_ms = steady_now_ms - node->last_heartbeat;
    if (delta_ms < 0) {
        delta_ms = 0; // 保护：避免负值导致错误判断
    }
    auto time_diff = static_cast<long long>(delta_ms / 1000);
    // 将steady时间差投影到system_clock，得到近似的最近更新时间（秒）
    auto node_updated_at = current_timestamp - time_diff;
    std::string node_status = (time_diff <= 20) ? "online" : "offline";
    
    // 构建节点数据
    NodeMetricsData node_data;
    node_data.board_type = node->board_type;
    node_data.box_id = node->box_id;
    node_data.box_type = node->box_type;
    node_data.cpu_arch = node->cpu_arch;
    node_data.cpu_id = node->cpu_id;
    node_data.cpu_type = node->cpu_type;
    node_data.updated_at = node_updated_at;
    node_data.host_ip = node->host_ip;
    node_data.hostname = node->hostname;
    node_data.id = node->box_id;
    node_data.latest_cpu_metrics = cpu_metrics;
    node_data.latest_disk_metrics = disk_metrics;
    node_data.latest_container_metrics = container_metrics;
    node_data.latest_gpu_metrics = gpu_metrics;
    node_data.latest_memory_metrics = memory_metrics;
    node_data.latest_network_metrics = network_metrics;
    node_data.latest_sensor_metrics = sensor_metrics;
    node_data.os_type = node->os_type;
    node_data.resource_type = node->resource_type;
    node_data.service_port = node->service_port;
    node_data.slot_id = node->slot_id;
    node_data.srio_id = node->srio_id;
    node_data.status = node_status;
    
    return node_data;
}

NodeMetricsDataListPagination ResourceManager::getPaginatedCurrentMetrics(int page, int page_size) {
    NodeMetricsDataListPagination response;
    response.pagination.page = page;
    response.pagination.page_size = page_size;  
    
    if (!m_node_storage || !m_resource_storage) {
        response.success = false;
        response.error_message = "Storage components not available";
        LogManager::getLogger()->error("ResourceManager: Storage components not available for paginated current metrics request");
        return response;
    }
    
    // 验证参数
    if (page < 1) page = 1;
    if (page_size < 1) page_size = 20;
    if (page_size > 1000) page_size = 1000; // 限制最大页面大小
    
    response.pagination.page = page;
    response.pagination.page_size = page_size;
    
    try {
        auto node_list = m_node_storage->getAllNodes();
        response.pagination.total_count = node_list.size();
        
        // 计算总页数
        response.pagination.total_pages = (response.pagination.total_count + page_size - 1) / page_size;
        
        // 设置分页状态
        response.pagination.has_prev = page > 1;
        response.pagination.has_next = page < response.pagination.total_pages;
        
        // 如果没有数据，直接返回
        if (response.pagination.total_count == 0) {
            response.data.nodes_metrics.clear();
            response.success = true;
            return response;
        }
        
        // 计算起始和结束索引
        int start_index = (page - 1) * page_size;
        int end_index = std::min(start_index + page_size, static_cast<int>(node_list.size()));
        
        std::vector<NodeMetricsData> nodes_metrics;
        
        for (int i = start_index; i < end_index; ++i) {
            const auto& node_ptr = node_list[i];
            NodeMetricsData node_metrics_data = buildNodeMetricsData(node_ptr);
            nodes_metrics.push_back(node_metrics_data);
        }
        
        // 构建响应数据结构
        response.data.nodes_metrics = nodes_metrics;
        response.success = true;
        
        LogManager::getLogger()->debug("ResourceManager: Successfully retrieved paginated current metrics for page {}/{} ({} out of {} nodes)", 
                                     response.pagination.page, response.pagination.total_pages, response.data.nodes_metrics.size(), response.pagination.total_count);
        
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = "Failed to retrieve paginated current metrics: " + std::string(e.what());
        LogManager::getLogger()->error("ResourceManager: Exception in getPaginatedCurrentMetrics: {}", e.what());
    }
    
    return response;
}

std::vector<std::shared_ptr<NodeData>> ResourceManager::getNodesList() {
    if (!m_node_storage) {
        LogManager::getLogger()->error("ResourceManager: Node storage not available for nodes list request");
        return {};
    }
    
    try {
        auto node_list = m_node_storage->getAllNodes();
        LogManager::getLogger()->debug("ResourceManager: Successfully retrieved {} nodes data", node_list.size());
        return node_list;
        
    } catch (const std::exception& e) {
        LogManager::getLogger()->error("ResourceManager: Exception in getNodesList: {}", e.what());
        return {};
    }
}

std::shared_ptr<NodeData> ResourceManager::getNode(const std::string& host_ip) {
    
    if (host_ip.empty()) {
        LogManager::getLogger()->warn("ResourceManager: Empty host_ip provided to getNode");
        return nullptr;
    }
    
    try {
        auto node = m_node_storage->getNodeData(host_ip);
        
        if (!node) {
            LogManager::getLogger()->warn("ResourceManager: Node not found for host_ip: {}", host_ip);
            return nullptr;
        }
        
        LogManager::getLogger()->debug("ResourceManager: Successfully retrieved node data for host_ip: {}", host_ip);
        return node;
        
    } catch (const std::exception& e) {
        LogManager::getLogger()->error("ResourceManager: Exception in getNode: {}", e.what());
        return nullptr;
    }
}