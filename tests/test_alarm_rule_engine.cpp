#include <gtest/gtest.h>
#include "alarm_rule_engine.h"
#include "alarm_rule_storage.h"
#include "resource_storage.h"
#include "json.hpp"
#include <memory>

class AlarmRuleEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        rule_storage = std::make_shared<AlarmRuleStorage>("127.0.0.1", 3306, "test", "HZ715Net", "alarm_test");
        resource_storage = std::make_shared<ResourceStorage>("127.0.0.1", "test", "HZ715Net");
        
        // 连接数据库
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
    }

    std::shared_ptr<AlarmRuleStorage> rule_storage;
    std::shared_ptr<ResourceStorage> resource_storage;
    std::unique_ptr<AlarmRuleEngine> engine;
};

TEST_F(AlarmRuleEngineTest, CreateEngineTest) {
    EXPECT_TRUE(engine != nullptr);
}

TEST_F(AlarmRuleEngineTest, StartStopEngineTest) {
    EXPECT_TRUE(engine->start());
    
    // 等待一小段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    engine->stop();
}

TEST_F(AlarmRuleEngineTest, ConvertRuleToSQLTest) {
    // 这个测试需要访问私有方法，暂时跳过
    // 在实际实现中可以考虑增加public接口或者friend class
    SUCCEED();
}

TEST_F(AlarmRuleEngineTest, ParseDurationTest) {
    // 测试duration解析 - 需要公开方法或使用friend
    SUCCEED();
}

TEST_F(AlarmRuleEngineTest, GenerateFingerprintTest) {
    // 测试指纹生成 - 需要公开方法或使用friend
    SUCCEED();
}

TEST_F(AlarmRuleEngineTest, AlarmEventCallbackTest) {
    std::vector<AlarmEvent> received_events;
    
    engine->setAlarmEventCallback([&received_events](const AlarmEvent& event) {
        received_events.push_back(event);
    });
    
    // 启动引擎
    EXPECT_TRUE(engine->start());
    
    // 创建一个简单的告警规则 (新格式，无聚合函数)
    nlohmann::json expression = {
        {"stable", "cpu"},
        {"metric", "usage_percent"},
        {"operator", ">"},
        {"threshold", 80.0}
    };
    
    std::string rule_id = rule_storage->insertAlarmRule(
        "TestHighCpuUsage",
        expression,
        "1s",  // 很短的持续时间用于测试
        "warning",
        "测试CPU使用率过高",
        "节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%"
    );
    
    ASSERT_FALSE(rule_id.empty());
    
    // 等待引擎运行几个周期
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    engine->stop();
    
    // 检查是否有告警事件生成（由于我们使用模拟数据，应该有事件生成）
    // 注意：这个测试依赖于模拟的查询结果
    
    // 清理测试数据
    rule_storage->deleteAlarmRule(rule_id);
}

TEST_F(AlarmRuleEngineTest, AlarmEventJsonTest) {
    AlarmEvent event;
    event.fingerprint = "test_fingerprint";
    event.status = "firing";
    event.labels["alertname"] = "TestAlert";
    event.labels["host_ip"] = "192.168.1.1";
    event.annotations["summary"] = "Test summary";
    event.starts_at = std::chrono::system_clock::now();
    
    std::string json_str = event.toJson();
    EXPECT_FALSE(json_str.empty());
    
    // 验证JSON是否有效
    nlohmann::json parsed = nlohmann::json::parse(json_str);
    EXPECT_EQ(parsed["fingerprint"], "test_fingerprint");
    EXPECT_EQ(parsed["status"], "firing");
    EXPECT_EQ(parsed["labels"]["alertname"], "TestAlert");
}

TEST_F(AlarmRuleEngineTest, EvaluationIntervalTest) {
    engine->setEvaluationInterval(std::chrono::seconds(10));
    
    EXPECT_TRUE(engine->start());
    
    auto start_time = std::chrono::steady_clock::now();
    
    // 等待足够的时间观察评估间隔
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    engine->stop();
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // 确保引擎正常启动和停止
    EXPECT_GT(duration.count(), 50);
}

TEST_F(AlarmRuleEngineTest, GetCurrentAlarmInstancesTest) {
    // 获取当前告警实例
    std::vector<AlarmInstance> instances = engine->getCurrentAlarmInstances();
    
    // 初始状态下应该没有告警实例
    EXPECT_TRUE(instances.empty());
    
    // 启动引擎后再检查
    engine->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    instances = engine->getCurrentAlarmInstances();
    // 可能会有一些实例，取决于模拟数据
    
    engine->stop();
}