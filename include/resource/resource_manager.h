#pragma once

#include "resource_storage.h"
#include "node_storage.h"
#include "bmc_storage.h"
#include "json.hpp"
#include <string>
#include <vector>
#include <memory>

struct HistoricalMetricsRequest {
    std::string host_ip;
    std::string time_range;
    std::vector<std::string> metrics;
};

struct HistoricalMetricsResponse {
    bool success = false;
    std::string error_message;
    NodeResourceRangeData data;
};

struct NodeMetricsResponse {
    bool success = false;
    std::string error_message;
    nlohmann::json data;
};

// 分页节点指标响应结构
struct PaginatedNodeMetricsResponse {
    bool success = false;
    std::string error_message;
    nlohmann::json data;             // 节点指标数据数组
    int total_count = 0;             // 总记录数
    int page = 1;                    // 当前页码 (从1开始)
    int page_size = 20;              // 每页大小
    int total_pages = 0;             // 总页数
    bool has_next = false;           // 是否有下一页
    bool has_prev = false;           // 是否有上一页
};

struct NodesListResponse {
    bool success = false;
    std::string error_message;
    nlohmann::json data;
};

struct NodeResponse {
    bool success = false;
    std::string error_message;
    nlohmann::json data;
};

class ResourceManager {
private:
    std::shared_ptr<ResourceStorage> m_resource_storage;
    std::shared_ptr<NodeStorage> m_node_storage;
    std::shared_ptr<BMCStorage> m_bmc_storage;

    // 私有方法
    std::pair<bool, std::string> validateRequest(const HistoricalMetricsRequest& request);
    nlohmann::json convertNodeToJson(const std::shared_ptr<NodeData>& node);

public:
    ResourceManager(std::shared_ptr<ResourceStorage> resource_storage, 
                   std::shared_ptr<NodeStorage> node_storage,
                   std::shared_ptr<BMCStorage> bmc_storage);
    
    // 历史指标查询
    HistoricalMetricsResponse getHistoricalMetrics(const HistoricalMetricsRequest& request);
    
    // 历史BMC数据查询
    HistoricalBMCResponse getHistoricalBMC(const HistoricalBMCRequest& request);
    
    // 指标参数解析
    std::vector<std::string> parseMetricsParam(const std::string& metrics_param);
    
    // 当前指标查询
    NodeMetricsResponse getCurrentMetrics();
    
    // 分页当前指标查询
    PaginatedNodeMetricsResponse getPaginatedCurrentMetrics(int page, int page_size);
    
    // 节点列表查询
    NodesListResponse getNodesList();
    
    // 单个节点查询
    NodeResponse getNode(const std::string& host_ip);
    
    // 将历史数据转换为JSON响应格式
    nlohmann::json formatResponse(const HistoricalMetricsResponse& response);
};