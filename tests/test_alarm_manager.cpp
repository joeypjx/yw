#include <gtest/gtest.h>
#include "alarm_manager.h"
#include "alarm_rule_engine.h"
#include "json.hpp"
#include <memory>

class AlarmManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        alarm_manager = std::make_unique<AlarmManager>("127.0.0.1", 3306, "test", "HZ715Net", "alarm_manager");
    }

    void TearDown() override {
        if (alarm_manager) {
            alarm_manager->disconnect();
        }
    }

    std::unique_ptr<AlarmManager> alarm_manager;
};

TEST_F(AlarmManagerTest, ConnectionTest) {
    EXPECT_TRUE(alarm_manager->connect());
}

TEST_F(AlarmManagerTest, DatabaseCreationTest) {
    ASSERT_TRUE(alarm_manager->connect());
    EXPECT_TRUE(alarm_manager->createDatabase());
}

TEST_F(AlarmManagerTest, EventTableCreationTest) {
    ASSERT_TRUE(alarm_manager->connect());
    ASSERT_TRUE(alarm_manager->createDatabase());
    EXPECT_TRUE(alarm_manager->createEventTable());
}

TEST_F(AlarmManagerTest, ProcessFiringAlarmEventTest) {
    ASSERT_TRUE(alarm_manager->connect());
    ASSERT_TRUE(alarm_manager->createDatabase());
    ASSERT_TRUE(alarm_manager->createEventTable());
    
    // 创建一个firing状态的告警事件
    AlarmEvent event;
    event.fingerprint = "alertname=TestHighCPU,host_ip=192.168.1.100";
    event.status = "firing";
    event.labels["alertname"] = "TestHighCPU";
    event.labels["host_ip"] = "192.168.1.100";
    event.labels["severity"] = "critical";
    event.annotations["summary"] = "CPU使用率过高";
    event.annotations["description"] = "节点 192.168.1.100 CPU使用率达到 95%";
    event.starts_at = std::chrono::system_clock::now();
    event.generator_url = "http://monitoring.example.com/graph";
    
    EXPECT_TRUE(alarm_manager->processAlarmEvent(event));
    
    // 验证事件已被存储
    std::vector<AlarmEventRecord> active_events = alarm_manager->getActiveAlarmEvents();
    EXPECT_EQ(active_events.size(), 1);
    
    if (!active_events.empty()) {
        const auto& record = active_events[0];
        EXPECT_EQ(record.fingerprint, event.fingerprint);
        EXPECT_EQ(record.status, "firing");
        EXPECT_FALSE(record.labels_json.empty());
        EXPECT_FALSE(record.annotations_json.empty());
        EXPECT_TRUE(record.ends_at.empty()); // firing状态没有结束时间
    }
}

TEST_F(AlarmManagerTest, ProcessResolvedAlarmEventTest) {
    ASSERT_TRUE(alarm_manager->connect());
    ASSERT_TRUE(alarm_manager->createDatabase());
    ASSERT_TRUE(alarm_manager->createEventTable());
    
    // 首先创建一个firing状态的告警事件
    AlarmEvent firing_event;
    firing_event.fingerprint = "alertname=TestHighCPU,host_ip=192.168.1.100";
    firing_event.status = "firing";
    firing_event.labels["alertname"] = "TestHighCPU";
    firing_event.labels["host_ip"] = "192.168.1.100";
    firing_event.labels["severity"] = "critical";
    firing_event.annotations["summary"] = "CPU使用率过高";
    firing_event.annotations["description"] = "节点 192.168.1.100 CPU使用率达到 95%";
    firing_event.starts_at = std::chrono::system_clock::now();
    
    ASSERT_TRUE(alarm_manager->processAlarmEvent(firing_event));
    
    // 验证firing状态的事件存在
    std::vector<AlarmEventRecord> active_events = alarm_manager->getActiveAlarmEvents();
    EXPECT_EQ(active_events.size(), 1);
    
    // 创建一个resolved状态的告警事件
    AlarmEvent resolved_event;
    resolved_event.fingerprint = firing_event.fingerprint;
    resolved_event.status = "resolved";
    resolved_event.labels = firing_event.labels;
    resolved_event.annotations = firing_event.annotations;
    resolved_event.starts_at = firing_event.starts_at;
    resolved_event.ends_at = std::chrono::system_clock::now();
    
    EXPECT_TRUE(alarm_manager->processAlarmEvent(resolved_event));
    
    // 验证活跃告警数量减少
    active_events = alarm_manager->getActiveAlarmEvents();
    EXPECT_EQ(active_events.size(), 0);
    
    // 验证按指纹查询可以找到resolved状态的事件
    std::vector<AlarmEventRecord> fingerprint_events = 
        alarm_manager->getAlarmEventsByFingerprint(firing_event.fingerprint);
    EXPECT_EQ(fingerprint_events.size(), 1);
    
    if (!fingerprint_events.empty()) {
        const auto& record = fingerprint_events[0];
        EXPECT_EQ(record.status, "resolved");
        EXPECT_FALSE(record.ends_at.empty()); // resolved状态有结束时间
    }
}

TEST_F(AlarmManagerTest, GetActiveAlarmCountTest) {
    ASSERT_TRUE(alarm_manager->connect());
    ASSERT_TRUE(alarm_manager->createDatabase());
    ASSERT_TRUE(alarm_manager->createEventTable());
    
    // 初始应该没有活跃告警
    EXPECT_EQ(alarm_manager->getActiveAlarmCount(), 0);
    
    // 添加一个firing状态的告警
    AlarmEvent event;
    event.fingerprint = "alertname=TestAlert1,host_ip=192.168.1.100";
    event.status = "firing";
    event.labels["alertname"] = "TestAlert1";
    event.labels["host_ip"] = "192.168.1.100";
    event.annotations["summary"] = "测试告警1";
    event.starts_at = std::chrono::system_clock::now();
    
    ASSERT_TRUE(alarm_manager->processAlarmEvent(event));
    EXPECT_EQ(alarm_manager->getActiveAlarmCount(), 1);
    
    // 添加另一个firing状态的告警
    event.fingerprint = "alertname=TestAlert2,host_ip=192.168.1.101";
    event.labels["alertname"] = "TestAlert2";
    event.labels["host_ip"] = "192.168.1.101";
    event.annotations["summary"] = "测试告警2";
    
    ASSERT_TRUE(alarm_manager->processAlarmEvent(event));
    EXPECT_EQ(alarm_manager->getActiveAlarmCount(), 2);
}

TEST_F(AlarmManagerTest, GetRecentAlarmEventsTest) {
    ASSERT_TRUE(alarm_manager->connect());
    ASSERT_TRUE(alarm_manager->createDatabase());
    ASSERT_TRUE(alarm_manager->createEventTable());
    
    // 添加多个告警事件
    for (int i = 0; i < 5; i++) {
        AlarmEvent event;
        event.fingerprint = "alertname=TestAlert" + std::to_string(i) + ",host_ip=192.168.1.100";
        event.status = "firing";
        event.labels["alertname"] = "TestAlert" + std::to_string(i);
        event.labels["host_ip"] = "192.168.1.100";
        event.annotations["summary"] = "测试告警" + std::to_string(i);
        event.starts_at = std::chrono::system_clock::now();
        
        ASSERT_TRUE(alarm_manager->processAlarmEvent(event));
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 确保时间戳不同
    }
    
    // 获取最近的3个事件
    std::vector<AlarmEventRecord> recent_events = alarm_manager->getRecentAlarmEvents(3);
    EXPECT_EQ(recent_events.size(), 3);
    
    // 验证事件是按时间倒序排列的
    if (recent_events.size() >= 2) {
        EXPECT_GE(recent_events[0].created_at, recent_events[1].created_at);
    }
}

TEST_F(AlarmManagerTest, GetTotalAlarmCountTest) {
    ASSERT_TRUE(alarm_manager->connect());
    ASSERT_TRUE(alarm_manager->createDatabase());
    ASSERT_TRUE(alarm_manager->createEventTable());
    
    // 初始应该没有告警
    EXPECT_EQ(alarm_manager->getTotalAlarmCount(), 0);
    
    // 添加多个告警事件
    for (int i = 0; i < 3; i++) {
        AlarmEvent event;
        event.fingerprint = "alertname=TestAlert" + std::to_string(i) + ",host_ip=192.168.1.100";
        event.status = "firing";
        event.labels["alertname"] = "TestAlert" + std::to_string(i);
        event.labels["host_ip"] = "192.168.1.100";
        event.annotations["summary"] = "测试告警" + std::to_string(i);
        event.starts_at = std::chrono::system_clock::now();
        
        ASSERT_TRUE(alarm_manager->processAlarmEvent(event));
    }
    
    EXPECT_EQ(alarm_manager->getTotalAlarmCount(), 3);
    
    // 解决一个告警
    AlarmEvent resolved_event;
    resolved_event.fingerprint = "alertname=TestAlert0,host_ip=192.168.1.100";
    resolved_event.status = "resolved";
    resolved_event.labels["alertname"] = "TestAlert0";
    resolved_event.annotations["summary"] = "测试告警0";
    resolved_event.starts_at = std::chrono::system_clock::now();
    resolved_event.ends_at = std::chrono::system_clock::now();
    
    ASSERT_TRUE(alarm_manager->processAlarmEvent(resolved_event));
    
    // 总数应该保持不变，因为我们只是更新了状态
    EXPECT_EQ(alarm_manager->getTotalAlarmCount(), 3);
    
    // 但活跃告警数应该减少
    EXPECT_EQ(alarm_manager->getActiveAlarmCount(), 2);
}

TEST_F(AlarmManagerTest, AlarmEventRecordStructTest) {
    // 测试AlarmEventRecord结构体
    AlarmEventRecord record;
    record.id = "test-id";
    record.fingerprint = "test-fingerprint";
    record.status = "firing";
    record.labels_json = "{\"alertname\":\"test\"}";
    record.annotations_json = "{\"summary\":\"test\"}";
    record.starts_at = "2023-01-01 12:00:00";
    record.ends_at = "";
    record.generator_url = "http://test.com";
    record.created_at = "2023-01-01 12:00:00";
    record.updated_at = "2023-01-01 12:00:00";
    
    EXPECT_EQ(record.id, "test-id");
    EXPECT_EQ(record.fingerprint, "test-fingerprint");
    EXPECT_EQ(record.status, "firing");
    EXPECT_EQ(record.labels_json, "{\"alertname\":\"test\"}");
    EXPECT_EQ(record.annotations_json, "{\"summary\":\"test\"}");
    EXPECT_EQ(record.starts_at, "2023-01-01 12:00:00");
    EXPECT_EQ(record.ends_at, "");
    EXPECT_EQ(record.generator_url, "http://test.com");
    EXPECT_EQ(record.created_at, "2023-01-01 12:00:00");
    EXPECT_EQ(record.updated_at, "2023-01-01 12:00:00");
}

TEST_F(AlarmManagerTest, InvalidConnectionTest) {
    auto invalid_manager = std::make_unique<AlarmManager>("invalid_host", 3306, "test", "HZ715Net", "alarm_manager");
    EXPECT_FALSE(invalid_manager->connect());
}