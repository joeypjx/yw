#include <gtest/gtest.h>
#include "http_server.h"
#include "resource_storage.h"
#include "alarm_rule_storage.h"
#include "json.hpp"

using json = nlohmann::json;

// 测试类专门用于测试指标名称解析功能
class MetricNameParsingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试用的HTTP服务器实例
        m_resource_storage = std::make_shared<ResourceStorage>("127.0.0.1", "test", "HZ715Net");
        m_alarm_rule_storage = std::make_shared<AlarmRuleStorage>("127.0.0.1", 3306, "test", "HZ715Net", "alarm_test");
        
        m_server = std::make_shared<HttpServer>(m_resource_storage, m_alarm_rule_storage, "127.0.0.1", 8082);
    }
    
    void TearDown() override {
        // 清理资源
    }
    
    // 辅助函数：创建测试条件
    json createCondition(const std::string& type, double threshold) {
        json condition;
        condition["type"] = type;
        condition["threshold"] = threshold;
        return condition;
    }
    
    // 辅助函数：创建复合条件
    json createCompoundCondition(const std::string& type, const std::vector<std::pair<std::string, double>>& conditions) {
        json compound_condition;
        compound_condition["type"] = type;
        compound_condition["conditions"] = json::array();
        
        for (const auto& cond : conditions) {
            json sub_condition;
            sub_condition["type"] = cond.first;
            sub_condition["threshold"] = cond.second;
            compound_condition["conditions"].push_back(sub_condition);
        }
        
        return compound_condition;
    }
    
    std::shared_ptr<ResourceStorage> m_resource_storage;
    std::shared_ptr<AlarmRuleStorage> m_alarm_rule_storage;
    std::shared_ptr<HttpServer> m_server;
};

// 测试简单的CPU指标解析
TEST_F(MetricNameParsingTest, SimpleCPUMetric) {
    std::string metric_name = "resource.cpu.usage_percent";
    json condition = createCondition("GreaterThan", 90.0);
    
    json result = m_server->parseMetricNameAndBuildExpression(metric_name, condition);
    
    EXPECT_EQ(result["stable"], "cpu");
    EXPECT_EQ(result["metric"], "usage_percent");
    EXPECT_EQ(result["operator"], ">");
    EXPECT_EQ(result["value"], 90.0);
}

// 测试简单的内存指标解析
TEST_F(MetricNameParsingTest, SimpleMemoryMetric) {
    std::string metric_name = "resource.memory.usage_percent";
    json condition = createCondition("LessThan", 50.0);
    
    json result = m_server->parseMetricNameAndBuildExpression(metric_name, condition);
    
    EXPECT_EQ(result["stable"], "memory");
    EXPECT_EQ(result["metric"], "usage_percent");
    EXPECT_EQ(result["operator"], "<");
    EXPECT_EQ(result["value"], 50.0);
}

// 测试带标签的磁盘指标解析
TEST_F(MetricNameParsingTest, DiskMetricWithMountPoint) {
    std::string metric_name = "resource.disk[mount_point=/].usage_percent";
    json condition = createCondition("GreaterThan", 80.0);
    
    json result = m_server->parseMetricNameAndBuildExpression(metric_name, condition);
    
    EXPECT_TRUE(result.contains("and"));
    EXPECT_TRUE(result["and"].is_array());
    EXPECT_EQ(result["and"].size(), 2);
    
    // 第一个条件：标签过滤
    json tag_condition = result["and"][0];
    EXPECT_EQ(tag_condition["stable"], "disk");
    EXPECT_EQ(tag_condition["tag"], "mount_point");
    EXPECT_EQ(tag_condition["operator"], "==");
    EXPECT_EQ(tag_condition["threshold"], "/");
    
    // 第二个条件：指标条件
    json metric_condition = result["and"][1];
    EXPECT_EQ(metric_condition["stable"], "disk");
    EXPECT_EQ(metric_condition["metric"], "usage_percent");
    EXPECT_EQ(metric_condition["operator"], ">");
    EXPECT_EQ(metric_condition["value"], 80.0);
}

// 测试带标签的网络接口指标解析
TEST_F(MetricNameParsingTest, NetworkMetricWithInterface) {
    std::string metric_name = "resource.network[interface=eth0].rx_bytes";
    json condition = createCondition("GreaterThan", 1000000.0);
    
    json result = m_server->parseMetricNameAndBuildExpression(metric_name, condition);
    
    EXPECT_TRUE(result.contains("and"));
    EXPECT_TRUE(result["and"].is_array());
    EXPECT_EQ(result["and"].size(), 2);
    
    // 第一个条件：接口过滤
    json interface_condition = result["and"][0];
    EXPECT_EQ(interface_condition["stable"], "network");
    EXPECT_EQ(interface_condition["tag"], "interface");
    EXPECT_EQ(interface_condition["operator"], "==");
    EXPECT_EQ(interface_condition["threshold"], "eth0");
    
    // 第二个条件：指标条件
    json metric_condition = result["and"][1];
    EXPECT_EQ(metric_condition["stable"], "network");
    EXPECT_EQ(metric_condition["metric"], "rx_bytes");
    EXPECT_EQ(metric_condition["operator"], ">");
    EXPECT_EQ(metric_condition["value"], 1000000.0);
}

// 测试GPU指标解析
TEST_F(MetricNameParsingTest, GPUMetricWithIndex) {
    std::string metric_name = "resource.gpu[gpu_index=0].compute_usage";
    json condition = createCondition("GreaterThanOrEqual", 95.0);
    
    json result = m_server->parseMetricNameAndBuildExpression(metric_name, condition);
    
    EXPECT_TRUE(result.contains("and"));
    EXPECT_TRUE(result["and"].is_array());
    EXPECT_EQ(result["and"].size(), 2);
    
    // 第一个条件：GPU索引过滤
    json gpu_condition = result["and"][0];
    EXPECT_EQ(gpu_condition["stable"], "gpu");
    EXPECT_EQ(gpu_condition["tag"], "gpu_index");
    EXPECT_EQ(gpu_condition["operator"], "==");
    EXPECT_EQ(gpu_condition["threshold"], "0");
    
    // 第二个条件：指标条件
    json metric_condition = result["and"][1];
    EXPECT_EQ(metric_condition["stable"], "gpu");
    EXPECT_EQ(metric_condition["metric"], "compute_usage");
    EXPECT_EQ(metric_condition["operator"], ">=");
    EXPECT_EQ(metric_condition["value"], 95.0);
}

// 测试复合条件解析
TEST_F(MetricNameParsingTest, CompoundConditionParsing) {
    std::string metric_name = "resource.cpu.usage_percent";
    json condition = createCompoundCondition("And", {
        {"GreaterThan", 60.0},
        {"LessThan", 90.0}
    });
    
    json result = m_server->parseMetricNameAndBuildExpression(metric_name, condition);
    
    EXPECT_TRUE(result.contains("and"));
    EXPECT_TRUE(result["and"].is_array());
    EXPECT_EQ(result["and"].size(), 2);  // 2个子条件
    
    // 第一个条件：> 60.0
    json first_condition = result["and"][0];
    EXPECT_EQ(first_condition["stable"], "cpu");
    EXPECT_EQ(first_condition["metric"], "usage_percent");
    EXPECT_EQ(first_condition["operator"], ">");
    EXPECT_EQ(first_condition["value"], 60.0);
    
    // 第二个条件：< 90.0
    json second_condition = result["and"][1];
    EXPECT_EQ(second_condition["stable"], "cpu");
    EXPECT_EQ(second_condition["metric"], "usage_percent");
    EXPECT_EQ(second_condition["operator"], "<");
    EXPECT_EQ(second_condition["value"], 90.0);
}

// 测试所有支持的条件类型
TEST_F(MetricNameParsingTest, AllConditionTypes) {
    std::string metric_name = "resource.cpu.usage_percent";
    
    std::vector<std::pair<std::string, std::string>> condition_mappings = {
        {"GreaterThan", ">"},
        {"LessThan", "<"},
        {"Equal", "=="},
        {"GreaterThanOrEqual", ">="},
        {"LessThanOrEqual", "<="}
    };
    
    for (const auto& mapping : condition_mappings) {
        json condition = createCondition(mapping.first, 50.0);
        json result = m_server->parseMetricNameAndBuildExpression(metric_name, condition);
        
        EXPECT_EQ(result["stable"], "cpu");
        EXPECT_EQ(result["metric"], "usage_percent");
        EXPECT_EQ(result["operator"], mapping.second);
        EXPECT_EQ(result["value"], 50.0);
    }
}

// 测试无效的指标名称格式
TEST_F(MetricNameParsingTest, InvalidMetricNameFormat) {
    std::vector<std::string> invalid_metric_names = {
        "invalid.cpu.usage_percent",  // 不以resource.开头
        "resource",                   // 只有resource
        "resource.",                  // 只有resource.
        "resource.cpu",               // 缺少指标名称
        "resource.cpu[invalid",       // 不完整的标签
        "resource.cpu[=value]",       // 无效的标签格式
        "resource.cpu[]"              // 空标签
    };
    
    json condition = createCondition("GreaterThan", 50.0);
    
    for (const auto& invalid_name : invalid_metric_names) {
        EXPECT_THROW(
            m_server->parseMetricNameAndBuildExpression(invalid_name, condition),
            std::runtime_error
        ) << "应该抛出异常的指标名称: " << invalid_name;
    }
}

// 测试复杂的磁盘设备指标
TEST_F(MetricNameParsingTest, ComplexDiskDeviceMetric) {
    std::string metric_name = "resource.disk[mount_point=/var/log].usage_percent";
    json condition = createCondition("GreaterThan", 70.0);
    
    json result = m_server->parseMetricNameAndBuildExpression(metric_name, condition);
    
    EXPECT_TRUE(result.contains("and"));
    EXPECT_TRUE(result["and"].is_array());
    EXPECT_EQ(result["and"].size(), 2);
    
    // 第一个条件：挂载点过滤
    json mount_condition = result["and"][0];
    EXPECT_EQ(mount_condition["stable"], "disk");
    EXPECT_EQ(mount_condition["tag"], "mount_point");
    EXPECT_EQ(mount_condition["operator"], "==");
    EXPECT_EQ(mount_condition["threshold"], "/var/log");
    
    // 第二个条件：使用率条件
    json usage_condition = result["and"][1];
    EXPECT_EQ(usage_condition["stable"], "disk");
    EXPECT_EQ(usage_condition["metric"], "usage_percent");
    EXPECT_EQ(usage_condition["operator"], ">");
    EXPECT_EQ(usage_condition["value"], 70.0);
}

// 测试GPU内存使用率指标
TEST_F(MetricNameParsingTest, GPUMemoryUsageMetric) {
    std::string metric_name = "resource.gpu[gpu_index=1].mem_usage";
    json condition = createCondition("LessThanOrEqual", 80.0);
    
    json result = m_server->parseMetricNameAndBuildExpression(metric_name, condition);
    
    EXPECT_TRUE(result.contains("and"));
    EXPECT_TRUE(result["and"].is_array());
    EXPECT_EQ(result["and"].size(), 2);
    
    // 第一个条件：GPU索引过滤
    json gpu_condition = result["and"][0];
    EXPECT_EQ(gpu_condition["stable"], "gpu");
    EXPECT_EQ(gpu_condition["tag"], "gpu_index");
    EXPECT_EQ(gpu_condition["operator"], "==");
    EXPECT_EQ(gpu_condition["threshold"], "1");
    
    // 第二个条件：内存使用率条件
    json mem_condition = result["and"][1];
    EXPECT_EQ(mem_condition["stable"], "gpu");
    EXPECT_EQ(mem_condition["metric"], "mem_usage");
    EXPECT_EQ(mem_condition["operator"], "<=");
    EXPECT_EQ(mem_condition["value"], 80.0);
}

// 测试网络发送速率指标
TEST_F(MetricNameParsingTest, NetworkTxRateMetric) {
    std::string metric_name = "resource.network[interface=enp0s3].tx_rate";
    json condition = createCondition("Equal", 1000000.0);
    
    json result = m_server->parseMetricNameAndBuildExpression(metric_name, condition);
    
    EXPECT_TRUE(result.contains("and"));
    EXPECT_TRUE(result["and"].is_array());
    EXPECT_EQ(result["and"].size(), 2);
    
    // 第一个条件：网络接口过滤
    json interface_condition = result["and"][0];
    EXPECT_EQ(interface_condition["stable"], "network");
    EXPECT_EQ(interface_condition["tag"], "interface");
    EXPECT_EQ(interface_condition["operator"], "==");
    EXPECT_EQ(interface_condition["threshold"], "enp0s3");
    
    // 第二个条件：发送速率条件
    json rate_condition = result["and"][1];
    EXPECT_EQ(rate_condition["stable"], "network");
    EXPECT_EQ(rate_condition["metric"], "tx_rate");
    EXPECT_EQ(rate_condition["operator"], "==");
    EXPECT_EQ(rate_condition["value"], 1000000.0);
}

// 测试边界条件
TEST_F(MetricNameParsingTest, EdgeCases) {
    // 测试阈值为0的情况
    std::string metric_name = "resource.cpu.usage_percent";
    json condition = createCondition("GreaterThan", 0.0);
    
    json result = m_server->parseMetricNameAndBuildExpression(metric_name, condition);
    
    EXPECT_EQ(result["stable"], "cpu");
    EXPECT_EQ(result["metric"], "usage_percent");
    EXPECT_EQ(result["operator"], ">");
    EXPECT_EQ(result["value"], 0.0);
    
    // 测试负数阈值
    condition = createCondition("LessThan", -10.0);
    result = m_server->parseMetricNameAndBuildExpression(metric_name, condition);
    
    EXPECT_EQ(result["stable"], "cpu");
    EXPECT_EQ(result["metric"], "usage_percent");
    EXPECT_EQ(result["operator"], "<");
    EXPECT_EQ(result["value"], -10.0);
    
    // 测试很大的阈值
    condition = createCondition("GreaterThan", 999999.99);
    result = m_server->parseMetricNameAndBuildExpression(metric_name, condition);
    
    EXPECT_EQ(result["stable"], "cpu");
    EXPECT_EQ(result["metric"], "usage_percent");
    EXPECT_EQ(result["operator"], ">");
    EXPECT_EQ(result["value"], 999999.99);
}

// 测试描述模板转换功能
TEST_F(MetricNameParsingTest, DescriptionTemplateConversion) {
    // 测试单个占位符映射
    std::string template1 = "节点 {nodeId} 发生告警";
    std::string result1 = m_server->convertDescriptionTemplate(template1);
    EXPECT_EQ(result1, "节点 {{host_ip}} 发生告警");
    
    // 测试多个占位符映射
    std::string template2 = "节点 {nodeId} 发生 {alarmLevel} 告警，CPU使用值为 {value}";
    std::string result2 = m_server->convertDescriptionTemplate(template2);
    EXPECT_EQ(result2, "节点 {{host_ip}} 发生 {{severity}} 告警，CPU使用值为 {{value}}");
    
    // 测试没有占位符的情况
    std::string template3 = "这是一个没有占位符的模板";
    std::string result3 = m_server->convertDescriptionTemplate(template3);
    EXPECT_EQ(result3, "这是一个没有占位符的模板");
    
    // 测试空字符串
    std::string template4 = "";
    std::string result4 = m_server->convertDescriptionTemplate(template4);
    EXPECT_EQ(result4, "");
    
    // 测试未映射的占位符（应该保持原样但转换为{{}}格式）
    std::string template5 = "磁盘 {device} 在挂载点 {mount_point} 的使用率为 {usage_percent}%";
    std::string result5 = m_server->convertDescriptionTemplate(template5);
    EXPECT_EQ(result5, "磁盘 {{device}} 在挂载点 {{mount_point}} 的使用率为 {{usage_percent}}%");
    
    // 测试所有映射关系
    std::string template6 = "节点 {nodeId} 发生 {alarmLevel} 告警，值为 {value}";
    std::string result6 = m_server->convertDescriptionTemplate(template6);
    EXPECT_EQ(result6, "节点 {{host_ip}} 发生 {{severity}} 告警，值为 {{value}}");
}