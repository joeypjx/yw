#include <gtest/gtest.h>
#include "alarm_rule_storage.h"
#include "json.hpp"

class AlarmRuleStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        storage = std::make_unique<AlarmRuleStorage>("127.0.0.1", 3306, "test", "HZ715Net", "alarm");
    }

    void TearDown() override {
        if (storage) {
            storage->disconnect();
        }
    }

    std::unique_ptr<AlarmRuleStorage> storage;
};

TEST_F(AlarmRuleStorageTest, ConnectionTest) {
    EXPECT_TRUE(storage->connect());
}

TEST_F(AlarmRuleStorageTest, DatabaseCreationTest) {
    ASSERT_TRUE(storage->connect());
    EXPECT_TRUE(storage->createDatabase());
}

TEST_F(AlarmRuleStorageTest, TableCreationTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase());
    EXPECT_TRUE(storage->createTable());
}

TEST_F(AlarmRuleStorageTest, InsertAlarmRuleTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase());
    ASSERT_TRUE(storage->createTable());

    // Create a sample alarm rule with new format (no agg_func)
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

    std::string id = storage->insertAlarmRule(
        "HighCpuOrLoadOnSpecificHost",
        expression,
        "5m",
        "critical",
        "特定主机CPU或负载过高",
        "节点 {{host_ip}} 资源使用率异常。CPU: {{usage_percent}}%, 负载: {{load_avg_1m}}。"
    );

    EXPECT_FALSE(id.empty());
}

TEST_F(AlarmRuleStorageTest, GetAlarmRuleTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase());
    ASSERT_TRUE(storage->createTable());

    // Create a simple alarm rule with new format
    nlohmann::json expression = {
        {"stable", "cpu_metrics"},
        {"metric", "usage_percent"},
        {"operator", ">"},
        {"threshold", 80.0}
    };

    std::string id = storage->insertAlarmRule(
        "HighCpuUsage",
        expression,
        "2m",
        "warning",
        "CPU使用率过高",
        "节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%"
    );

    ASSERT_FALSE(id.empty());

    AlarmRule rule = storage->getAlarmRule(id);
    EXPECT_EQ(rule.id, id);
    EXPECT_EQ(rule.alert_name, "HighCpuUsage");
    EXPECT_EQ(rule.for_duration, "2m");
    EXPECT_EQ(rule.severity, "warning");
    EXPECT_EQ(rule.summary, "CPU使用率过高");
    EXPECT_TRUE(rule.enabled);
}

TEST_F(AlarmRuleStorageTest, UpdateAlarmRuleTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase());
    ASSERT_TRUE(storage->createTable());

    // Create initial alarm rule
    nlohmann::json expression = {
        {"stable", "cpu_metrics"},
        {"metric", "usage_percent"},
        {"operator", ">"},
        {"threshold", 80.0}
    };

    std::string id = storage->insertAlarmRule(
        "HighCpuUsage",
        expression,
        "2m",
        "warning",
        "CPU使用率过高",
        "节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%"
    );

    ASSERT_FALSE(id.empty());

    // Update the rule
    nlohmann::json new_expression = {
        {"stable", "cpu_metrics"},
        {"metric", "usage_percent"},
        {"operator", ">"},
        {"threshold", 90.0}
    };

    EXPECT_TRUE(storage->updateAlarmRule(
        id,
        "HighCpuUsageUpdated",
        new_expression,
        "5m",
        "critical",
        "CPU使用率过高(更新)",
        "节点 {{host_ip}} CPU使用率达到 {{usage_percent}}% (更新后的规则)",
        false
    ));

    // Verify the update
    AlarmRule updated_rule = storage->getAlarmRule(id);
    EXPECT_EQ(updated_rule.alert_name, "HighCpuUsageUpdated");
    EXPECT_EQ(updated_rule.for_duration, "5m");
    EXPECT_EQ(updated_rule.severity, "critical");
    EXPECT_FALSE(updated_rule.enabled);
}

TEST_F(AlarmRuleStorageTest, DeleteAlarmRuleTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase());
    ASSERT_TRUE(storage->createTable());

    // Create alarm rule
    nlohmann::json expression = {
        {"stable", "cpu_metrics"},
        {"metric", "usage_percent"},
        {"operator", ">"},
        {"threshold", 80.0}
    };

    std::string id = storage->insertAlarmRule(
        "HighCpuUsage",
        expression,
        "2m",
        "warning",
        "CPU使用率过高",
        "节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%"
    );

    ASSERT_FALSE(id.empty());

    // Delete the rule
    EXPECT_TRUE(storage->deleteAlarmRule(id));

    // Verify deletion
    AlarmRule deleted_rule = storage->getAlarmRule(id);
    EXPECT_TRUE(deleted_rule.id.empty());
}

TEST_F(AlarmRuleStorageTest, GetAllAlarmRulesTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase());
    ASSERT_TRUE(storage->createTable());

    // Create multiple alarm rules
    nlohmann::json expression1 = {
        {"stable", "cpu_metrics"},
        {"metric", "usage_percent"},
        {"operator", ">"},
        {"threshold", 80.0}
    };

    nlohmann::json expression2 = {
        {"stable", "memory_metrics"},
        {"metric", "usage_percent"},
        {"operator", ">"},
        {"threshold", 90.0}
    };

    std::string id1 = storage->insertAlarmRule(
        "HighCpuUsage",
        expression1,
        "2m",
        "warning",
        "CPU使用率过高",
        "节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%"
    );

    std::string id2 = storage->insertAlarmRule(
        "HighMemUsage",
        expression2,
        "3m",
        "critical",
        "内存使用率过高",
        "节点 {{host_ip}} 内存使用率达到 {{usage_percent}}%"
    );

    ASSERT_FALSE(id1.empty());
    ASSERT_FALSE(id2.empty());

    // Get all rules
    std::vector<AlarmRule> all_rules = storage->getAllAlarmRules();
    EXPECT_GE(all_rules.size(), 2);

    // Verify rules are present
    bool found_cpu = false, found_mem = false;
    for (const auto& rule : all_rules) {
        if (rule.id == id1) found_cpu = true;
        if (rule.id == id2) found_mem = true;
    }
    EXPECT_TRUE(found_cpu);
    EXPECT_TRUE(found_mem);
}

TEST_F(AlarmRuleStorageTest, GetEnabledAlarmRulesTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase());
    ASSERT_TRUE(storage->createTable());

    nlohmann::json expression = {
        {"stable", "cpu_metrics"},
        {"metric", "usage_percent"},
        {"operator", ">"},
        {"threshold", 80.0}
    };

    // Create enabled rule
    std::string id1 = storage->insertAlarmRule(
        "EnabledRule",
        expression,
        "2m",
        "warning",
        "启用的规则",
        "这是一个启用的规则",
        true
    );

    // Create disabled rule
    std::string id2 = storage->insertAlarmRule(
        "DisabledRule",
        expression,
        "2m",
        "warning",
        "禁用的规则",
        "这是一个禁用的规则",
        false
    );

    ASSERT_FALSE(id1.empty());
    ASSERT_FALSE(id2.empty());

    // Get enabled rules
    std::vector<AlarmRule> enabled_rules = storage->getEnabledAlarmRules();
    
    // Check that only enabled rule is returned
    bool found_enabled = false;
    bool found_disabled = false;
    for (const auto& rule : enabled_rules) {
        if (rule.id == id1) found_enabled = true;
        if (rule.id == id2) found_disabled = true;
    }
    EXPECT_TRUE(found_enabled);
    EXPECT_FALSE(found_disabled);
}

TEST_F(AlarmRuleStorageTest, ComplexAlarmRuleTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase());
    ASSERT_TRUE(storage->createTable());

    // Create a complex alarm rule with new format
    nlohmann::json complex_expression = {
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

    std::string id = storage->insertAlarmRule(
        "HighCpuOrLoadOnSpecificHost",
        complex_expression,
        "5m",
        "critical",
        "特定主机CPU或负载过高",
        "节点 {{host_ip}} 资源使用率异常。CPU: {{usage_percent}}%, 负载: {{load_avg_1m}}。"
    );

    ASSERT_FALSE(id.empty());

    // Verify the complex rule was stored correctly
    AlarmRule rule = storage->getAlarmRule(id);
    EXPECT_EQ(rule.alert_name, "HighCpuOrLoadOnSpecificHost");
    EXPECT_EQ(rule.for_duration, "5m");
    EXPECT_EQ(rule.severity, "critical");
    
    // Parse the stored JSON to verify structure
    nlohmann::json stored_expression = nlohmann::json::parse(rule.expression_json);
    EXPECT_TRUE(stored_expression.contains("and"));
    EXPECT_TRUE(stored_expression["and"].is_array());
    EXPECT_EQ(stored_expression["and"].size(), 2);
}

TEST_F(AlarmRuleStorageTest, InvalidConnectionTest) {
    auto invalid_storage = std::make_unique<AlarmRuleStorage>("invalid_host", 3306, "test", "HZ715Net", "alarm");
    EXPECT_FALSE(invalid_storage->connect());
}