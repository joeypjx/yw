#include "resource_manager.h"
#include "log_manager.h"
#include <sstream>
#include <algorithm>

using json = nlohmann::json;

ResourceManager::ResourceManager(std::shared_ptr<ResourceStorage> resource_storage, 
                                 std::shared_ptr<NodeStorage> node_storage)
    : m_resource_storage(resource_storage), m_node_storage(node_storage) {
}

HistoricalMetricsResponse ResourceManager::getHistoricalMetrics(const HistoricalMetricsRequest& request) {
    HistoricalMetricsResponse response;
    
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
        auto nodes = m_node_storage->getAllNodes();
        int box_id = 0;
        int cpu_id = 1;
        int slot_id = 0;
        
        for (const auto& node : nodes) {
            if (node->host_ip == request.host_ip) {
                box_id = node->box_id;
                cpu_id = node->cpu_id;
                slot_id = node->slot_id;
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
            NodeResourceRangeData::TimeSeriesData newTS;
            newTS.metric_type = ts.metric_type;
            
            if (ts.metric_type == "cpu" || ts.metric_type == "memory") {
                // CPU和Memory是单一数据源，直接转换
                for (const auto& point : ts.data_points) {
                    QueryResult newPoint = point;
                    // 转换时间戳为秒
                    auto timestamp_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                        point.timestamp.time_since_epoch()).count();
                    newPoint.metrics["timestamp"] = static_cast<double>(timestamp_seconds);
                    newTS.data_points.push_back(newPoint);
                }
            } else if (ts.metric_type == "disk" || ts.metric_type == "network" || ts.metric_type == "gpu") {
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
                    }
                    
                    QueryResult newPoint = point;
                    // 转换时间戳为秒
                    auto timestamp_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                        point.timestamp.time_since_epoch()).count();
                    newPoint.metrics["timestamp"] = static_cast<double>(timestamp_seconds);
                    
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

std::vector<std::string> ResourceManager::parseMetricsParam(const std::string& metrics_param) {
    std::vector<std::string> metrics;
    
    if (metrics_param.empty()) {
        // 默认获取所有指标类型
        return {"cpu", "memory", "disk", "network", "gpu"};
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
    const std::vector<std::string> valid_metrics = {"cpu", "memory", "disk", "network", "gpu"};
    for (const auto& metric : request.metrics) {
        if (std::find(valid_metrics.begin(), valid_metrics.end(), metric) == valid_metrics.end()) {
            return {false, "Invalid metric type: " + metric + ". Valid types are: cpu, memory, disk, network, gpu"};
        }
    }
    
    // time_range在这里不做详细验证，由TDengine处理
    
    return {true, ""};
}

nlohmann::json ResourceManager::formatResponse(const HistoricalMetricsResponse& response) {
    json result;
    
    if (response.success) {
        result = {
            {"api_version", 1},
            {"status", "success"},
            {"data", {
                {"historical_metrics", response.data.to_json()}
            }}
        };
    } else {
        result = {
            {"api_version", 1},
            {"status", "error"},
            {"error", response.error_message}
        };
    }
    
    return result;
}

NodeMetricsResponse ResourceManager::getCurrentMetrics() {
    NodeMetricsResponse response;
    
    if (!m_node_storage || !m_resource_storage) {
        response.success = false;
        response.error_message = "Storage components not available";
        LogManager::getLogger()->error("ResourceManager: Storage components not available for current metrics request");
        return response;
    }
    
    try {
        auto nodes = m_node_storage->getAllNodes();
        json nodes_metrics = json::array();
        
        for (const auto& node : nodes) {
            std::string host_ip = node->host_ip;
            auto current_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // 使用 getNodeResourceData 方法获取所有资源数据
            auto resourceData = m_resource_storage->getNodeResourceData(host_ip);

            // 构建CPU指标
            json latest_cpu_metrics = {
                {"core_allocated", resourceData.cpu.core_allocated},
                {"core_count", resourceData.cpu.core_count},
                {"current", resourceData.cpu.current},
                {"load_avg_15m", resourceData.cpu.load_avg_15m},
                {"load_avg_1m", resourceData.cpu.load_avg_1m},
                {"load_avg_5m", resourceData.cpu.load_avg_5m},
                {"power", resourceData.cpu.power},
                {"temperature", resourceData.cpu.temperature},
                {"timestamp", resourceData.cpu.has_data ? 
                    std::chrono::duration_cast<std::chrono::seconds>(resourceData.cpu.timestamp.time_since_epoch()).count() : current_timestamp},
                {"usage_percent", resourceData.cpu.usage_percent},
                {"voltage", resourceData.cpu.voltage}
            };
            
            // 构建Memory指标
            json latest_memory_metrics = {
                {"free", resourceData.memory.free},
                {"timestamp", resourceData.memory.has_data ? 
                    std::chrono::duration_cast<std::chrono::seconds>(resourceData.memory.timestamp.time_since_epoch()).count() : current_timestamp},
                {"total", resourceData.memory.total},
                {"usage_percent", resourceData.memory.usage_percent},
                {"used", resourceData.memory.used}
            };
            
            // 构建Disk指标
            json disks = json::array();
            long long disk_timestamp = current_timestamp;
            if (!resourceData.disks.empty() && resourceData.disks[0].timestamp.time_since_epoch().count() > 0) {
                disk_timestamp = std::chrono::duration_cast<std::chrono::seconds>(resourceData.disks[0].timestamp.time_since_epoch()).count();
            }
            for (const auto& disk : resourceData.disks) {
                disks.push_back({
                    {"device", disk.device},
                    {"free", disk.free},
                    {"mount_point", disk.mount_point},
                    {"total", disk.total},
                    {"usage_percent", disk.usage_percent},
                    {"used", disk.used},
                    {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(disk.timestamp.time_since_epoch()).count()}
                });
            }
            json latest_disk_metrics = {
                {"disk_count", static_cast<int>(resourceData.disks.size())},
                {"disks", disks},
                {"timestamp", disk_timestamp}
            };
            
            // 构建Network指标
            json networks = json::array();
            long long network_timestamp = current_timestamp;
            if (!resourceData.networks.empty() && resourceData.networks[0].timestamp.time_since_epoch().count() > 0) {
                network_timestamp = std::chrono::duration_cast<std::chrono::seconds>(resourceData.networks[0].timestamp.time_since_epoch()).count();
            }
            for (const auto& network : resourceData.networks) {
                networks.push_back({
                    {"interface", network.interface},
                    {"rx_bytes", network.rx_bytes},
                    {"rx_errors", network.rx_errors},
                    {"rx_packets", network.rx_packets},
                    {"tx_bytes", network.tx_bytes},
                    {"tx_errors", network.tx_errors},
                    {"tx_packets", network.tx_packets},
                    {"tx_rate", network.tx_rate},
                    {"rx_rate", network.rx_rate},
                    {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(network.timestamp.time_since_epoch()).count()}
                });
            }
            json latest_network_metrics = {
                {"network_count", static_cast<int>(resourceData.networks.size())},
                {"networks", networks},
                {"timestamp", network_timestamp}
            };
            
            // 构建GPU指标
            json gpus = json::array();
            long long gpu_timestamp = current_timestamp;
            if (!resourceData.gpus.empty() && resourceData.gpus[0].timestamp.time_since_epoch().count() > 0) {
                gpu_timestamp = std::chrono::duration_cast<std::chrono::seconds>(resourceData.gpus[0].timestamp.time_since_epoch()).count();
            }
            for (const auto& gpu : resourceData.gpus) {
                gpus.push_back({
                    {"compute_usage", gpu.compute_usage},
                    {"current", 0.0}, // Not available in current schema
                    {"index", gpu.index},
                    {"mem_total", gpu.mem_total},
                    {"mem_usage", gpu.mem_usage},
                    {"mem_used", gpu.mem_used},
                    {"name", gpu.name},
                    {"power", gpu.power},
                    {"temperature", gpu.temperature},
                    {"voltage", 0.0}, // Not available in current schema
                    {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(gpu.timestamp.time_since_epoch()).count()}
                });
            }
            json latest_gpu_metrics = {
                {"gpu_count", static_cast<int>(resourceData.gpus.size())},
                {"gpus", gpus},
                {"timestamp", gpu_timestamp}
            };
            
            // Docker指标 (暂时使用默认值，因为当前没有docker表)
            json latest_docker_metrics = {
                {"component", json::array()},
                {"container_count", 0},
                {"paused_count", 0},
                {"running_count", 0},
                {"stopped_count", 0},
                {"timestamp", current_timestamp}
            };
            
            // 计算节点状态：如果updated_at与当前时间差距大于5秒，则判断为离线
            auto node_updated_at = std::chrono::duration_cast<std::chrono::seconds>(node->last_heartbeat.time_since_epoch()).count();
            auto time_diff = current_timestamp - node_updated_at;
            std::string node_status = (time_diff <= 5) ? "online" : "offline";
            
            // 构建节点数据
            json node_data = {
                {"board_type", node->board_type},
                {"box_id", node->box_id},
                {"box_type", node->box_type},
                {"cpu_arch", node->cpu_arch},
                {"cpu_id", node->cpu_id},
                {"cpu_type", node->cpu_type},
                {"updated_at", node_updated_at},
                {"gpu", node->gpu},
                {"host_ip", node->host_ip},
                {"hostname", node->hostname},
                {"id", node->box_id},
                {"latest_cpu_metrics", latest_cpu_metrics},
                {"latest_disk_metrics", latest_disk_metrics},
                {"latest_docker_metrics", latest_docker_metrics},
                {"latest_gpu_metrics", latest_gpu_metrics},
                {"latest_memory_metrics", latest_memory_metrics},
                {"latest_network_metrics", latest_network_metrics},
                {"os_type", node->os_type},
                {"resource_type", node->resource_type},
                {"service_port", node->service_port},
                {"slot_id", node->slot_id},
                {"srio_id", node->srio_id},
                {"status", node_status}
            };
            
            nodes_metrics.push_back(node_data);
        }
        
        response.data = {
            {"api_version", 1},
            {"data", {
                {"nodes_metrics", nodes_metrics}
            }},
            {"status", "success"}
        };
        
        response.success = true;
        
        LogManager::getLogger()->debug("ResourceManager: Successfully retrieved current metrics for {} nodes", nodes.size());
        
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = "Failed to retrieve current metrics: " + std::string(e.what());
        LogManager::getLogger()->error("ResourceManager: Exception in getCurrentMetrics: {}", e.what());
    }
    
    return response;
}

PaginatedNodeMetricsResponse ResourceManager::getPaginatedCurrentMetrics(int page, int page_size) {
    PaginatedNodeMetricsResponse response;
    response.page = page;
    response.page_size = page_size;
    
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
    
    response.page = page;
    response.page_size = page_size;
    
    try {
        auto nodes = m_node_storage->getAllNodes();
        response.total_count = nodes.size();
        
        // 计算总页数
        response.total_pages = (response.total_count + page_size - 1) / page_size;
        
        // 设置分页状态
        response.has_prev = page > 1;
        response.has_next = page < response.total_pages;
        
        // 如果没有数据，直接返回
        if (response.total_count == 0) {
            response.data = json::array();
            response.success = true;
            return response;
        }
        
        // 计算起始和结束索引
        int start_index = (page - 1) * page_size;
        int end_index = std::min(start_index + page_size, static_cast<int>(nodes.size()));
        
        json nodes_metrics = json::array();
        
        for (int i = start_index; i < end_index; ++i) {
            const auto& node = nodes[i];
            std::string host_ip = node->host_ip;
            auto current_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            // 使用 getNodeResourceData 方法获取所有资源数据
            auto resourceData = m_resource_storage->getNodeResourceData(host_ip);

            // 构建CPU指标
            json latest_cpu_metrics = {
                {"core_allocated", resourceData.cpu.core_allocated},
                {"core_count", resourceData.cpu.core_count},
                {"current", resourceData.cpu.current},
                {"load_avg_15m", resourceData.cpu.load_avg_15m},
                {"load_avg_1m", resourceData.cpu.load_avg_1m},
                {"load_avg_5m", resourceData.cpu.load_avg_5m},
                {"power", resourceData.cpu.power},
                {"temperature", resourceData.cpu.temperature},
                {"timestamp", resourceData.cpu.has_data ? 
                    std::chrono::duration_cast<std::chrono::seconds>(resourceData.cpu.timestamp.time_since_epoch()).count() : current_timestamp},
                {"usage_percent", resourceData.cpu.usage_percent},
                {"voltage", resourceData.cpu.voltage}
            };
            
            // 构建Memory指标
            json latest_memory_metrics = {
                {"free", resourceData.memory.free},
                {"timestamp", resourceData.memory.has_data ? 
                    std::chrono::duration_cast<std::chrono::seconds>(resourceData.memory.timestamp.time_since_epoch()).count() : current_timestamp},
                {"total", resourceData.memory.total},
                {"usage_percent", resourceData.memory.usage_percent},
                {"used", resourceData.memory.used}
            };
            
            // 构建Disk指标
            json disks = json::array();
            long long disk_timestamp = current_timestamp;
            if (!resourceData.disks.empty() && resourceData.disks[0].timestamp.time_since_epoch().count() > 0) {
                disk_timestamp = std::chrono::duration_cast<std::chrono::seconds>(resourceData.disks[0].timestamp.time_since_epoch()).count();
            }
            for (const auto& disk : resourceData.disks) {
                disks.push_back({
                    {"device", disk.device},
                    {"free", disk.free},
                    {"mount_point", disk.mount_point},
                    {"total", disk.total},
                    {"usage_percent", disk.usage_percent},
                    {"used", disk.used},
                    {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(disk.timestamp.time_since_epoch()).count()}
                });
            }
            json latest_disk_metrics = {
                {"disk_count", static_cast<int>(resourceData.disks.size())},
                {"disks", disks},
                {"timestamp", disk_timestamp}
            };
            
            // 构建Network指标
            json networks = json::array();
            long long network_timestamp = current_timestamp;
            if (!resourceData.networks.empty() && resourceData.networks[0].timestamp.time_since_epoch().count() > 0) {
                network_timestamp = std::chrono::duration_cast<std::chrono::seconds>(resourceData.networks[0].timestamp.time_since_epoch()).count();
            }
            for (const auto& network : resourceData.networks) {
                networks.push_back({
                    {"interface", network.interface},
                    {"rx_bytes", network.rx_bytes},
                    {"rx_errors", network.rx_errors},
                    {"rx_packets", network.rx_packets},
                    {"tx_bytes", network.tx_bytes},
                    {"tx_errors", network.tx_errors},
                    {"tx_packets", network.tx_packets},
                    {"tx_rate", network.tx_rate},
                    {"rx_rate", network.rx_rate},
                    {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(network.timestamp.time_since_epoch()).count()}
                });
            }
            json latest_network_metrics = {
                {"network_count", static_cast<int>(resourceData.networks.size())},
                {"networks", networks},
                {"timestamp", network_timestamp}
            };
            
            // 构建GPU指标
            json gpus = json::array();
            long long gpu_timestamp = current_timestamp;
            if (!resourceData.gpus.empty() && resourceData.gpus[0].timestamp.time_since_epoch().count() > 0) {
                gpu_timestamp = std::chrono::duration_cast<std::chrono::seconds>(resourceData.gpus[0].timestamp.time_since_epoch()).count();
            }
            for (const auto& gpu : resourceData.gpus) {
                gpus.push_back({
                    {"compute_usage", gpu.compute_usage},
                    {"current", 0.0}, // Not available in current schema
                    {"index", gpu.index},
                    {"mem_total", gpu.mem_total},
                    {"mem_usage", gpu.mem_usage},
                    {"mem_used", gpu.mem_used},
                    {"name", gpu.name},
                    {"power", gpu.power},
                    {"temperature", gpu.temperature},
                    {"voltage", 0.0}, // Not available in current schema
                    {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(gpu.timestamp.time_since_epoch()).count()}
                });
            }
            json latest_gpu_metrics = {
                {"gpu_count", static_cast<int>(resourceData.gpus.size())},
                {"gpus", gpus},
                {"timestamp", gpu_timestamp}
            };
            
            // Docker指标 (暂时使用默认值，因为当前没有docker表)
            json latest_docker_metrics = {
                {"component", json::array()},
                {"container_count", 0},
                {"paused_count", 0},
                {"running_count", 0},
                {"stopped_count", 0},
                {"timestamp", current_timestamp}
            };
            
            // 计算节点状态：如果updated_at与当前时间差距大于5秒，则判断为离线
            auto node_updated_at = std::chrono::duration_cast<std::chrono::seconds>(node->last_heartbeat.time_since_epoch()).count();
            auto time_diff = current_timestamp - node_updated_at;
            std::string node_status = (time_diff <= 5) ? "online" : "offline";
            
            // 构建节点数据
            json node_data = {
                {"board_type", node->board_type},
                {"box_id", node->box_id},
                {"box_type", node->box_type},
                {"cpu_arch", node->cpu_arch},
                {"cpu_id", node->cpu_id},
                {"cpu_type", node->cpu_type},
                {"updated_at", node_updated_at},
                {"gpu", node->gpu},
                {"host_ip", node->host_ip},
                {"hostname", node->hostname},
                {"id", node->box_id},
                {"latest_cpu_metrics", latest_cpu_metrics},
                {"latest_disk_metrics", latest_disk_metrics},
                {"latest_docker_metrics", latest_docker_metrics},
                {"latest_gpu_metrics", latest_gpu_metrics},
                {"latest_memory_metrics", latest_memory_metrics},
                {"latest_network_metrics", latest_network_metrics},
                {"os_type", node->os_type},
                {"resource_type", node->resource_type},
                {"service_port", node->service_port},
                {"slot_id", node->slot_id},
                {"srio_id", node->srio_id},
                {"status", node_status}
            };
            
            nodes_metrics.push_back(node_data);
        }
        
        response.data = nodes_metrics;
        response.success = true;
        
        LogManager::getLogger()->debug("ResourceManager: Successfully retrieved paginated current metrics for page {}/{} ({} out of {} nodes)", 
                                     response.page, response.total_pages, end_index - start_index, response.total_count);
        
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = "Failed to retrieve paginated current metrics: " + std::string(e.what());
        LogManager::getLogger()->error("ResourceManager: Exception in getPaginatedCurrentMetrics: {}", e.what());
    }
    
    return response;
}

NodesListResponse ResourceManager::getNodesList() {
    NodesListResponse response;
    
    if (!m_node_storage) {
        response.success = false;
        response.error_message = "Node storage not available";
        LogManager::getLogger()->error("ResourceManager: Node storage not available for nodes list request");
        return response;
    }
    
    try {
        auto nodes = m_node_storage->getAllNodes();
        
        json nodes_json = json::array();
        for (const auto& node : nodes) {
            // Calculate time since last heartbeat
            auto now = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - node->last_heartbeat);
            
            // 计算节点状态：如果updated_at与当前时间差距大于5秒，则判断为离线
            std::string node_status = (duration.count() <= 5) ? "online" : "offline";
            
            json node_json = {
                {"api_version", node->api_version},
                {"data", {
                    {"box_id", node->box_id},
                    {"slot_id", node->slot_id},
                    {"cpu_id", node->cpu_id},
                    {"srio_id", node->srio_id},
                    {"host_ip", node->host_ip},
                    {"hostname", node->hostname},
                    {"service_port", node->service_port},
                    {"box_type", node->box_type},
                    {"board_type", node->board_type},
                    {"cpu_type", node->cpu_type},
                    {"os_type", node->os_type},
                    {"resource_type", node->resource_type},
                    {"cpu_arch", node->cpu_arch},
                    {"gpu", node->gpu}
                }},
                {"last_heartbeat", std::chrono::duration_cast<std::chrono::milliseconds>(
                    node->last_heartbeat.time_since_epoch()).count()},
                {"seconds_since_last_heartbeat", duration.count()},
                {"status", node_status}
            };
            nodes_json.push_back(node_json);
        }
        
        response.data = {
            {"total_nodes", nodes.size()},
            {"nodes", nodes_json}
        };
        
        response.success = true;
        
        LogManager::getLogger()->debug("ResourceManager: Successfully retrieved {} nodes data", nodes.size());
        
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = "Failed to retrieve nodes data: " + std::string(e.what());
        LogManager::getLogger()->error("ResourceManager: Exception in getNodesList: {}", e.what());
    }
    
    return response;
}