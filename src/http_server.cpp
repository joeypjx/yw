#include "http_server.h"
#include "log_manager.h"
#include "json.hpp"
#include <iostream>
#include <regex>
#include <tuple>

using json = nlohmann::json;

const char* get_web_page_html() {
    return R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Alarm Management</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; margin: 2em; background-color: #f4f6f8; color: #333; }
        h1, h2 { color: #1a253c; }
        #container { max-width: 1200px; margin: 0 auto; background: white; padding: 2em; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        table { width: 100%; border-collapse: collapse; margin-bottom: 1.5em; }
        th, td { border: 1px solid #ddd; padding: 12px; text-align: left; vertical-align: top; }
        th { background-color: #f2f2f2; font-weight: 600; }
        tr:nth-child(even) { background-color: #f9f9f9; }
        form { display: grid; grid-template-columns: 150px 1fr; gap: 15px; margin-top: 1em; padding: 1.5em; border: 1px solid #ddd; border-radius: 5px; background-color: #fafafa; }
        form label { font-weight: 600; text-align: right; padding-top: 8px; }
        input[type="text"], textarea { width: 100%; padding: 8px; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; }
        textarea { resize: vertical; }
        .form-buttons { grid-column: 2; display: flex; gap: 10px; }
        button { cursor: pointer; padding: 10px 15px; border: none; border-radius: 4px; font-size: 1em; font-weight: 500; }
        button[type="submit"] { background-color: #28a745; color: white; }
        #cancel-edit, .refresh-btn { background-color: #6c757d; color: white; }
        .actions-cell button { margin-right: 5px; padding: 5px 10px; font-size: 0.9em; }
        .edit-btn { background-color: #007bff; color: white; }
        .delete-btn { background-color: #dc3545; color: white; }
        pre { white-space: pre-wrap; word-wrap: break-word; background: #eee; padding: 5px; border-radius: 3px; }
        .section-header { display: flex; justify-content: space-between; align-items: center; }
        .status-online { color: #28a745; font-weight: bold; }
        .status-offline { color: #dc3545; font-weight: bold; }
        .status-warning { color: #ffc107; font-weight: bold; }
    </style>
</head>
<body>
    <div id="container">
        <h1>System Management</h1>

        <!-- Nodes Section -->
        <div class="section-header">
            <h2>Node Information</h2>
            <button id="refresh-nodes" class="refresh-btn">Refresh Nodes</button>
        </div>
        <table id="nodes-table">
            <thead>
                <tr>
                    <th>Host IP</th>
                    <th>Hostname</th>
                    <th>Box Type</th>
                    <th>CPU Type</th>
                    <th>OS Type</th>
                    <th>CPU Arch</th>
                    <th>GPU Count</th>
                    <th>Last Heartbeat</th>
                    <th>Status</th>
                </tr>
            </thead>
            <tbody></tbody>
        </table>

        <!-- Alarm Rules Section -->
        <div class="section-header">
            <h2>Alarm Rules</h2>
        </div>
        <table id="rules-table">
            <thead>
                <tr>
                    <th>Name</th>
                    <th>Expression</th>
                    <th>For</th>
                    <th>Severity</th>
                    <th>Summary</th>
                    <th>Actions</th>
                </tr>
            </thead>
            <tbody></tbody>
        </table>
        
        <h3>Add / Edit Rule</h3>
        <form id="rule-form">
            <input type="hidden" id="rule-id">
            
            <label for="alert_name">Alert Name:</label>
            <input type="text" id="alert_name" required>
            
            <label for="expression">Expression (JSON):</label>
            <textarea id="expression" rows="5" required></textarea>
            
            <label for="for_duration">For:</label>
            <input type="text" id="for_duration" value="1m" required>
            
            <label for="severity">Severity:</label>
            <input type="text" id="severity" value="warning" required>
            
            <label for="summary">Summary:</label>
            <input type="text" id="summary" required>
            
            <label for="description">Description:</label>
            <input type="text" id="description" required>
            
            <label for="alert_type">Alert Type:</label>
            <input type="text" id="alert_type" value="metric" required>
            
            <label></label>
            <div class="form-buttons">
                <button type="submit">Save Rule</button>
                <button type="button" id="cancel-edit">Cancel</button>
            </div>
        </form>

        <!-- Alarm Events Section -->
        <div class="section-header">
            <h2>Alarm Events</h2>
            <button id="refresh-events" class="refresh-btn">Refresh Events</button>
        </div>
        <table id="events-table">
            <thead>
                <tr>
                    <th>Status</th>
                    <th>Labels</th>
                    <th>Annotations</th>
                    <th>Starts At</th>
                    <th>Ends At</th>
                </tr>
            </thead>
            <tbody></tbody>
        </table>
    </div>

    <script>
    document.addEventListener('DOMContentLoaded', () => {
        const nodesTableBody = document.querySelector('#nodes-table tbody');
        const rulesTableBody = document.querySelector('#rules-table tbody');
        const eventsTableBody = document.querySelector('#events-table tbody');
        const ruleForm = document.getElementById('rule-form');
        const ruleIdInput = document.getElementById('rule-id');
        const cancelEditButton = document.getElementById('cancel-edit');
        const refreshEventsButton = document.getElementById('refresh-events');
        const refreshNodesButton = document.getElementById('refresh-nodes');

        const API_BASE = '';

        const fetchAndRenderNodes = async () => {
            try {
                const response = await fetch(`${API_BASE}/nodes`);
                if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
                const data = await response.json();
                nodesTableBody.innerHTML = '';
                
                if (data.nodes.length === 0) {
                    nodesTableBody.innerHTML = '<tr><td colspan="9">No nodes found.</td></tr>';
                    return;
                }
                
                data.nodes.forEach(node => {
                    const nodeData = node.data;
                    const secondsSinceHeartbeat = node.seconds_since_last_heartbeat;
                    
                    // Determine node status based on heartbeat
                    let status = 'Online';
                    let statusClass = 'status-online';
                    if (secondsSinceHeartbeat > 30) {
                        status = 'Offline';
                        statusClass = 'status-offline';
                    } else if (secondsSinceHeartbeat > 15) {
                        status = 'Warning';
                        statusClass = 'status-warning';
                    }
                    
                    // Format last heartbeat time
                    const lastHeartbeat = new Date(node.last_heartbeat).toLocaleString();
                    
                    // Count GPU devices
                    const gpuCount = Array.isArray(nodeData.gpu) ? nodeData.gpu.length : 0;
                    
                    const row = document.createElement('tr');
                    row.innerHTML = `
                        <td>${escapeHtml(nodeData.host_ip)}</td>
                        <td>${escapeHtml(nodeData.hostname)}</td>
                        <td>${escapeHtml(nodeData.box_type)}</td>
                        <td>${escapeHtml(nodeData.cpu_type)}</td>
                        <td>${escapeHtml(nodeData.os_type)}</td>
                        <td>${escapeHtml(nodeData.cpu_arch)}</td>
                        <td>${gpuCount}</td>
                        <td>${lastHeartbeat}</td>
                        <td class="${statusClass}">${status}</td>
                    `;
                    nodesTableBody.appendChild(row);
                });
            } catch (error) {
                console.error('Error fetching nodes:', error);
                nodesTableBody.innerHTML = '<tr><td colspan="9">Failed to load node information.</td></tr>';
            }
        };

        const fetchAndRenderRules = async () => {
            try {
                const response = await fetch(`${API_BASE}/alarm/rules`);
                if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
                const rules = await response.json();
                rulesTableBody.innerHTML = '';
                rules.forEach(rule => {
                    const row = document.createElement('tr');
                    row.innerHTML = `
                        <td>${escapeHtml(rule.alert_name)}</td>
                        <td><pre>${escapeHtml(JSON.stringify(rule.expression, null, 2))}</pre></td>
                        <td>${escapeHtml(rule.for)}</td>
                        <td>${escapeHtml(rule.severity)}</td>
                        <td>${escapeHtml(rule.summary)}</td>
                        <td class="actions-cell">
                            <button class="edit-btn" onclick="editRule('${rule.id}')">Edit</button>
                            <button class="delete-btn" onclick="deleteRule('${rule.id}')">Delete</button>
                        </td>
                    `;
                    rulesTableBody.appendChild(row);
                });
            } catch (error) {
                console.error('Error fetching rules:', error);
                rulesTableBody.innerHTML = '<tr><td colspan="6">Failed to load alarm rules.</td></tr>';
            }
        };

        const fetchAndRenderEvents = async () => {
            try {
                const response = await fetch(`${API_BASE}/alarm/events?limit=100`);
                if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
                const events = await response.json();
                eventsTableBody.innerHTML = '';
                if (events.length === 0) {
                    eventsTableBody.innerHTML = '<tr><td colspan="5">No alarm events found.</td></tr>';
                    return;
                }
                events.forEach(event => {
                    const row = document.createElement('tr');
                    row.innerHTML = `
                        <td>${escapeHtml(event.status)}</td>
                        <td><pre>${escapeHtml(JSON.stringify(event.labels, null, 2))}</pre></td>
                        <td><pre>${escapeHtml(JSON.stringify(event.annotations, null, 2))}</pre></td>
                        <td>${new Date(event.starts_at).toLocaleString()}</td>
                        <td>${event.ends_at ? new Date(event.ends_at).toLocaleString() : 'N/A'}</td>
                    `;
                    eventsTableBody.appendChild(row);
                });
            } catch (error) {
                console.error('Error fetching events:', error);
                eventsTableBody.innerHTML = '<tr><td colspan="5">Failed to load alarm events.</td></tr>';
            }
        };

        const resetForm = () => {
            ruleForm.reset();
            ruleIdInput.value = '';
            document.querySelector('h3').innerText = 'Add / Edit Rule';
        };

        ruleForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            const id = ruleIdInput.value;
            const url = id ? `${API_BASE}/alarm/rules/${id}/update` : `${API_BASE}/alarm/rules`;
            const method = 'POST';

            let expression;
            try {
                expression = JSON.parse(document.getElementById('expression').value);
            } catch (err) {
                alert('Expression is not valid JSON.');
                return;
            }

            const body = {
                alert_name: document.getElementById('alert_name').value,
                expression: expression,
                for: document.getElementById('for_duration').value,
                severity: document.getElementById('severity').value,
                summary: document.getElementById('summary').value,
                description: document.getElementById('description').value,
                alert_type: document.getElementById('alert_type').value,
            };

            try {
                const response = await fetch(url, {
                    method: method,
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(body)
                });
                if (!response.ok) {
                    const errData = await response.json();
                    throw new Error(errData.error || `HTTP error! status: ${response.status}`);
                }
                await response.json();
                resetForm();
                fetchAndRenderRules();
            } catch (error) {
                console.error('Error saving rule:', error);
                alert(`Failed to save alarm rule: ${error.message}`);
            }
        });

        window.editRule = async (id) => {
            try {
                const response = await fetch(`${API_BASE}/alarm/rules/${id}`);
                if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
                const rule = await response.json();
                
                document.querySelector('h3').innerText = `Edit Rule: ${escapeHtml(rule.alert_name)}`;
                ruleIdInput.value = rule.id;
                document.getElementById('alert_name').value = rule.alert_name;
                document.getElementById('expression').value = JSON.stringify(rule.expression, null, 2);
                document.getElementById('for_duration').value = rule.for;
                document.getElementById('severity').value = rule.severity;
                document.getElementById('summary').value = rule.summary;
                document.getElementById('description').value = rule.description;
                document.getElementById('alert_type').value = rule.alert_type;
                
                ruleForm.scrollIntoView({ behavior: 'smooth' });
                document.getElementById('alert_name').focus();
            } catch (error) {
                console.error('Error fetching rule for edit:', error);
                alert('Failed to fetch rule details.');
            }
        };

        window.deleteRule = async (id) => {
            if (!confirm('Are you sure you want to delete this rule?')) return;
            try {
                const response = await fetch(`${API_BASE}/alarm/rules/${id}/delete`, { method: 'POST' });
                if (!response.ok) {
                    const errData = await response.json();
                    throw new Error(errData.error || `HTTP error! status: ${response.status}`);
                }
                await response.json();
                fetchAndRenderRules();
            } catch (error) {
                console.error('Error deleting rule:', error);
                alert(`Failed to delete alarm rule: ${error.message}`);
            }
        };

        function escapeHtml(unsafe) {
            if (unsafe === null || unsafe === undefined) return '';
            return unsafe
                .toString()
                .replace(/&/g, "&amp;")
                .replace(/</g, "&lt;")
                .replace(/>/g, "&gt;")
                .replace(/"/g, "&quot;")
                .replace(/'/g, "&#039;");
        }

        cancelEditButton.addEventListener('click', resetForm);
        refreshEventsButton.addEventListener('click', fetchAndRenderEvents);
        refreshNodesButton.addEventListener('click', fetchAndRenderNodes);

        fetchAndRenderNodes();
        fetchAndRenderRules();
        fetchAndRenderEvents();
        
        // Auto-refresh nodes every 10 seconds
        setInterval(fetchAndRenderNodes, 10000);
    });
    </script>
</body>
</html>
)HTML";
}

HttpServer::HttpServer(std::shared_ptr<ResourceStorage> resource_storage,
                       std::shared_ptr<AlarmRuleStorage> alarm_rule_storage,
                       std::shared_ptr<AlarmManager> alarm_manager,
                       std::shared_ptr<NodeStorage> node_storage,
                       const std::string& host, int port)
    : m_resource_storage(resource_storage), m_alarm_rule_storage(alarm_rule_storage), 
      m_alarm_manager(alarm_manager), m_node_storage(node_storage), m_host(host), m_port(port) {
    setup_routes();
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::setup_routes() {
    m_server.Get("/", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(get_web_page_html(), "text/html");
    });

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
    
    // 节点心跳路由
    m_server.Post("/heart", [this](const httplib::Request& req, httplib::Response& res) {
        this->handle_heart(req, res);
    });
    
    // 节点数据查询路由
    m_server.Get("/nodes", [this](const httplib::Request& req, httplib::Response& res) {
        this->handle_nodes_list(req, res);
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

void HttpServer::handle_heart(const httplib::Request& req, httplib::Response& res) {
    try {
        json body = json::parse(req.body);

        if (!body.contains("data") || !body["data"].is_object()) {
            res.set_content("{\"error\":\"'data' field is missing or not an object\"}", "application/json");
            res.status = 400;
            LogManager::getLogger()->warn("Heart request missing 'data' field");
            return;
        }

        const auto& data = body["data"];
        if (!data.contains("host_ip") || !data["host_ip"].is_string()) {
            res.set_content("{\"error\":\"'host_ip' is missing or not a string\"}", "application/json");
            res.status = 400;
            LogManager::getLogger()->warn("Heart request missing 'host_ip' field");
            return;
        }

        std::string host_ip = data["host_ip"];

        if (!m_node_storage) {
            res.set_content("{\"error\":\"Node storage not available\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Node storage not available for heart request");
            return;
        }

        if (m_node_storage->storeNodeData(host_ip, body)) {
            res.set_content("{\"status\":\"success\"}", "application/json");
            res.status = 200;
            LogManager::getLogger()->debug("Successfully processed heart data for node: {}", host_ip);
        } else {
            res.set_content("{\"error\":\"Failed to store node data\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Failed to store heart data for node: {}", host_ip);
        }

    } catch (const json::parse_error& e) {
        res.set_content("{\"error\":\"Invalid JSON format\"}", "application/json");
        res.status = 400;
        LogManager::getLogger()->error("JSON parse error in handle_heart: {}", e.what());
    } catch (const std::exception& e) {
        res.set_content("{\"error\":\"An unexpected error occurred\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_heart: {}", e.what());
    }
}

void HttpServer::handle_nodes_list(const httplib::Request& req, httplib::Response& res) {
    try {
        if (!m_node_storage) {
            res.set_content("{\"error\":\"Node storage not available\"}", "application/json");
            res.status = 500;
            LogManager::getLogger()->error("Node storage not available for nodes list request");
            return;
        }

        auto nodes = m_node_storage->getAllNodes();
        
        json response = json::array();
        for (const auto& node : nodes) {
            // Calculate time since last heartbeat
            auto now = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - node->last_heartbeat);
            
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
                {"seconds_since_last_heartbeat", duration.count()}
            };
            response.push_back(node_json);
        }
        
        json final_response = {
            {"total_nodes", nodes.size()},
            {"nodes", response}
        };
        
        res.set_content(final_response.dump(2), "application/json");
        res.status = 200;
        LogManager::getLogger()->info("Successfully retrieved {} nodes data", nodes.size());
        
    } catch (const std::exception& e) {
        res.set_content("{\"error\":\"Failed to retrieve nodes data\"}", "application/json");
        res.status = 500;
        LogManager::getLogger()->error("Exception in handle_nodes_list: {}", e.what());
    }
}
