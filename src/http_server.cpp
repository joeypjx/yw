#include "http_server.h"
#include "log_manager.h"
#include "json.hpp"
#include <iostream>
#include <regex>
#include <tuple>

using json = nlohmann::json;

HttpServer::HttpServer(std::shared_ptr<ResourceStorage> resource_storage,
                       std::shared_ptr<AlarmRuleStorage> alarm_rule_storage,
                       std::shared_ptr<AlarmManager> alarm_manager,
                       const std::string& host, int port)
    : m_resource_storage(resource_storage), m_alarm_rule_storage(alarm_rule_storage), 
      m_alarm_manager(alarm_manager), m_host(host), m_port(port) {
    setup_routes();
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::setup_routes() {
    m_server.Post("/resource", [this](const httplib::Request& req, httplib::Response& res) {
        this->handle_resource(req, res);
    });
    
    // 告警规则相关路由
    m_server.Post("/alarm/rules", [this](const httplib::Request& req, httplib::Response& res) {
        this->handle_alarm_rules_create(req, res);
    });
    
    m_server.Get("/alarm/rules", [this](const httplib::Request& req, httplib::Response& res) {
        this->handle_alarm_rules_list(req, res);
    });
    
    m_server.Get(R"(/alarm/rules/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        this->handle_alarm_rules_get(req, res);
    });
    
    m_server.Post(R"(/alarm/rules/([^/]+)/update)", [this](const httplib::Request& req, httplib::Response& res) {
        this->handle_alarm_rules_update(req, res);
    });
    
    m_server.Post(R"(/alarm/rules/([^/]+)/delete)", [this](const httplib::Request& req, httplib::Response& res) {
        this->handle_alarm_rules_delete(req, res);
    });
    
    // 告警事件相关路由
    m_server.Get("/alarm/events", [this](const httplib::Request& req, httplib::Response& res) {
        this->handle_alarm_events_list(req, res);
    });
}

void HttpServer::handle_alarm_rules_create(const httplib::Request& req, httplib::Response& res) {
    try {
        json body = json::parse(req.body);

        std::string id = m_alarm_rule_storage->insertAlarmRule(
            body["alert_name"].get<std::string>(),
            body["expression"].get<nlohmann::json>(),
            body["for"].get<std::string>(),
            body["severity"].get<std::string>(),
            body["summary"].get<std::string>(),
            body["description"].get<std::string>(),
            body["alert_type"].get<std::string>(),
            true
        );
        if (id.empty()) {
            res.set_content("{\"error\":\"Failed to store alarm rules\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Failed to store alarm rules");
        }
        res.set_content("{\"status\":\"success\", \"id\":\"" + id + "\"}", "application/json");
        res.status = 200;
        LogManager::getLogger()->info("Successfully processed alarm rules");
    } catch (const json::parse_error& e) {
        res.set_content("{\"error\":\"Invalid JSON format\"}", "application/json");
        res.status = 400;
        LogManager::getLogger()->error("Exception in handle_alarm_rules_create: {}", e.what());
    } catch (const std::exception& e) {
        res.set_content("{\"error\":\"An unexpected error occurred\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_alarm_rules_create: {}", e.what());
    }
}   

void HttpServer::handle_alarm_rules_list(const httplib::Request& req, httplib::Response& res) {
    try {
        auto rules = m_alarm_rule_storage->getAllAlarmRules();
        
        json response = json::array();
        for (const auto& rule : rules) {
            json rule_json = {
                {"id", rule.id},
                {"alert_name", rule.alert_name},
                {"expression", json::parse(rule.expression_json)},
                {"for", rule.for_duration},
                {"severity", rule.severity},
                {"summary", rule.summary},
                {"description", rule.description},
                {"alert_type", rule.alert_type},
                {"enabled", rule.enabled},
                {"created_at", rule.created_at},
                {"updated_at", rule.updated_at}
            };
            response.push_back(rule_json);
        }
        
        res.set_content(response.dump(2), "application/json");
        res.status = 200;
        LogManager::getLogger()->info("Successfully retrieved {} alarm rules", rules.size());
        
    } catch (const std::exception& e) {
        res.set_content("{\"error\":\"Failed to retrieve alarm rules\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_alarm_rules_list: {}", e.what());
    }
}

void HttpServer::handle_alarm_rules_get(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string rule_id = req.matches[1];
        
        AlarmRule rule = m_alarm_rule_storage->getAlarmRule(rule_id);
        
        if (rule.id.empty()) {
            res.set_content("{\"error\":\"Alarm rule not found\"}", "application/json");
            res.status = 404;
            LogManager::getLogger()->warn("Alarm rule not found: {}", rule_id);
            return;
        }
        
        json rule_json = {
            {"id", rule.id},
            {"alert_name", rule.alert_name},
            {"expression", json::parse(rule.expression_json)},
            {"for", rule.for_duration},
            {"severity", rule.severity},
            {"summary", rule.summary},
            {"description", rule.description},
            {"alert_type", rule.alert_type},
            {"enabled", rule.enabled},
            {"created_at", rule.created_at},
            {"updated_at", rule.updated_at}
        };
        
        res.set_content(rule_json.dump(2), "application/json");
        res.status = 200;
        LogManager::getLogger()->info("Successfully retrieved alarm rule: {}", rule_id);
        
    } catch (const std::exception& e) {
        res.set_content("{\"error\":\"Failed to retrieve alarm rule\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_alarm_rules_get: {}", e.what());
    }
}

void HttpServer::handle_alarm_rules_update(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string rule_id = req.matches[1];
        json body = json::parse(req.body);
        
        // 检查规则是否存在
        AlarmRule existing_rule = m_alarm_rule_storage->getAlarmRule(rule_id);
        if (existing_rule.id.empty()) {
            res.set_content("{\"error\":\"Alarm rule not found\"}", "application/json");
            res.status = 404;
            LogManager::getLogger()->warn("Alarm rule not found for update: {}", rule_id);
            return;
        }
        
        // 更新规则
        bool success = m_alarm_rule_storage->updateAlarmRule(
            rule_id,
            body["alert_name"].get<std::string>(),
            body["expression"].get<nlohmann::json>(),
            body["for"].get<std::string>(),
            body["severity"].get<std::string>(),
            body["summary"].get<std::string>(),
            body["description"].get<std::string>(),
            body["alert_type"].get<std::string>(),
            body.value("enabled", true)
        );
        
        if (success) {
            res.set_content("{\"status\":\"success\", \"id\":\"" + rule_id + "\"}", "application/json");
            res.status = 200;
            LogManager::getLogger()->info("Successfully updated alarm rule: {}", rule_id);
        } else {
            res.set_content("{\"error\":\"Failed to update alarm rule\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Failed to update alarm rule: {}", rule_id);
        }
        
    } catch (const json::parse_error& e) {
        res.set_content("{\"error\":\"Invalid JSON format\"}", "application/json");
        res.status = 400;
        LogManager::getLogger()->error("JSON parse error in handle_alarm_rules_update: {}", e.what());
    } catch (const std::exception& e) {
        res.set_content("{\"error\":\"Failed to update alarm rule\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_alarm_rules_update: {}", e.what());
    }
}

void HttpServer::handle_alarm_rules_delete(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string rule_id = req.matches[1];
        
        // 检查规则是否存在
        AlarmRule existing_rule = m_alarm_rule_storage->getAlarmRule(rule_id);
        if (existing_rule.id.empty()) {
            res.set_content("{\"error\":\"Alarm rule not found\"}", "application/json");
            res.status = 404;
            LogManager::getLogger()->warn("Alarm rule not found for deletion: {}", rule_id);
            return;
        }
        
        // 删除规则
        bool success = m_alarm_rule_storage->deleteAlarmRule(rule_id);
        
        if (success) {
            res.set_content("{\"status\":\"success\", \"id\":\"" + rule_id + "\"}", "application/json");
            res.status = 200;
            LogManager::getLogger()->info("Successfully deleted alarm rule: {}", rule_id);
        } else {
            res.set_content("{\"error\":\"Failed to delete alarm rule\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Failed to delete alarm rule: {}", rule_id);
        }
        
    } catch (const std::exception& e) {
        res.set_content("{\"error\":\"Failed to delete alarm rule\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_alarm_rules_delete: {}", e.what());
    }
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

void HttpServer::handle_alarm_events_list(const httplib::Request& req, httplib::Response& res) {
    try {
        if (!m_alarm_manager) {
            res.set_content("{\"error\":\"Alarm manager not available\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Alarm manager not available");
            return;
        }
        
        // 支持查询参数
        std::string status = req.get_param_value("status");
        std::string limit_str = req.get_param_value("limit");
        
        std::vector<AlarmEventRecord> events;
        
        if (status == "active" || status == "firing") {
            // 获取活跃的告警事件
            events = m_alarm_manager->getActiveAlarmEvents();
        } else if (!limit_str.empty()) {
            // 获取最近的告警事件（带限制）
            int limit = std::stoi(limit_str);
            events = m_alarm_manager->getRecentAlarmEvents(limit);
        } else {
            // 获取最近的告警事件（默认限制100条）
            events = m_alarm_manager->getRecentAlarmEvents(100);
        }
        
        json response = json::array();
        for (const auto& event : events) {
            json event_json = {
                {"id", event.id},
                {"fingerprint", event.fingerprint},
                {"status", event.status},
                {"labels", json::parse(event.labels_json)},
                {"annotations", json::parse(event.annotations_json)},
                {"starts_at", event.starts_at},
                {"ends_at", event.ends_at},
                {"generator_url", event.generator_url},
                {"created_at", event.created_at},
                {"updated_at", event.updated_at}
            };
            response.push_back(event_json);
        }
        
        res.set_content(response.dump(2), "application/json");
        res.status = 200;
        LogManager::getLogger()->info("Successfully retrieved {} alarm events", events.size());
        
    } catch (const std::exception& e) {
        res.set_content("{\"error\":\"Failed to retrieve alarm events\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_alarm_events_list: {}", e.what());
    }
}
