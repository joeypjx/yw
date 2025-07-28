#pragma once

#include "resource_storage.h"
#include "node_storage.h"
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
public:
    ResourceManager(std::shared_ptr<ResourceStorage> resource_storage, 
                   std::shared_ptr<NodeStorage> node_storage);
    ~ResourceManager() = default;

    /**
     * @brief 获取节点历史指标数据
     * @param request 请求参数
     * @return 历史指标响应
     */
    HistoricalMetricsResponse getHistoricalMetrics(const HistoricalMetricsRequest& request);

    /**
     * @brief 获取所有节点的当前指标数据
     * @return 节点指标响应
     */
    NodeMetricsResponse getCurrentMetrics();
    
    /**
     * @brief 获取分页的节点指标数据
     * @param page 页码 (从1开始)
     * @param page_size 每页大小
     * @return 分页节点指标响应
     */
    PaginatedNodeMetricsResponse getPaginatedCurrentMetrics(int page = 1, int page_size = 20);

    /**
     * @brief 获取所有节点列表数据
     * @return 节点列表响应
     */
    NodesListResponse getNodesList();

    /**
     * @brief 获取指定节点的数据
     * @param host_ip 节点的IP地址
     * @return 节点响应
     */
    NodeResponse getNode(const std::string& host_ip);

    /**
     * @brief 解析metrics参数字符串为vector
     * @param metrics_param 逗号分隔的指标类型字符串
     * @return 指标类型vector
     */
    std::vector<std::string> parseMetricsParam(const std::string& metrics_param);

    /**
     * @brief 验证请求参数
     * @param request 请求参数
     * @return 验证结果，如果失败返回错误消息
     */
    std::pair<bool, std::string> validateRequest(const HistoricalMetricsRequest& request);

    /**
     * @brief 将历史数据转换为JSON响应格式
     * @param response 历史指标响应
     * @return JSON格式的响应
     */
    nlohmann::json formatResponse(const HistoricalMetricsResponse& response);

private:
    std::shared_ptr<ResourceStorage> m_resource_storage;
    std::shared_ptr<NodeStorage> m_node_storage;
};