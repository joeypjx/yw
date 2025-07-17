#include "alarm_rule_engine.h"
#include "alarm_manager.h"
#include "log_manager.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <iomanip>
#include <taos.h>

AlarmRuleEngine::AlarmRuleEngine(std::shared_ptr<AlarmRuleStorage> rule_storage,
                               std::shared_ptr<ResourceStorage> resource_storage)
    : m_rule_storage(rule_storage), m_resource_storage(resource_storage), m_alarm_manager(nullptr),
      m_running(false), m_evaluation_interval(std::chrono::seconds(30)) {
}

AlarmRuleEngine::AlarmRuleEngine(std::shared_ptr<AlarmRuleStorage> rule_storage,
                               std::shared_ptr<ResourceStorage> resource_storage,
                               std::shared_ptr<AlarmManager> alarm_manager)
    : m_rule_storage(rule_storage), m_resource_storage(resource_storage), m_alarm_manager(alarm_manager),
      m_running(false), m_evaluation_interval(std::chrono::seconds(30)) {
}

AlarmRuleEngine::~AlarmRuleEngine() {
    stop();
}

bool AlarmRuleEngine::start() {
    if (m_running) {
        return true;
    }
    
    logInfo("Starting alarm rule engine...");
    
    loadRulesFromDatabase();
    
    m_running = true;
    m_evaluation_thread = std::thread(&AlarmRuleEngine::evaluationLoop, this);
    
    logInfo("Alarm rule engine started");
    return true;
}

void AlarmRuleEngine::stop() {
    if (!m_running) {
        return;
    }
    
    logInfo("Stopping alarm rule engine...");
    
    m_running = false;
    if (m_evaluation_thread.joinable()) {
        m_evaluation_thread.join();
    }
    
    logInfo("Alarm rule engine stopped");
}

void AlarmRuleEngine::setEvaluationInterval(std::chrono::seconds interval) {
    m_evaluation_interval = interval;
}

std::vector<AlarmInstance> AlarmRuleEngine::getCurrentAlarmInstances() const {
    std::lock_guard<std::mutex> lock(m_instances_mutex);
    std::vector<AlarmInstance> instances;
    instances.reserve(m_alarm_instances.size());
    
    for (const auto& pair : m_alarm_instances) {
        instances.push_back(pair.second);
    }
    
    return instances;
}

void AlarmRuleEngine::setAlarmEventCallback(std::function<void(const AlarmEvent&)> callback) {
    m_alarm_event_callback = callback;
}

void AlarmRuleEngine::evaluationLoop() {
    while (m_running) {
        try {
            loadRulesFromDatabase();
            evaluateRules();
        } catch (const std::exception& e) {
            logError("Error in evaluation loop: " + std::string(e.what()));
        }
        
        std::this_thread::sleep_for(m_evaluation_interval);
    }
}

void AlarmRuleEngine::loadRulesFromDatabase() {
    try {
        std::lock_guard<std::mutex> lock(m_rules_mutex);
        m_rules = m_rule_storage->getEnabledAlarmRules();
        logDebug("Loaded " + std::to_string(m_rules.size()) + " alarm rules from database");
    } catch (const std::exception& e) {
        logError("Failed to load rules from database: " + std::string(e.what()));
    }
}

void AlarmRuleEngine::evaluateRules() {
    std::lock_guard<std::mutex> lock(m_rules_mutex);
    
    for (const auto& rule : m_rules) {
        try {
            evaluateRule(rule);
        } catch (const std::exception& e) {
            logError("Failed to evaluate rule " + rule.alert_name + ": " + std::string(e.what()));
        }
    }
}

void AlarmRuleEngine::evaluateRule(const AlarmRule& rule) {
    logDebug("Evaluating rule: " + rule.alert_name);
    
    nlohmann::json expression;
    try {
        expression = nlohmann::json::parse(rule.expression_json);
    } catch (const std::exception& e) {
        logError("Failed to parse rule expression for " + rule.alert_name + ": " + std::string(e.what()));
        return;
    }
    
    if (!expression.contains("stable") || !expression.contains("metric")) {
        logError("Rule " + rule.alert_name + " missing required fields: stable or metric");
        return;
    }
    
    std::string stable = expression["stable"];
    std::string metric = expression["metric"];
    
    std::string sql = convertRuleToSQL(expression, stable, metric);
    if (sql.empty()) {
        logError("Failed to convert rule to SQL for " + rule.alert_name);
        return;
    }
    
    logDebug("Generated SQL for rule " + rule.alert_name + ": " + sql);
    
    std::vector<QueryResult> results = executeQuery(sql);
    
    std::set<std::string> active_from_db;
    for (const auto& result : results) {
        std::string fingerprint = generateFingerprint(rule.alert_name, result.labels);
        active_from_db.insert(fingerprint);
    }
    
    reconcileAlarmStates(rule, active_from_db, results);
}

std::string AlarmRuleEngine::convertRuleToSQL(const nlohmann::json& expression, 
                                            const std::string& stable, 
                                            const std::string& metric) {
    std::ostringstream sql;
    
    sql << "SELECT LAST(" << metric << ") AS " << metric << ", host_ip, ";
    
    std::vector<std::string> used_tag_fields;
    std::vector<std::string> where_conditions;
    
    if (expression.contains("tags") && expression["tags"].is_array()) {
        for (const auto& tag_condition : expression["tags"]) {
            for (auto it = tag_condition.begin(); it != tag_condition.end(); ++it) {
                std::string tag_key = it.key();
                std::string tag_value = it.value().get<std::string>();
                
                used_tag_fields.push_back(tag_key);
                where_conditions.push_back(tag_key + " = '" + tag_value + "'");
            }
        }
    }
    
    for (const auto& tag : used_tag_fields) {
        sql << tag << ", ";
    }
    
    sql << "ts FROM " << stable;
    
    if (expression.contains("conditions") && expression["conditions"].is_array()) {
        for (const auto& condition : expression["conditions"]) {
            if (condition.contains("operator") && condition.contains("threshold")) {
                std::string op = condition["operator"];
                double threshold = condition["threshold"];
                where_conditions.push_back(metric + " " + op + " " + std::to_string(threshold));
            }
        }
    }
    
    where_conditions.push_back("ts > NOW() - 10s");
    
    if (!where_conditions.empty()) {
        sql << " WHERE ";
        for (size_t i = 0; i < where_conditions.size(); ++i) {
            if (i > 0) sql << " AND ";
            sql << "(" << where_conditions[i] << ")";
        }
    }
    
    sql << " GROUP BY host_ip";
    for (const auto& tag : used_tag_fields) {
        sql << ", " << tag;
    }
    
    return sql.str();
}


void AlarmRuleEngine::reconcileAlarmStates(const AlarmRule& rule, 
                                         const std::set<std::string>& active_from_db,
                                         const std::vector<QueryResult>& results) {
    std::lock_guard<std::mutex> lock(m_instances_mutex);
    
    auto now = std::chrono::system_clock::now();
    std::map<std::string, QueryResult> result_map;
    
    for (const auto& result : results) {
        std::string fingerprint = generateFingerprint(rule.alert_name, result.labels);
        result_map[fingerprint] = result;
    }
    
    for (const auto& fingerprint : active_from_db) {
        auto it = m_alarm_instances.find(fingerprint);
        if (it == m_alarm_instances.end()) {
            createNewAlarmInstance(fingerprint, rule, result_map[fingerprint], now);
        } else {
            updateExistingAlarmInstance(it->second, rule, result_map[fingerprint], now);
        }
    }
    
    // 处理已恢复的告警（在previous_state_map中但不在active_from_db中）
    std::string alert_prefix = "alertname=" + rule.alert_name + ",";
    std::vector<std::string> to_remove;
    
    for (auto& pair : m_alarm_instances) {
        const std::string& fingerprint = pair.first;
        
        // 只处理属于当前规则的实例
        if (fingerprint.find(alert_prefix) == 0) {
            // 如果不在active_from_db中，说明已恢复
            if (active_from_db.find(fingerprint) == active_from_db.end()) {
                QueryResult empty_result;
                empty_result.value = 0.0;
                handleResolvedAlarm(pair.second, rule, empty_result, now);
                if (pair.second.state == AlarmInstanceState::INACTIVE) {
                    to_remove.push_back(fingerprint);
                }
            }
        }
    }
    
    for (const auto& fingerprint : to_remove) {
        m_alarm_instances.erase(fingerprint);
    }
}

void AlarmRuleEngine::createNewAlarmInstance(const std::string& fingerprint, 
                                           const AlarmRule& rule, 
                                           const QueryResult& result, 
                                           std::chrono::system_clock::time_point now) {
    AlarmInstance instance;
    instance.fingerprint = fingerprint;
    instance.alert_name = rule.alert_name;
    instance.state = AlarmInstanceState::PENDING;
    instance.state_changed_at = now;
    instance.pending_start_at = now;
    instance.labels = result.labels;
    instance.labels["alertname"] = rule.alert_name;
    instance.labels["severity"] = rule.severity;
    instance.labels["alert_type"] = rule.alert_type;
    instance.labels["value"] = std::to_string(result.value);
    instance.labels["metrics"] = result.metric;
    instance.value = result.value;
    
    instance.annotations["summary"] = rule.summary;
    instance.annotations["description"] = replaceTemplate(rule.description, instance.labels);
    
    m_alarm_instances[fingerprint] = instance;
    
    logInfo("Created new alarm instance: " + fingerprint + " (PENDING)");
}

void AlarmRuleEngine::updateExistingAlarmInstance(AlarmInstance& instance, 
                                                const AlarmRule& rule, 
                                                const QueryResult& result, 
                                                std::chrono::system_clock::time_point now) {
    // 更新当前值
    instance.value = result.value;
    instance.labels["value"] = std::to_string(result.value);
    
    if (instance.state == AlarmInstanceState::PENDING) {
        auto for_duration = parseDuration(rule.for_duration);
        if (now - instance.pending_start_at >= for_duration) {
            instance.state = AlarmInstanceState::FIRING;
            instance.state_changed_at = now;
            generateAlarmEvent(instance, rule, "firing");
            logInfo("Alarm instance " + instance.fingerprint + " transitioned to FIRING");
        }
    }
}

void AlarmRuleEngine::handleResolvedAlarm(AlarmInstance& instance, 
                                        const AlarmRule& rule, 
                                        const QueryResult& result,
                                        std::chrono::system_clock::time_point now) {
    if (instance.state == AlarmInstanceState::FIRING) {
        instance.state = AlarmInstanceState::RESOLVED;
        instance.state_changed_at = now;
        generateAlarmEvent(instance, rule, "resolved");
        logInfo("Alarm instance " + instance.fingerprint + " RESOLVED");
        
        instance.state = AlarmInstanceState::INACTIVE;
    } else if (instance.state == AlarmInstanceState::PENDING) {
        instance.state = AlarmInstanceState::INACTIVE;
        instance.state_changed_at = now;
        logInfo("Alarm instance " + instance.fingerprint + " transitioned from PENDING to INACTIVE");
    }
}

void AlarmRuleEngine::generateAlarmEvent(const AlarmInstance& instance, 
                                       const AlarmRule& rule, 
                                       const std::string& status) {
    AlarmEvent event;
    event.fingerprint = instance.fingerprint;
    event.status = status;
    event.labels = instance.labels;
    event.annotations = instance.annotations;
    event.starts_at = instance.pending_start_at;
    
    if (status == "resolved") {
        event.ends_at = instance.state_changed_at;
    }
    
    logInfo("Generated alarm event: " + event.toJson());
    
    if (m_alarm_manager) {
        m_alarm_manager->processAlarmEvent(event);
    }
    
    if (m_alarm_event_callback) {
        m_alarm_event_callback(event);
    }
}

std::vector<QueryResult> AlarmRuleEngine::executeQuery(const std::string& sql) {
    logDebug("Executing query: " + sql);
    
    if (m_resource_storage) {
        return m_resource_storage->executeQuerySQL(sql);
    }
    
    logError("ResourceStorage not available for query execution");
    return std::vector<QueryResult>();
}

bool AlarmRuleEngine::evaluateCondition(double value, const std::string& op, double threshold) {
    if (op == ">") return value > threshold;
    if (op == "<") return value < threshold;
    if (op == ">=") return value >= threshold;
    if (op == "<=") return value <= threshold;
    if (op == "=") return value == threshold;
    if (op == "!=") return value != threshold;
    return false;
}

std::string AlarmRuleEngine::generateFingerprint(const std::string& alert_name, 
                                               const std::map<std::string, std::string>& labels) {
    std::ostringstream fingerprint;
    fingerprint << "alertname=" << alert_name;
    
    std::vector<std::pair<std::string, std::string>> sorted_labels(labels.begin(), labels.end());
    std::sort(sorted_labels.begin(), sorted_labels.end());
    
    for (const auto& label : sorted_labels) {
        fingerprint << "," << label.first << "=" << label.second;
    }
    
    return fingerprint.str();
}

std::chrono::seconds AlarmRuleEngine::parseDuration(const std::string& duration) {
    std::regex duration_regex(R"((\d+)([smhd]))");
    std::smatch match;
    
    if (std::regex_match(duration, match, duration_regex)) {
        int value = std::stoi(match[1]);
        std::string unit = match[2];
        
        if (unit == "s") return std::chrono::seconds(value);
        if (unit == "m") return std::chrono::seconds(value * 60);
        if (unit == "h") return std::chrono::seconds(value * 3600);
        if (unit == "d") return std::chrono::seconds(value * 86400);
    }
    
    return std::chrono::seconds(0);
}

std::string AlarmRuleEngine::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    if (tp == std::chrono::system_clock::time_point{}) {
        return "";
    }
    
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string AlarmRuleEngine::replaceTemplate(const std::string& template_str, 
                                           const std::map<std::string, std::string>& values) {
    std::string result = template_str;
    
    for (const auto& pair : values) {
        std::string placeholder = "{{" + pair.first + "}}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), pair.second);
            pos += pair.second.length();
        }
    }
    
    return result;
}

std::string AlarmEvent::toJson() const {
    nlohmann::json json;
    json["fingerprint"] = fingerprint;
    json["status"] = status;
    json["labels"] = labels;
    json["annotations"] = annotations;
    json["starts_at"] = AlarmRuleEngine::formatTimestamp(starts_at);
    
    if (ends_at != std::chrono::system_clock::time_point{}) {
        json["ends_at"] = AlarmRuleEngine::formatTimestamp(ends_at);
    } else {
        json["ends_at"] = nullptr;
    }
    
    json["generator_url"] = generator_url;
    
    return json.dump(2);
}

void AlarmRuleEngine::logDebug(const std::string& message) {
    LogManager::getLogger()->debug(message);
}

void AlarmRuleEngine::logInfo(const std::string& message) {
    LogManager::getLogger()->info(message);
}

void AlarmRuleEngine::logError(const std::string& message) {
    LogManager::getLogger()->error(message);
}