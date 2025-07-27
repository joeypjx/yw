#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include "json.hpp"
#include "alarm_rule_storage.h"
#include "resource_storage.h"


// 告警实例状态
enum class AlarmInstanceState {
    INACTIVE,   // 条件不满足
    PENDING,    // 条件首次满足，开始计时
    FIRING,     // 条件持续满足超过for时长，正式触发告警
    RESOLVED    // 此前处于FIRING状态的告警，条件不再满足
};

// 告警实例
struct AlarmInstance {
    std::string fingerprint;        // 告警指纹 (alert_name + 实例的唯一标签组合)
    std::string alert_name;         // 告警名称
    AlarmInstanceState state;       // 当前状态
    std::chrono::system_clock::time_point state_changed_at;  // 状态变更时间
    std::chrono::system_clock::time_point pending_start_at;  // 开始pending的时间
    std::map<std::string, std::string> labels;              // 标签 (用于生成事件)
    std::map<std::string, std::string> annotations;         // 注解 (用于生成事件)
    double value;                   // 触发告警的值
};

// 告警事件
struct AlarmEvent {
    std::string fingerprint;
    std::string status;             // "firing" 或 "resolved"
    std::map<std::string, std::string> labels;
    std::map<std::string, std::string> annotations;
    std::chrono::system_clock::time_point starts_at;
    std::chrono::system_clock::time_point ends_at;
    std::string generator_url;
    
    std::string toJson() const;
};


class AlarmRuleEngine {
public:
    AlarmRuleEngine(std::shared_ptr<AlarmRuleStorage> rule_storage,
                   std::shared_ptr<ResourceStorage> resource_storage);
    ~AlarmRuleEngine();

    // 启动和停止引擎
    bool start();
    void stop();
    
    // 设置评估间隔
    void setEvaluationInterval(std::chrono::seconds interval);
    
    // 获取当前告警实例
    std::vector<AlarmInstance> getCurrentAlarmInstances() const;
    
    // 获取告警事件回调
    void setAlarmEventCallback(std::function<void(const AlarmEvent&)> callback);
    
    // 工具函数 (public for AlarmEvent)
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);

private:
    std::shared_ptr<AlarmRuleStorage> m_rule_storage;
    std::shared_ptr<ResourceStorage> m_resource_storage;
    
    std::vector<AlarmRule> m_rules;                           // 内存中的告警规则
    std::map<std::string, AlarmInstance> m_alarm_instances;   // 告警实例状态
    
    std::atomic<bool> m_running;
    std::thread m_evaluation_thread;
    std::chrono::seconds m_evaluation_interval;
    
    mutable std::mutex m_instances_mutex;
    mutable std::mutex m_rules_mutex;
    
    std::function<void(const AlarmEvent&)> m_alarm_event_callback;
    
    // 核心逻辑
    void evaluationLoop();
    void loadRulesFromDatabase();
    void evaluateRules();
    void evaluateRule(const AlarmRule& rule);
    
    // 规则到SQL转换 (新设计)
    std::string convertRuleToSQL(const nlohmann::json& expression, const std::string& stable, const std::string& metric);
    bool evaluateCondition(double value, const std::string& op, double threshold);
    
    // 查询执行
    std::vector<QueryResult> executeQuery(const std::string& sql);
    
    // 告警实例管理 (新的状态协调算法)
    std::string generateFingerprint(const std::string& alert_name, const std::map<std::string, std::string>& labels);
    void reconcileAlarmStates(const AlarmRule& rule, const std::set<std::string>& active_from_db, 
                             const std::vector<QueryResult>& results);
    void createNewAlarmInstance(const std::string& fingerprint, const AlarmRule& rule, 
                               const QueryResult& result, std::chrono::system_clock::time_point now);
    void updateExistingAlarmInstance(AlarmInstance& instance, const AlarmRule& rule, 
                                   const QueryResult& result, std::chrono::system_clock::time_point now);
    void handleResolvedAlarm(AlarmInstance& instance, const AlarmRule& rule, 
                           const QueryResult& result, std::chrono::system_clock::time_point now);
    
    // 告警事件生成
    void generateAlarmEvent(const AlarmInstance& instance, const AlarmRule& rule, 
                          const std::string& status);
    
    // 工具函数
    std::chrono::seconds parseDuration(const std::string& duration);
    std::string replaceTemplate(const std::string& template_str, const std::map<std::string, std::string>& values);
    
    // 日志输出
    void logDebug(const std::string& message);
    void logInfo(const std::string& message);
    void logError(const std::string& message);
};