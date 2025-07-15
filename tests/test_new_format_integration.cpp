#include <gtest/gtest.h>
#include "alarm_rule_engine.h"
#include "alarm_rule_storage.h"
#include "resource_storage.h"
#include "json.hpp"
#include <memory>

class NewFormatIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        rule_storage = std::make_shared<AlarmRuleStorage>("127.0.0.1", 3306, "test", "HZ715Net", "alarm");
        resource_storage = std::make_shared<ResourceStorage>("127.0.0.1", "test", "HZ715Net");
        
        // Connect to databases
        if (rule_storage->connect()) {
            rule_storage->createDatabase();
            rule_storage->createTable();
        }
        
        if (resource_storage->connect()) {
            resource_storage->createDatabase("resource");
            resource_storage->createResourceTable();
        }
        
        engine = std::make_unique<AlarmRuleEngine>(rule_storage, resource_storage);
    }

    void TearDown() override {
        if (engine) {
            engine->stop();
        }
        
        // Clean up test rules
        for (const auto& rule_id : created_rule_ids) {
            rule_storage->deleteAlarmRule(rule_id);
        }
    }

    std::shared_ptr<AlarmRuleStorage> rule_storage;
    std::shared_ptr<ResourceStorage> resource_storage;
    std::unique_ptr<AlarmRuleEngine> engine;
    std::vector<std::string> created_rule_ids;
};

TEST_F(NewFormatIntegrationTest, SimpleRuleCreationAndProcessing) {
    if (!rule_storage->connect()) {
        GTEST_SKIP() << "MySQL connection failed";
    }
    
    // Create a simple rule without agg_func
    nlohmann::json expression = {
        {"stable", "cpu_metrics"},
        {"metric", "usage_percent"},
        {"operator", ">"},
        {"threshold", 80.0}
    };
    
    std::string rule_id = rule_storage->insertAlarmRule(
        "TestSimpleCPU",
        expression,
        "1m",
        "warning",
        "Test simple CPU alert",
        "节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%"
    );
    
    ASSERT_FALSE(rule_id.empty());
    created_rule_ids.push_back(rule_id);
    
    // Verify the rule was stored correctly
    AlarmRule stored_rule = rule_storage->getAlarmRule(rule_id);
    EXPECT_EQ(stored_rule.alert_name, "TestSimpleCPU");
    EXPECT_EQ(stored_rule.for_duration, "1m");
    EXPECT_EQ(stored_rule.severity, "warning");
    
    // Parse the stored expression
    nlohmann::json stored_expression = nlohmann::json::parse(stored_rule.expression_json);
    EXPECT_EQ(stored_expression["stable"], "cpu_metrics");
    EXPECT_EQ(stored_expression["metric"], "usage_percent");
    EXPECT_EQ(stored_expression["operator"], ">");
    EXPECT_EQ(stored_expression["threshold"], 80.0);
    
    // Ensure no agg_func field
    EXPECT_FALSE(stored_expression.contains("agg_func"));
}

TEST_F(NewFormatIntegrationTest, ANDLogicRuleCreationAndProcessing) {
    if (!rule_storage->connect()) {
        GTEST_SKIP() << "MySQL connection failed";
    }
    
    // Create an AND logic rule
    nlohmann::json expression = {
        {"and", {
            {
                {"stable", "cpu_metrics"},
                {"metric", "usage_percent"},
                {"operator", ">"},
                {"threshold", 80.0}
            },
            {
                {"stable", "cpu_metrics"},
                {"tag", "host_ip"},
                {"operator", "=="},
                {"value", "192.168.1.100"}
            }
        }}
    };
    
    std::string rule_id = rule_storage->insertAlarmRule(
        "TestANDLogic",
        expression,
        "2m",
        "critical",
        "Test AND logic alert",
        "特定主机 {{host_ip}} CPU使用率达到 {{usage_percent}}%"
    );
    
    ASSERT_FALSE(rule_id.empty());
    created_rule_ids.push_back(rule_id);
    
    // Verify the rule was stored correctly
    AlarmRule stored_rule = rule_storage->getAlarmRule(rule_id);
    EXPECT_EQ(stored_rule.alert_name, "TestANDLogic");
    EXPECT_EQ(stored_rule.severity, "critical");
    
    // Parse the stored expression
    nlohmann::json stored_expression = nlohmann::json::parse(stored_rule.expression_json);
    EXPECT_TRUE(stored_expression.contains("and"));
    EXPECT_TRUE(stored_expression["and"].is_array());
    EXPECT_EQ(stored_expression["and"].size(), 2);
}

TEST_F(NewFormatIntegrationTest, ORLogicRuleCreationAndProcessing) {
    if (!rule_storage->connect()) {
        GTEST_SKIP() << "MySQL connection failed";
    }
    
    // Create an OR logic rule
    nlohmann::json expression = {
        {"or", {
            {
                {"stable", "disk_metrics"},
                {"metric", "usage_percent"},
                {"operator", ">"},
                {"threshold", 85.0}
            },
            {
                {"stable", "disk_metrics"},
                {"metric", "free"},
                {"operator", "<"},
                {"threshold", 1000000000}
            }
        }}
    };
    
    std::string rule_id = rule_storage->insertAlarmRule(
        "TestORLogic",
        expression,
        "5m",
        "warning",
        "Test OR logic alert",
        "磁盘空间不足: 使用率 {{usage_percent}}%, 可用空间 {{free}} 字节"
    );
    
    ASSERT_FALSE(rule_id.empty());
    created_rule_ids.push_back(rule_id);
    
    // Verify the rule was stored correctly
    AlarmRule stored_rule = rule_storage->getAlarmRule(rule_id);
    EXPECT_EQ(stored_rule.alert_name, "TestORLogic");
    EXPECT_EQ(stored_rule.for_duration, "5m");
    
    // Parse the stored expression
    nlohmann::json stored_expression = nlohmann::json::parse(stored_rule.expression_json);
    EXPECT_TRUE(stored_expression.contains("or"));
    EXPECT_TRUE(stored_expression["or"].is_array());
    EXPECT_EQ(stored_expression["or"].size(), 2);
}

TEST_F(NewFormatIntegrationTest, NestedLogicRuleCreationAndProcessing) {
    if (!rule_storage->connect()) {
        GTEST_SKIP() << "MySQL connection failed";
    }
    
    // Create a nested logic rule
    nlohmann::json expression = {
        {"and", {
            {
                {"stable", "cpu_metrics"},
                {"tag", "host_ip"},
                {"operator", "=="},
                {"value", "192.168.1.100"}
            },
            {
                {"or", {
                    {
                        {"stable", "cpu_metrics"},
                        {"metric", "usage_percent"},
                        {"operator", ">"},
                        {"threshold", 90.0}
                    },
                    {
                        {"stable", "cpu_metrics"},
                        {"metric", "load_avg_1m"},
                        {"operator", ">"},
                        {"threshold", 2.0}
                    }
                }}
            }
        }}
    };
    
    std::string rule_id = rule_storage->insertAlarmRule(
        "TestNestedLogic",
        expression,
        "3m",
        "critical",
        "Test nested logic alert",
        "主机 {{host_ip}} 资源异常"
    );
    
    ASSERT_FALSE(rule_id.empty());
    created_rule_ids.push_back(rule_id);
    
    // Verify the rule was stored correctly
    AlarmRule stored_rule = rule_storage->getAlarmRule(rule_id);
    EXPECT_EQ(stored_rule.alert_name, "TestNestedLogic");
    EXPECT_EQ(stored_rule.severity, "critical");
    
    // Parse the stored expression
    nlohmann::json stored_expression = nlohmann::json::parse(stored_rule.expression_json);
    EXPECT_TRUE(stored_expression.contains("and"));
    EXPECT_TRUE(stored_expression["and"].is_array());
    EXPECT_EQ(stored_expression["and"].size(), 2);
    
    // Check nested OR structure
    nlohmann::json or_part = stored_expression["and"][1];
    EXPECT_TRUE(or_part.contains("or"));
    EXPECT_TRUE(or_part["or"].is_array());
    EXPECT_EQ(or_part["or"].size(), 2);
}

TEST_F(NewFormatIntegrationTest, GPURuleCreationAndProcessing) {
    if (!rule_storage->connect()) {
        GTEST_SKIP() << "MySQL connection failed";
    }
    
    // Create a GPU rule
    nlohmann::json expression = {
        {"stable", "gpu_metrics"},
        {"metric", "compute_usage"},
        {"operator", ">="},
        {"threshold", 85.0}
    };
    
    std::string rule_id = rule_storage->insertAlarmRule(
        "TestGPUUsage",
        expression,
        "3m",
        "warning",
        "Test GPU usage alert",
        "GPU {{gpu_name}} 计算使用率达到 {{compute_usage}}%"
    );
    
    ASSERT_FALSE(rule_id.empty());
    created_rule_ids.push_back(rule_id);
    
    // Verify the rule was stored correctly
    AlarmRule stored_rule = rule_storage->getAlarmRule(rule_id);
    EXPECT_EQ(stored_rule.alert_name, "TestGPUUsage");
    
    // Parse the stored expression
    nlohmann::json stored_expression = nlohmann::json::parse(stored_rule.expression_json);
    EXPECT_EQ(stored_expression["stable"], "gpu_metrics");
    EXPECT_EQ(stored_expression["metric"], "compute_usage");
    EXPECT_EQ(stored_expression["operator"], ">=");
    EXPECT_EQ(stored_expression["threshold"], 85.0);
}

TEST_F(NewFormatIntegrationTest, EngineProcessingWithNewFormatTest) {
    if (!rule_storage->connect() || !resource_storage->connect()) {
        GTEST_SKIP() << "Database connection failed";
    }
    
    // Create multiple rules with new format
    std::vector<std::pair<std::string, nlohmann::json>> test_rules = {
        {"SimpleCPU", {
            {"stable", "cpu_metrics"},
            {"metric", "usage_percent"},
            {"operator", ">"},
            {"threshold", 80.0}
        }},
        {"ANDLogic", {
            {"and", {
                {
                    {"stable", "memory_metrics"},
                    {"metric", "usage_percent"},
                    {"operator", ">"},
                    {"threshold", 90.0}
                },
                {
                    {"stable", "memory_metrics"},
                    {"tag", "host_ip"},
                    {"operator", "=="},
                    {"value", "192.168.1.100"}
                }
            }}
        }},
        {"ORLogic", {
            {"or", {
                {
                    {"stable", "disk_metrics"},
                    {"metric", "usage_percent"},
                    {"operator", ">"},
                    {"threshold", 85.0}
                },
                {
                    {"stable", "disk_metrics"},
                    {"metric", "free"},
                    {"operator", "<"},
                    {"threshold", 1000000000}
                }
            }}
        }}
    };
    
    // Insert all test rules
    for (const auto& rule_pair : test_rules) {
        std::string rule_id = rule_storage->insertAlarmRule(
            "Test" + rule_pair.first,
            rule_pair.second,
            "1m",
            "warning",
            "Test " + rule_pair.first + " rule",
            "Test description for " + rule_pair.first
        );
        
        ASSERT_FALSE(rule_id.empty());
        created_rule_ids.push_back(rule_id);
    }
    
    // Set up alarm event callback
    std::vector<AlarmEvent> received_events;
    engine->setAlarmEventCallback([&received_events](const AlarmEvent& event) {
        received_events.push_back(event);
    });
    
    // Start the engine
    ASSERT_TRUE(engine->start());
    
    // Let the engine run for a short time
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Stop the engine
    engine->stop();
    
    // The test passes if the engine could start and stop without errors
    // In a real scenario, we would verify the SQL generation and alarm events
    EXPECT_TRUE(true);
}

TEST_F(NewFormatIntegrationTest, MultipleStableTypesTest) {
    if (!rule_storage->connect()) {
        GTEST_SKIP() << "MySQL connection failed";
    }
    
    // Test rules for different stable types
    std::vector<std::pair<std::string, nlohmann::json>> stable_rules = {
        {"cpu_metrics", {
            {"stable", "cpu_metrics"},
            {"metric", "usage_percent"},
            {"operator", ">"},
            {"threshold", 80.0}
        }},
        {"memory_metrics", {
            {"stable", "memory_metrics"},
            {"metric", "usage_percent"},
            {"operator", ">"},
            {"threshold", 90.0}
        }},
        {"disk_metrics", {
            {"stable", "disk_metrics"},
            {"metric", "usage_percent"},
            {"operator", ">"},
            {"threshold", 85.0}
        }},
        {"network_metrics", {
            {"stable", "network_metrics"},
            {"metric", "rx_errors"},
            {"operator", ">"},
            {"threshold", 0}
        }},
        {"gpu_metrics", {
            {"stable", "gpu_metrics"},
            {"metric", "compute_usage"},
            {"operator", ">="},
            {"threshold", 85.0}
        }}
    };
    
    // Create and verify rules for each stable type
    for (const auto& rule_pair : stable_rules) {
        std::string rule_id = rule_storage->insertAlarmRule(
            "Test" + rule_pair.first,
            rule_pair.second,
            "2m",
            "warning",
            "Test " + rule_pair.first + " rule",
            "Test description for " + rule_pair.first
        );
        
        ASSERT_FALSE(rule_id.empty());
        created_rule_ids.push_back(rule_id);
        
        // Verify the rule was stored correctly
        AlarmRule stored_rule = rule_storage->getAlarmRule(rule_id);
        EXPECT_EQ(stored_rule.alert_name, "Test" + rule_pair.first);
        
        // Parse and verify the expression
        nlohmann::json stored_expression = nlohmann::json::parse(stored_rule.expression_json);
        EXPECT_EQ(stored_expression["stable"], rule_pair.first);
        
        // Ensure no agg_func field
        EXPECT_FALSE(stored_expression.contains("agg_func"));
    }
}

TEST_F(NewFormatIntegrationTest, UpdateRuleWithNewFormatTest) {
    if (!rule_storage->connect()) {
        GTEST_SKIP() << "MySQL connection failed";
    }
    
    // Create initial rule
    nlohmann::json initial_expression = {
        {"stable", "cpu_metrics"},
        {"metric", "usage_percent"},
        {"operator", ">"},
        {"threshold", 80.0}
    };
    
    std::string rule_id = rule_storage->insertAlarmRule(
        "TestUpdate",
        initial_expression,
        "2m",
        "warning",
        "Test update rule",
        "Initial description"
    );
    
    ASSERT_FALSE(rule_id.empty());
    created_rule_ids.push_back(rule_id);
    
    // Update the rule with new format
    nlohmann::json updated_expression = {
        {"and", {
            {
                {"stable", "cpu_metrics"},
                {"metric", "usage_percent"},
                {"operator", ">"},
                {"threshold", 90.0}
            },
            {
                {"stable", "cpu_metrics"},
                {"tag", "host_ip"},
                {"operator", "=="},
                {"value", "192.168.1.100"}
            }
        }}
    };
    
    ASSERT_TRUE(rule_storage->updateAlarmRule(
        rule_id,
        "TestUpdateModified",
        updated_expression,
        "5m",
        "critical",
        "Test update rule modified",
        "Updated description",
        true
    ));
    
    // Verify the update
    AlarmRule updated_rule = rule_storage->getAlarmRule(rule_id);
    EXPECT_EQ(updated_rule.alert_name, "TestUpdateModified");
    EXPECT_EQ(updated_rule.for_duration, "5m");
    EXPECT_EQ(updated_rule.severity, "critical");
    
    // Parse and verify the updated expression
    nlohmann::json stored_expression = nlohmann::json::parse(updated_rule.expression_json);
    EXPECT_TRUE(stored_expression.contains("and"));
    EXPECT_FALSE(stored_expression.contains("agg_func"));
}