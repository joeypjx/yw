#include "http_server.h"
#include "log_manager.h"
#include "json.hpp"
#include <iostream>
#include <regex>
#include <tuple>

using json = nlohmann::json;

HttpServer::HttpServer(std::shared_ptr<ResourceStorage> resource_storage,
                       std::shared_ptr<AlarmRuleStorage> alarm_rule_storage,
                       const std::string& host, int port)
    : m_resource_storage(resource_storage), m_alarm_rule_storage(alarm_rule_storage), m_host(host), m_port(port) {
    setup_routes();
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::setup_routes() {
    m_server.Post("/resource", [this](const httplib::Request& req, httplib::Response& res) {
        this->handle_resource(req, res);
    });
    
    m_server.Post("/alarm/rules", [this](const httplib::Request& req, httplib::Response& res) {
        this->handle_alarm_rules(req, res);
    });
}

void HttpServer::handle_resource(const httplib::Request& req, httplib::Response& res) {
    try {
        json body = json::parse(req.body);

        if (!body.contains("data") || !body["data"].is_object()) {
            res.set_content("{\"error\":\"'data' field is missing or not an object\"}", "application/json");
            res.status = 400;
            return;
        }
        const auto& data = body["data"];

        if (!data.contains("host_ip") || !data["host_ip"].is_string()) {
            res.set_content("{\"error\":\"'host_ip' is missing or not a string\"}", "application/json");
            res.status = 400;
            return;
        }
        std::string host_ip = data["host_ip"];

        if (!data.contains("resource") || !data["resource"].is_object()) {
            res.set_content("{\"error\":\"'resource' field is missing or not an object\"}", "application/json");
            res.status = 400;
            return;
        }
        const auto& resource = data["resource"];

        if (m_resource_storage->insertResourceData(host_ip, resource)) {
            res.set_content("{\"status\":\"success\"}", "application/json");
            res.status = 200;
            LogManager::getLogger()->info("Successfully processed resource data for host: {}", host_ip);
        } else {
            res.set_content("{\"error\":\"Failed to store resource data\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Failed to store resource data for host: {}", host_ip);
        }

    } catch (const json::parse_error& e) {
        res.set_content("{\"error\":\"Invalid JSON format\"}", "application/json");
        res.status = 400;
    } catch (const std::exception& e) {
        res.set_content("{\"error\":\"An unexpected error occurred\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_resource: {}", e.what());
    }
}

std::string HttpServer::convertDescriptionTemplate(const std::string& template_str) {
    std::string result = template_str;
    
    // 定义占位符映射关系
    std::map<std::string, std::string> placeholder_mappings = {
        {"nodeId", "host_ip"},
        {"alarmLevel", "severity"},
        {"value", "value"}
    };
    
    // 查找所有{xxx}格式的占位符，并转换为{{mapped_xxx}}
    std::regex pattern(R"(\{([^}]+)\})");
    std::sregex_iterator iter(result.begin(), result.end(), pattern);
    std::sregex_iterator end;
    
    // 存储需要替换的内容（从后往前替换，避免位置偏移）
    std::vector<std::tuple<size_t, size_t, std::string>> replacements;
    
    for (; iter != end; ++iter) {
        std::smatch match = *iter;
        std::string placeholder = match[1].str();
        
        // 查找映射关系
        auto mapping_it = placeholder_mappings.find(placeholder);
        if (mapping_it != placeholder_mappings.end()) {
            // 使用映射后的名称
            std::string replacement = "{{" + mapping_it->second + "}}";
            replacements.push_back(std::make_tuple(match.position(), match.length(), replacement));
        } else {
            // 如果没有映射关系，保持原样但转换为{{}}格式
            std::string replacement = "{{" + placeholder + "}}";
            replacements.push_back(std::make_tuple(match.position(), match.length(), replacement));
        }
    }
    
    // 从后往前替换，避免位置偏移问题
    for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
        result.replace(std::get<0>(*it), std::get<1>(*it), std::get<2>(*it));
    }
    
    return result;
}

json HttpServer::parseMetricNameAndBuildExpression(const std::string& metric_name, const json& condition) {
    json expression;
    
    // 解析metricName格式: resource.stable[tag=value].metric
    // 示例: resource.disk[mount_point=/].usage_percent
    //       resource.cpu.usage_percent
    
    std::string stable;
    std::string metric;
    std::string tag_key;
    std::string tag_value;
    
    // 查找resource.前缀
    size_t resource_pos = metric_name.find("resource.");
    if (resource_pos != 0) {
        throw std::runtime_error("Invalid metric name format: should start with 'resource.'");
    }
    
    std::string remaining = metric_name.substr(9); // 跳过"resource."
    
    // 基本验证
    if (remaining.empty()) {
        throw std::runtime_error("Invalid metric name format: missing stable name");
    }
    
    // 检查是否有标签条件 [tag=value]
    size_t bracket_start = remaining.find('[');
    size_t bracket_end = remaining.find(']');
    
    if (bracket_start != std::string::npos && bracket_end != std::string::npos) {
        // 有标签条件的情况
        if (bracket_end <= bracket_start) {
            throw std::runtime_error("Invalid metric name format: invalid bracket placement");
        }
        
        stable = remaining.substr(0, bracket_start);
        if (stable.empty()) {
            throw std::runtime_error("Invalid metric name format: missing stable name");
        }
        
        // 解析标签
        std::string tag_part = remaining.substr(bracket_start + 1, bracket_end - bracket_start - 1);
        if (tag_part.empty()) {
            throw std::runtime_error("Invalid metric name format: empty tag condition");
        }
        
        size_t eq_pos = tag_part.find('=');
        if (eq_pos == std::string::npos || eq_pos == 0 || eq_pos == tag_part.length() - 1) {
            throw std::runtime_error("Invalid metric name format: invalid tag condition format");
        }
        
        tag_key = tag_part.substr(0, eq_pos);
        tag_value = tag_part.substr(eq_pos + 1);
        
        // 获取metric部分
        if (bracket_end + 1 < remaining.length() && remaining[bracket_end + 1] == '.') {
            metric = remaining.substr(bracket_end + 2);
            if (metric.empty()) {
                throw std::runtime_error("Invalid metric name format: missing metric name");
            }
        } else {
            throw std::runtime_error("Invalid metric name format: missing metric name after tag");
        }
    } else if (bracket_start != std::string::npos || bracket_end != std::string::npos) {
        // 只有一个括号，无效
        throw std::runtime_error("Invalid metric name format: unmatched bracket");
    } else {
        // 没有标签条件的情况
        size_t last_dot = remaining.find_last_of('.');
        if (last_dot != std::string::npos) {
            stable = remaining.substr(0, last_dot);
            metric = remaining.substr(last_dot + 1);
            if (stable.empty() || metric.empty()) {
                throw std::runtime_error("Invalid metric name format: missing stable or metric name");
            }
        } else {
            // 没有metric部分，只有stable
            throw std::runtime_error("Invalid metric name format: missing metric name");
        }
    }
    
    // 构建表达式
    json and_conditions = json::array();
    
    // 如果有标签条件，添加标签过滤条件
    if (!tag_key.empty()) {
        json tag_condition;
        tag_condition["stable"] = stable;
        tag_condition["tag"] = tag_key;
        tag_condition["operator"] = "==";
        tag_condition["threshold"] = tag_value;
        and_conditions.push_back(tag_condition);
    }
    
    // 处理复合条件
    if (condition.contains("conditions") && condition["conditions"].is_array()) {
        // 复合条件：只处理子条件
        for (const auto& sub_condition : condition["conditions"]) {
            json sub_metric_condition;
            sub_metric_condition["stable"] = stable;
            sub_metric_condition["metric"] = metric;
            
            if (sub_condition["type"] == "GreaterThan") {
                sub_metric_condition["operator"] = ">";
            } else if (sub_condition["type"] == "LessThan") {
                sub_metric_condition["operator"] = "<";
            } else if (sub_condition["type"] == "Equal") {
                sub_metric_condition["operator"] = "==";
            } else if (sub_condition["type"] == "GreaterThanOrEqual") {
                sub_metric_condition["operator"] = ">=";
            } else if (sub_condition["type"] == "LessThanOrEqual") {
                sub_metric_condition["operator"] = "<=";
            }
            
            sub_metric_condition["value"] = sub_condition["threshold"];
            and_conditions.push_back(sub_metric_condition);
        }
    } else {
        // 简单条件：添加metric条件
        json metric_condition;
        metric_condition["stable"] = stable;
        metric_condition["metric"] = metric;
        
        // 转换condition类型
        if (condition["type"] == "GreaterThan") {
            metric_condition["operator"] = ">";
        } else if (condition["type"] == "LessThan") {
            metric_condition["operator"] = "<";
        } else if (condition["type"] == "Equal") {
            metric_condition["operator"] = "==";
        } else if (condition["type"] == "GreaterThanOrEqual") {
            metric_condition["operator"] = ">=";
        } else if (condition["type"] == "LessThanOrEqual") {
            metric_condition["operator"] = "<=";
        } else {
            metric_condition["operator"] = ">";
        }
        
        metric_condition["value"] = condition["threshold"];
        and_conditions.push_back(metric_condition);
    }
    
    if (and_conditions.size() == 1) {
        expression = and_conditions[0];
    } else {
        expression["and"] = and_conditions;
    }
    
    return expression;
}

void HttpServer::handle_alarm_rules(const httplib::Request& req, httplib::Response& res) {
    try {
        json body = json::parse(req.body);
        
        // 验证必填字段
        if (!body.contains("templateId") || !body["templateId"].is_string()) {
            res.set_content("{\"error\":\"'templateId' is missing or not a string\"}", "application/json");
            res.status = 400;
            return;
        }
        
        if (!body.contains("metricName") || !body["metricName"].is_string()) {
            res.set_content("{\"error\":\"'metricName' is missing or not a string\"}", "application/json");
            res.status = 400;
            return;
        }
        
        if (!body.contains("alarmLevel") || !body["alarmLevel"].is_string()) {
            res.set_content("{\"error\":\"'alarmLevel' is missing or not a string\"}", "application/json");
            res.status = 400;
            return;
        }
        
        if (!body.contains("triggerCountThreshold") || !body["triggerCountThreshold"].is_number_integer()) {
            res.set_content("{\"error\":\"'triggerCountThreshold' is missing or not an integer\"}", "application/json");
            res.status = 400;
            return;
        }
        
        if (!body.contains("contentTemplate") || !body["contentTemplate"].is_string()) {
            res.set_content("{\"error\":\"'contentTemplate' is missing or not a string\"}", "application/json");
            res.status = 400;
            return;
        }
        
        if (!body.contains("condition") || !body["condition"].is_object()) {
            res.set_content("{\"error\":\"'condition' is missing or not an object\"}", "application/json");
            res.status = 400;
            return;
        }
        
        if (!body.contains("actions") || !body["actions"].is_array()) {
            res.set_content("{\"error\":\"'actions' is missing or not an array\"}", "application/json");
            res.status = 400;
            return;
        }
        
        // 提取字段
        std::string template_id = body["templateId"];
        std::string metric_name = body["metricName"];
        std::string alarm_level = body["alarmLevel"];
        int trigger_count = body["triggerCountThreshold"];
        std::string content_template = body["contentTemplate"];
        json condition = body["condition"];
        json actions = body["actions"];
        
        // 可选字段
        std::string alarm_type = body.value("alarmType", "硬件状态");
        
        // 验证alarmType字段
        if (alarm_type != "硬件状态" && alarm_type != "业务链路" && alarm_type != "系统故障") {
            res.set_content("{\"error\":\"Invalid alarmType. Must be one of: 硬件状态, 业务链路, 系统故障\"}", "application/json");
            res.status = 400;
            return;
        }
        
        // 映射alarmLevel到severity
        std::string severity;
        if (alarm_level == "严重") {
            severity = "critical";
        } else if (alarm_level == "一般") {
            severity = "warning";
        } else if (alarm_level == "提示") {
            severity = "info";
        } else {
            severity = alarm_level; // 直接使用原值
        }
        
        // 解析metricName并构建内部表达式格式
        json expression;
        try {
            expression = parseMetricNameAndBuildExpression(metric_name, condition);
            // 不在expression中包含actions和trigger_count信息
        } catch (const std::exception& e) {
            res.set_content("{\"error\":\"Invalid metric name format: " + std::string(e.what()) + "\"}", "application/json");
            res.status = 400;
            return;
        }
        
        // 计算for_duration (trigger_count * 5秒)
        std::string for_duration = std::to_string(trigger_count * 5) + "s";
        
        // 转换description中的{}为{{}}格式
        std::string converted_description = convertDescriptionTemplate(content_template);
        
        // 调用存储函数
        std::string rule_id = m_alarm_rule_storage->insertAlarmRule(
            template_id,           // alert_name
            expression,            // expression_json
            for_duration,          // for_duration
            severity,              // severity
            template_id,           // summary
            converted_description, // description
            alarm_type,            // alarm_type
            true                   // enabled
        );
        
        if (!rule_id.empty()) {
            json response;
            response["status"] = "success";
            response["rule_id"] = rule_id;
            res.set_content(response.dump(), "application/json");
            res.status = 200;
            LogManager::getLogger()->info("Successfully created alarm rule: {}", template_id);
        } else {
            res.set_content("{\"error\":\"Failed to create alarm rule\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Failed to create alarm rule: {}", template_id);
        }
        
    } catch (const json::parse_error& e) {
        res.set_content("{\"error\":\"Invalid JSON format\"}", "application/json");
        res.status = 400;
    } catch (const std::exception& e) {
        res.set_content("{\"error\":\"An unexpected error occurred\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_alarm_rules: {}", e.what());
    }
}

bool HttpServer::start() {
    if (m_server.is_running()) {
        return true;
    }
    
    m_server_thread = std::thread([this]() {
        LogManager::getLogger()->info("HTTP server starting on {}:{}", m_host, m_port);
        if (!m_server.listen(m_host.c_str(), m_port)) {
            LogManager::getLogger()->error("HTTP server failed to start.");
        }
    });

    // Give the server a moment to start up
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return m_server.is_running();
}

void HttpServer::stop() {
    if (m_server.is_running()) {
        m_server.stop();
        if (m_server_thread.joinable()) {
            m_server_thread.join();
        }
        LogManager::getLogger()->info("HTTP server stopped.");
    }
}
