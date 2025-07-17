#include <gtest/gtest.h>
#include <httplib.h>
#include <thread>
#include <chrono>
#include "http_server.h"
#include "resource_storage.h"
#include "alarm_rule_storage.h"
#include "json.hpp"

using json = nlohmann::json;

class HttpServerAlarmRulesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化存储组件
        m_resource_storage = std::make_shared<ResourceStorage>("127.0.0.1", "test", "HZ715Net");
        m_alarm_rule_storage = std::make_shared<AlarmRuleStorage>("127.0.0.1", 3306, "test", "HZ715Net", "alarm_test");
        
        // 连接数据库
        m_resource_storage->connect();
        m_alarm_rule_storage->connect();
        m_alarm_rule_storage->createDatabase();
        m_alarm_rule_storage->createTable();
        
        // 初始化HTTP服务器 (alarm_manager传nullptr，因为这些测试不需要告警事件功能)
        m_server = std::make_shared<HttpServer>(m_resource_storage, m_alarm_rule_storage, nullptr, "127.0.0.1", 8081);
        
        // 启动服务器
        ASSERT_TRUE(m_server->start());
        
        // 等待服务器启动
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // 初始化HTTP客户端
        m_client = std::make_shared<httplib::Client>("127.0.0.1", 8081);
    }
    
    void TearDown() override {
        // 停止服务器
        m_server->stop();
        
        // 断开数据库连接
        m_resource_storage->disconnect();
        m_alarm_rule_storage->disconnect();
    }
    
    std::shared_ptr<ResourceStorage> m_resource_storage;
    std::shared_ptr<AlarmRuleStorage> m_alarm_rule_storage;
    std::shared_ptr<HttpServer> m_server;
    std::shared_ptr<httplib::Client> m_client;
};

// 测试基本的告警规则创建
TEST_F(HttpServerAlarmRulesTest, BasicAlarmRuleCreation) {
    json request_body = {
        {"templateId", "CPU使用过高"},
        {"metricName", "resource.cpu.usage_percent"},
        {"alarmType", "硬件状态"},
        {"alarmLevel", "严重"},
        {"triggerCountThreshold", 3},
        {"contentTemplate", "节点 {nodeId} 发生 {alarmLevel} 告警，CPU使用值为 {value}"},
        {"condition", {
            {"type", "GreaterThan"},
            {"threshold", 90.0}
        }},
        {"actions", {
            {{"type", "Notify"}},
            {{"type", "Database"}}
        }}
    };
    
    auto response = m_client->Post("/alarm/rules", request_body.dump(), "application/json");
    
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
    
    json response_body = json::parse(response->body);
    EXPECT_EQ(response_body["status"], "success");
    EXPECT_TRUE(response_body.contains("rule_id"));
    EXPECT_FALSE(response_body["rule_id"].get<std::string>().empty());
}

// 测试复杂的带标签的指标名称
TEST_F(HttpServerAlarmRulesTest, ComplexMetricNameWithTags) {
    json request_body = {
        {"templateId", "根目录磁盘使用过高"},
        {"metricName", "resource.disk[mount_point=/].usage_percent"},
        {"alarmType", "硬件状态"},
        {"alarmLevel", "一般"},
        {"triggerCountThreshold", 2},
        {"contentTemplate", "节点 {nodeId} 根目录磁盘使用率达到 {usage_percent}%"},
        {"condition", {
            {"type", "GreaterThan"},
            {"threshold", 80.0}
        }},
        {"actions", {
            {{"type", "Database"}}
        }}
    };
    
    auto response = m_client->Post("/alarm/rules", request_body.dump(), "application/json");
    
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
    
    json response_body = json::parse(response->body);
    EXPECT_EQ(response_body["status"], "success");
    EXPECT_TRUE(response_body.contains("rule_id"));
}

// 测试网络接口指标
TEST_F(HttpServerAlarmRulesTest, NetworkInterfaceMetric) {
    json request_body = {
        {"templateId", "网络接口错误率过高"},
        {"metricName", "resource.network[interface=eth0].rx_errors"},
        {"alarmType", "硬件状态"},
        {"alarmLevel", "提示"},
        {"triggerCountThreshold", 1},
        {"contentTemplate", "节点 {nodeId} 网络接口 eth0 接收错误数达到 {value}"},
        {"condition", {
            {"type", "GreaterThan"},
            {"threshold", 100.0}
        }},
        {"actions", {
            {{"type", "Notify"}}
        }}
    };
    
    auto response = m_client->Post("/alarm/rules", request_body.dump(), "application/json");
    
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
    
    json response_body = json::parse(response->body);
    EXPECT_EQ(response_body["status"], "success");
}

// 测试GPU指标
TEST_F(HttpServerAlarmRulesTest, GPUMetric) {
    json request_body = {
        {"templateId", "GPU使用率过高"},
        {"metricName", "resource.gpu[gpu_index=0].compute_usage"},
        {"alarmType", "硬件状态"},
        {"alarmLevel", "严重"},
        {"triggerCountThreshold", 3},
        {"contentTemplate", "节点 {nodeId} GPU 0 计算使用率达到 {compute_usage}%"},
        {"condition", {
            {"type", "GreaterThan"},
            {"threshold", 95.0}
        }},
        {"actions", {
            {{"type", "Notify"}},
            {{"type", "Database"}}
        }}
    };
    
    auto response = m_client->Post("/alarm/rules", request_body.dump(), "application/json");
    
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
    
    json response_body = json::parse(response->body);
    EXPECT_EQ(response_body["status"], "success");
}

// 测试复合条件
TEST_F(HttpServerAlarmRulesTest, CompoundConditions) {
    json request_body = {
        {"templateId", "CPU使用率中等范围"},
        {"metricName", "resource.cpu.usage_percent"},
        {"alarmType", "硬件状态"},
        {"alarmLevel", "一般"},
        {"triggerCountThreshold", 2},
        {"contentTemplate", "节点 {nodeId} CPU使用率在中等范围 {usage_percent}%"},
        {"condition", {
            {"type", "And"},
            {"conditions", {
                {
                    {"type", "GreaterThan"},
                    {"threshold", 60.0}
                },
                {
                    {"type", "LessThan"},
                    {"threshold", 90.0}
                }
            }}
        }},
        {"actions", {
            {{"type", "Database"}}
        }}
    };
    
    auto response = m_client->Post("/alarm/rules", request_body.dump(), "application/json");
    
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
    
    json response_body = json::parse(response->body);
    EXPECT_EQ(response_body["status"], "success");
}

// 测试请求验证 - 缺少必填字段
TEST_F(HttpServerAlarmRulesTest, ValidationMissingTemplateId) {
    json request_body = {
        {"metricName", "resource.cpu.usage_percent"},
        {"alarmLevel", "严重"},
        {"triggerCountThreshold", 3},
        {"contentTemplate", "CPU使用过高"},
        {"condition", {
            {"type", "GreaterThan"},
            {"threshold", 90.0}
        }},
        {"actions", {
            {{"type", "Notify"}}
        }}
    };
    
    auto response = m_client->Post("/alarm/rules", request_body.dump(), "application/json");
    
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 400);
    
    json response_body = json::parse(response->body);
    EXPECT_TRUE(response_body.contains("error"));
    EXPECT_TRUE(response_body["error"].get<std::string>().find("templateId") != std::string::npos);
}

// 测试请求验证 - 缺少指标名称
TEST_F(HttpServerAlarmRulesTest, ValidationMissingMetricName) {
    json request_body = {
        {"templateId", "CPU使用过高"},
        {"alarmLevel", "严重"},
        {"triggerCountThreshold", 3},
        {"contentTemplate", "CPU使用过高"},
        {"condition", {
            {"type", "GreaterThan"},
            {"threshold", 90.0}
        }},
        {"actions", {
            {{"type", "Notify"}}
        }}
    };
    
    auto response = m_client->Post("/alarm/rules", request_body.dump(), "application/json");
    
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 400);
    
    json response_body = json::parse(response->body);
    EXPECT_TRUE(response_body.contains("error"));
    EXPECT_TRUE(response_body["error"].get<std::string>().find("metricName") != std::string::npos);
}

// 测试请求验证 - 无效的JSON格式
TEST_F(HttpServerAlarmRulesTest, ValidationInvalidJSON) {
    std::string invalid_json = "{\"templateId\": \"test\", \"invalid";
    
    auto response = m_client->Post("/alarm/rules", invalid_json, "application/json");
    
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 400);
    
    json response_body = json::parse(response->body);
    EXPECT_TRUE(response_body.contains("error"));
    EXPECT_TRUE(response_body["error"].get<std::string>().find("Invalid JSON") != std::string::npos);
}

// 测试请求验证 - 无效的触发次数
TEST_F(HttpServerAlarmRulesTest, ValidationInvalidTriggerCount) {
    json request_body = {
        {"templateId", "CPU使用过高"},
        {"metricName", "resource.cpu.usage_percent"},
        {"alarmLevel", "严重"},
        {"triggerCountThreshold", "invalid"},  // 应该是整数
        {"contentTemplate", "CPU使用过高"},
        {"condition", {
            {"type", "GreaterThan"},
            {"threshold", 90.0}
        }},
        {"actions", {
            {{"type", "Notify"}}
        }}
    };
    
    auto response = m_client->Post("/alarm/rules", request_body.dump(), "application/json");
    
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 400);
    
    json response_body = json::parse(response->body);
    EXPECT_TRUE(response_body.contains("error"));
    EXPECT_TRUE(response_body["error"].get<std::string>().find("triggerCountThreshold") != std::string::npos);
}

// 测试请求验证 - 无效的指标名称格式
TEST_F(HttpServerAlarmRulesTest, ValidationInvalidMetricNameFormat) {
    json request_body = {
        {"templateId", "无效指标测试"},
        {"metricName", "invalid.metric.format"},  // 不以resource.开头
        {"alarmLevel", "严重"},
        {"triggerCountThreshold", 3},
        {"contentTemplate", "无效指标测试"},
        {"condition", {
            {"type", "GreaterThan"},
            {"threshold", 90.0}
        }},
        {"actions", {
            {{"type", "Notify"}}
        }}
    };
    
    auto response = m_client->Post("/alarm/rules", request_body.dump(), "application/json");
    
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 400);
    
    json response_body = json::parse(response->body);
    EXPECT_TRUE(response_body.contains("error"));
    EXPECT_TRUE(response_body["error"].get<std::string>().find("Invalid metric name format") != std::string::npos);
}

// 测试告警级别映射
TEST_F(HttpServerAlarmRulesTest, AlarmLevelMapping) {
    // 测试中文告警级别映射
    std::vector<std::pair<std::string, std::string>> level_mappings = {
        {"严重", "critical"},
        {"一般", "warning"},
        {"提示", "info"}
    };
    
    for (const auto& mapping : level_mappings) {
        json request_body = {
            {"templateId", "告警级别测试_" + mapping.first},
            {"metricName", "resource.cpu.usage_percent"},
            {"alarmLevel", mapping.first},
            {"triggerCountThreshold", 1},
            {"contentTemplate", "告警级别测试"},
            {"condition", {
                {"type", "GreaterThan"},
                {"threshold", 50.0}
            }},
            {"actions", {
                {{"type", "Database"}}
            }}
        };
        
        auto response = m_client->Post("/alarm/rules", request_body.dump(), "application/json");
        
        ASSERT_TRUE(response);
        EXPECT_EQ(response->status, 200);
        
        json response_body = json::parse(response->body);
        EXPECT_EQ(response_body["status"], "success");
    }
}

// 测试不同的条件类型
TEST_F(HttpServerAlarmRulesTest, DifferentConditionTypes) {
    std::vector<std::pair<std::string, std::string>> condition_types = {
        {"GreaterThan", ">"},
        {"LessThan", "<"},
        {"Equal", "=="},
        {"GreaterThanOrEqual", ">="},
        {"LessThanOrEqual", "<="}
    };
    
    for (const auto& condition_type : condition_types) {
        json request_body = {
            {"templateId", "条件类型测试_" + condition_type.first},
            {"metricName", "resource.cpu.usage_percent"},
            {"alarmLevel", "一般"},
            {"triggerCountThreshold", 1},
            {"contentTemplate", "条件类型测试"},
            {"condition", {
                {"type", condition_type.first},
                {"threshold", 50.0}
            }},
            {"actions", {
                {{"type", "Database"}}
            }}
        };
        
        auto response = m_client->Post("/alarm/rules", request_body.dump(), "application/json");
        
        ASSERT_TRUE(response);
        EXPECT_EQ(response->status, 200);
        
        json response_body = json::parse(response->body);
        EXPECT_EQ(response_body["status"], "success");
    }
}

// 测试内存指标
TEST_F(HttpServerAlarmRulesTest, MemoryMetric) {
    json request_body = {
        {"templateId", "内存使用过高"},
        {"metricName", "resource.memory.usage_percent"},
        {"alarmType", "硬件状态"},
        {"alarmLevel", "严重"},
        {"triggerCountThreshold", 3},
        {"contentTemplate", "节点 {nodeId} 内存使用率达到 {usage_percent}%"},
        {"condition", {
            {"type", "GreaterThan"},
            {"threshold", 85.0}
        }},
        {"actions", {
            {{"type", "Notify"}},
            {{"type", "Database"}}
        }}
    };
    
    auto response = m_client->Post("/alarm/rules", request_body.dump(), "application/json");
    
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
    
    json response_body = json::parse(response->body);
    EXPECT_EQ(response_body["status"], "success");
}

// 测试For Duration计算
TEST_F(HttpServerAlarmRulesTest, ForDurationCalculation) {
    json request_body = {
        {"templateId", "For Duration测试"},
        {"metricName", "resource.cpu.usage_percent"},
        {"alarmLevel", "一般"},
        {"triggerCountThreshold", 5},  // 5次 * 5秒 = 25秒
        {"contentTemplate", "For Duration测试"},
        {"condition", {
            {"type", "GreaterThan"},
            {"threshold", 50.0}
        }},
        {"actions", {
            {{"type", "Database"}}
        }}
    };
    
    auto response = m_client->Post("/alarm/rules", request_body.dump(), "application/json");
    
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
    
    json response_body = json::parse(response->body);
    EXPECT_EQ(response_body["status"], "success");
    
    // 验证规则是否正确存储 - 可以通过查询数据库或存储接口验证
    std::string rule_id = response_body["rule_id"];
    EXPECT_FALSE(rule_id.empty());
}

// 测试描述模板转换功能
TEST_F(HttpServerAlarmRulesTest, DescriptionTemplateConversion) {
    json request_body = {
        {"templateId", "模板转换测试"},
        {"metricName", "resource.cpu.usage_percent"},
        {"alarmType", "硬件状态"},
        {"alarmLevel", "一般"},
        {"triggerCountThreshold", 1},
        {"contentTemplate", "节点 {nodeId} 发生 {alarmLevel} 告警，CPU使用值为 {value}"},
        {"condition", {
            {"type", "GreaterThan"},
            {"threshold", 50.0}
        }},
        {"actions", {
            {{"type", "Database"}}
        }}
    };
    
    auto response = m_client->Post("/alarm/rules", request_body.dump(), "application/json");
    
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
    
    json response_body = json::parse(response->body);
    EXPECT_EQ(response_body["status"], "success");
    EXPECT_TRUE(response_body.contains("rule_id"));
    
    // 验证规则是否正确存储（检查description格式转换）
    std::string rule_id = response_body["rule_id"];
    auto rule = m_alarm_rule_storage->getAlarmRule(rule_id);
    EXPECT_FALSE(rule.id.empty());
    
    // 检查description中的{}应该被转换为{{}}并映射到正确的字段
    EXPECT_NE(rule.description.find("{{host_ip}}"), std::string::npos);
    EXPECT_NE(rule.description.find("{{severity}}"), std::string::npos);
    EXPECT_NE(rule.description.find("{{value}}"), std::string::npos);
    
    // 确保原始的{xxx}格式不存在
    EXPECT_EQ(rule.description.find("{nodeId}"), std::string::npos);
    EXPECT_EQ(rule.description.find("{alarmLevel}"), std::string::npos);
    EXPECT_EQ(rule.description.find("{value}"), std::string::npos);
    
    // 确保没有映射的占位符名称不存在
    EXPECT_EQ(rule.description.find("{{nodeId}}"), std::string::npos);
    EXPECT_EQ(rule.description.find("{{alarmLevel}}"), std::string::npos);
}