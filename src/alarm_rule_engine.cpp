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
    
    // 加载规则
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
            // 重新加载规则
            loadRulesFromDatabase();
            
            // 评估规则
            evaluateRules();
            
        } catch (const std::exception& e) {
            logError("Error in evaluation loop: " + std::string(e.what()));
        }
        
        // 等待下一次评估
        std::this_thread::sleep_for(m_evaluation_interval);
    }
}

void AlarmRuleEngine::loadRulesFromDatabase() {
    try {
        std::lock_guard<std::mutex> lock(m_rules_mutex);
        m_rules = m_rule_storage->getEnabledAlarmRules();
        logInfo("Loaded " + std::to_string(m_rules.size()) + " alarm rules from database");
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
    
    // 解析规则表达式
    nlohmann::json expression;
    try {
        expression = nlohmann::json::parse(rule.expression_json);
    } catch (const std::exception& e) {
        logError("Failed to parse rule expression for " + rule.alert_name + ": " + std::string(e.what()));
        return;
    }
    
    // 确定stable名称
    std::string stable;
    if (expression.contains("stable")) {
        stable = expression["stable"];
    } else if (expression.contains("and") && expression["and"].is_array()) {
        // 在and条件中查找stable
        for (const auto& condition : expression["and"]) {
            if (condition.contains("stable")) {
                stable = condition["stable"];
                break;
            }
        }
    } else if (expression.contains("or") && expression["or"].is_array()) {
        // 在or条件中查找stable
        for (const auto& condition : expression["or"]) {
            if (condition.contains("stable")) {
                stable = condition["stable"];
                break;
            }
        }
    }
    
    if (stable.empty()) {
        logError("No stable found in rule expression for " + rule.alert_name);
        return;
    }
    
    // 转换为SQL
    std::string sql = convertRuleToSQL(expression, stable);
    if (sql.empty()) {
        logError("Failed to convert rule to SQL for " + rule.alert_name);
        return;
    }
    
    logDebug("Generated SQL for rule " + rule.alert_name + ": " + sql);
    
    // 执行查询
    std::vector<QueryResult> results = executeQuery(sql);
    
    // 处理查询结果
    for (const auto& result : results) {
        std::string fingerprint = generateFingerprint(rule.alert_name, result.labels);
        
        // 检查条件是否满足
        bool condition_met = false;
        if (expression.contains("threshold")) {
            std::string op = expression.value("operator", ">");
            double threshold = expression.value("threshold", 0.0);
            condition_met = evaluateCondition(result.value, op, threshold);
        } else {
            // 对于复杂表达式，简单检查值是否大于0
            condition_met = result.value > 0;
        }
        
        updateAlarmInstance(fingerprint, rule, result, condition_met);
    }
}

std::string AlarmRuleEngine::convertRuleToSQL(const nlohmann::json& expression, const std::string& stable) {
    std::ostringstream sql;
    
    // 基础查询结构
    sql << "SELECT ";
    
    // 确定指标字段
    std::string metric = "";
    
    if (expression.contains("metric")) {
        metric = expression["metric"];
    }
    
    // 在复杂表达式中查找metric
    if (metric.empty()) {
        std::function<void(const nlohmann::json&)> findMetric = [&](const nlohmann::json& expr) {
            if (expr.contains("metric")) {
                metric = expr["metric"];
                return;
            }
            if (expr.contains("and") && expr["and"].is_array()) {
                for (const auto& item : expr["and"]) {
                    findMetric(item);
                    if (!metric.empty()) break;
                }
            }
            if (expr.contains("or") && expr["or"].is_array()) {
                for (const auto& item : expr["or"]) {
                    findMetric(item);
                    if (!metric.empty()) break;
                }
            }
        };
        findMetric(expression);
    }
    
    if (metric.empty()) {
        logError("No metric found in expression");
        return "";
    }
    
    // 构建SELECT子句 - 不使用聚合函数
    sql << metric << " , ts, ";
    
    // 添加标签字段
    std::vector<std::string> tag_fields = {"host_ip"};
    if (stable == "disk") {
        tag_fields.push_back("device");
        tag_fields.push_back("mount_point");
    } else if (stable == "network") {
        tag_fields.push_back("interface");
    } else if (stable == "gpu") {
        tag_fields.push_back("gpu_index");
        tag_fields.push_back("gpu_name");
    }
    
    for (const auto& tag : tag_fields) {
        sql << tag << ", ";
    }
    
    // 移除最后的逗号和空格
    std::string sql_str = sql.str();
    sql_str = sql_str.substr(0, sql_str.length() - 2);
    
    sql.str("");
    sql << sql_str;
    
    // FROM子句
    sql << " FROM " << stable;
    
    // WHERE子句 - 只包含标签条件和时间条件，不包含度量条件
    std::string where_clause = buildWhereClause(expression);
    
    std::vector<std::string> conditions;
    if (!where_clause.empty()) {
        conditions.push_back(where_clause);
    }
    
    conditions.push_back("ts > now() - 10s");
    if (conditions.size() > 0) {
        sql << " WHERE ";
    }
    
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) sql << " AND ";
        sql << conditions[i];
    }
    
    // 按时间戳降序排序，获取最新的记录
    sql << " ORDER BY ts DESC LIMIT 1";
    
    return sql.str();
}

std::string AlarmRuleEngine::buildWhereClause(const nlohmann::json& expression) {
    std::ostringstream where;
    
    std::function<bool(const nlohmann::json&)> processExpression = [&](const nlohmann::json& expr) -> bool {
        if (expr.contains("tag")) {
            std::string tag = expr["tag"];
            std::string op = expr.value("operator", "==");
            std::string value = expr.value("value", "");
            
            if (!value.empty()) {
                where << tag << " " << convertOperator(op) << " '" << value << "'";
                return true;
            }
        } else if (expr.contains("and") && expr["and"].is_array()) {
            std::ostringstream temp;
            std::vector<std::string> conditions;
            
            for (const auto& item : expr["and"]) {
                std::ostringstream itemStream;
                where.str("");
                where.clear();
                if (processExpression(item)) {
                    conditions.push_back(where.str());
                }
            }
            
            if (!conditions.empty()) {
                where.str("");
                where.clear();
                where << "(";
                for (size_t i = 0; i < conditions.size(); ++i) {
                    if (i > 0) where << " AND ";
                    where << conditions[i];
                }
                where << ")";
                return true;
            }
        } else if (expr.contains("or") && expr["or"].is_array()) {
            std::vector<std::string> conditions;
            
            for (const auto& item : expr["or"]) {
                std::ostringstream itemStream;
                where.str("");
                where.clear();
                if (processExpression(item)) {
                    conditions.push_back(where.str());
                }
            }
            
            if (!conditions.empty()) {
                where.str("");
                where.clear();
                where << "(";
                for (size_t i = 0; i < conditions.size(); ++i) {
                    if (i > 0) where << " OR ";
                    where << conditions[i];
                }
                where << ")";
                return true;
            }
        }
        return false;
    };
    
    processExpression(expression);
    return where.str();
}

std::string AlarmRuleEngine::buildMetricCondition(const nlohmann::json& expression) {
    std::ostringstream condition;
    
    std::function<bool(const nlohmann::json&)> processExpression = [&](const nlohmann::json& expr) -> bool {
        if (expr.contains("metric")) {
            std::string metric = expr["metric"];
            std::string op = expr.value("operator", ">");
            double threshold = expr.value("threshold", 0.0);
            
            condition << metric << " " << convertOperator(op) << " " << threshold;
            return true;
        } else if (expr.contains("and") && expr["and"].is_array()) {
            std::vector<std::string> conditions;
            
            for (const auto& item : expr["and"]) {
                std::ostringstream itemStream;
                condition.str("");
                condition.clear();
                if (processExpression(item)) {
                    conditions.push_back(condition.str());
                }
            }
            
            if (!conditions.empty()) {
                condition.str("");
                condition.clear();
                condition << "(";
                for (size_t i = 0; i < conditions.size(); ++i) {
                    if (i > 0) condition << " AND ";
                    condition << conditions[i];
                }
                condition << ")";
                return true;
            }
        } else if (expr.contains("or") && expr["or"].is_array()) {
            std::vector<std::string> conditions;
            
            for (const auto& item : expr["or"]) {
                std::ostringstream itemStream;
                condition.str("");
                condition.clear();
                if (processExpression(item)) {
                    conditions.push_back(condition.str());
                }
            }
            
            if (!conditions.empty()) {
                condition.str("");
                condition.clear();
                condition << "(";
                for (size_t i = 0; i < conditions.size(); ++i) {
                    if (i > 0) condition << " OR ";
                    condition << conditions[i];
                }
                condition << ")";
                return true;
            }
        }
        return false;
    };
    
    processExpression(expression);
    return condition.str();
}

std::string AlarmRuleEngine::convertOperator(const std::string& op) {
    if (op == "==") return "=";
    if (op == "!=") return "<>";
    return op;  // >, <, >=, <=保持不变
}

std::vector<QueryResult> AlarmRuleEngine::executeQuery(const std::string& sql) {
    logDebug("Executing query: " + sql);
    
    // 使用ResourceStorage的executeQuery接口
    if (m_resource_storage) {
        return m_resource_storage->executeQuerySQL(sql);
    }
    
    logError("ResourceStorage not available for query execution");
    return std::vector<QueryResult>();
}

bool AlarmRuleEngine::evaluateCondition(double value, const std::string& op, double threshold) {
    if (op == ">" || op == "gt") return value > threshold;
    if (op == "<" || op == "lt") return value < threshold;
    if (op == ">=" || op == "gte") return value >= threshold;
    if (op == "<=" || op == "lte") return value <= threshold;
    if (op == "==" || op == "eq") return value == threshold;
    if (op == "!=" || op == "ne") return value != threshold;
    return false;
}

std::string AlarmRuleEngine::generateFingerprint(const std::string& alert_name, 
                                               const std::map<std::string, std::string>& labels) {
    std::ostringstream fingerprint;
    fingerprint << "alertname=" << alert_name;
    
    for (const auto& label : labels) {
        fingerprint << "," << label.first << "=" << label.second;
    }
    
    return fingerprint.str();
}

void AlarmRuleEngine::updateAlarmInstance(const std::string& fingerprint, const AlarmRule& rule, 
                                        const QueryResult& result, bool condition_met) {
    std::lock_guard<std::mutex> lock(m_instances_mutex);
    
    auto now = std::chrono::system_clock::now();
    
    // 查找或创建告警实例
    auto it = m_alarm_instances.find(fingerprint);
    if (it == m_alarm_instances.end()) {
        // 创建新实例
        AlarmInstance instance;
        instance.fingerprint = fingerprint;
        instance.alert_name = rule.alert_name;
        instance.state = condition_met ? AlarmInstanceState::PENDING : AlarmInstanceState::INACTIVE;
        instance.state_changed_at = now;
        instance.pending_start_at = condition_met ? now : std::chrono::system_clock::time_point{};
        instance.labels = result.labels;
        instance.labels["alertname"] = rule.alert_name;
        instance.labels["severity"] = rule.severity;
        instance.labels[result.metric] = std::to_string(result.value);
        instance.value = result.value;
        
        // 设置annotations
        instance.annotations["summary"] = rule.summary;
        instance.annotations["description"] = replaceTemplate(rule.description, instance.labels);
        
        m_alarm_instances[fingerprint] = instance;
        it = m_alarm_instances.find(fingerprint);
    }
    
    AlarmInstance& instance = it->second;
    instance.value = result.value;
    
    // 状态转换逻辑
    AlarmInstanceState old_state = instance.state;
    
    if (condition_met) {
        if (instance.state == AlarmInstanceState::INACTIVE) {
            instance.state = AlarmInstanceState::PENDING;
            instance.pending_start_at = now;
            instance.state_changed_at = now;
        } else if (instance.state == AlarmInstanceState::PENDING) {
            auto for_duration = parseDuration(rule.for_duration);
            if (now - instance.pending_start_at >= for_duration) {
                instance.state = AlarmInstanceState::FIRING;
                instance.state_changed_at = now;
            }
        }
        // FIRING状态保持不变
    } else {
        if (instance.state == AlarmInstanceState::FIRING) {
            instance.state = AlarmInstanceState::RESOLVED;
            instance.state_changed_at = now;
        } else if (instance.state == AlarmInstanceState::PENDING) {
            instance.state = AlarmInstanceState::INACTIVE;
            instance.state_changed_at = now;
        }
        // INACTIVE状态保持不变
    }
    
    // 如果状态发生变化，生成事件
    if (old_state != instance.state) {
        processStateTransition(instance, rule);
    }
}

void AlarmRuleEngine::processStateTransition(AlarmInstance& instance, const AlarmRule& rule) {
    if (instance.state == AlarmInstanceState::FIRING || 
        instance.state == AlarmInstanceState::RESOLVED) {
        generateAlarmEvent(instance, rule);
    }
    
    logInfo("Alarm instance " + instance.fingerprint + " state changed to " + 
           std::to_string(static_cast<int>(instance.state)));
}

void AlarmRuleEngine::generateAlarmEvent(const AlarmInstance& instance, const AlarmRule& rule) {
    AlarmEvent event;
    event.fingerprint = instance.fingerprint;
    event.status = (instance.state == AlarmInstanceState::FIRING) ? "firing" : "resolved";
    event.labels = instance.labels;
    event.annotations = instance.annotations;
    event.starts_at = instance.pending_start_at;
    event.ends_at = (instance.state == AlarmInstanceState::RESOLVED) ? 
                   instance.state_changed_at : std::chrono::system_clock::time_point{};
    
    logInfo("Generated alarm event: " + event.toJson());
    
    // 发送事件到告警管理器
    if (m_alarm_manager) {
        m_alarm_manager->processAlarmEvent(event);
    }
    
    // 调用回调函数
    if (m_alarm_event_callback) {
        m_alarm_event_callback(event);
    }
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
    
    return std::chrono::seconds(300); // 默认5分钟
}

std::string AlarmRuleEngine::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
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