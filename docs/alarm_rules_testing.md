# 告警规则测试文档

## 概述

本文档描述了告警规则添加功能的测试用例，包括HTTP接口测试、指标名称解析测试和集成测试。

## 测试文件结构

### 1. `test_http_server_alarm_rules.cpp`
完整的HTTP服务器告警规则接口测试，包括：

#### 基本功能测试
- **BasicAlarmRuleCreation**: 测试基本的告警规则创建功能
- **ComplexMetricNameWithTags**: 测试复杂的带标签的指标名称
- **NetworkInterfaceMetric**: 测试网络接口指标
- **GPUMetric**: 测试GPU相关指标
- **MemoryMetric**: 测试内存指标

#### 复合条件测试
- **CompoundConditions**: 测试复合条件（And/Or逻辑）

#### 验证测试
- **ValidationMissingTemplateId**: 测试缺少模板ID的错误处理
- **ValidationMissingMetricName**: 测试缺少指标名称的错误处理
- **ValidationInvalidJSON**: 测试无效JSON格式的错误处理
- **ValidationInvalidTriggerCount**: 测试无效触发次数的错误处理
- **ValidationInvalidMetricNameFormat**: 测试无效指标名称格式的错误处理

#### 功能特性测试
- **AlarmLevelMapping**: 测试告警级别映射（中文到英文）
- **DifferentConditionTypes**: 测试不同的条件类型
- **ForDurationCalculation**: 测试持续时间计算

### 2. `test_metric_name_parsing.cpp`
专门测试指标名称解析功能，包括：

#### 简单指标测试
- **SimpleCPUMetric**: 测试简单的CPU指标解析
- **SimpleMemoryMetric**: 测试简单的内存指标解析

#### 复杂指标测试
- **DiskMetricWithMountPoint**: 测试带挂载点的磁盘指标
- **NetworkMetricWithInterface**: 测试带接口的网络指标
- **GPUMetricWithIndex**: 测试带索引的GPU指标

#### 条件类型测试
- **AllConditionTypes**: 测试所有支持的条件类型
- **CompoundConditionParsing**: 测试复合条件解析

#### 错误处理测试
- **InvalidMetricNameFormat**: 测试无效的指标名称格式
- **EdgeCases**: 测试边界条件

## 支持的指标名称格式

### 简单格式
```
resource.{stable}.{metric}
```

**示例:**
- `resource.cpu.usage_percent`
- `resource.memory.usage_percent`
- `resource.memory.free`

### 复杂格式（带标签）
```
resource.{stable}[{tag}={value}].{metric}
```

**示例:**
- `resource.disk[mount_point=/].usage_percent`
- `resource.network[interface=eth0].rx_bytes`
- `resource.gpu[gpu_index=0].compute_usage`

## 支持的条件类型

| API条件类型 | 内部操作符 | 说明 |
|-------------|------------|------|
| `GreaterThan` | `>` | 大于 |
| `LessThan` | `<` | 小于 |
| `Equal` | `==` | 等于 |
| `GreaterThanOrEqual` | `>=` | 大于等于 |
| `LessThanOrEqual` | `<=` | 小于等于 |

## 复合条件支持

### And条件
```json
{
  "type": "And",
  "conditions": [
    {
      "type": "GreaterThan",
      "threshold": 60.0
    },
    {
      "type": "LessThan",
      "threshold": 90.0
    }
  ]
}
```

### Or条件
```json
{
  "type": "Or",
  "conditions": [
    {
      "type": "GreaterThan",
      "threshold": 90.0
    },
    {
      "type": "LessThan",
      "threshold": 10.0
    }
  ]
}
```

## 告警级别映射

| 中文级别 | 英文级别 |
|----------|----------|
| 严重 | critical |
| 一般 | warning |
| 提示 | info |

## 表达式转换示例

### 简单指标转换
**输入:**
```json
{
  "metricName": "resource.cpu.usage_percent",
  "condition": {
    "type": "GreaterThan",
    "threshold": 90.0
  }
}
```

**输出:**
```json
{
  "stable": "cpu",
  "tag": "usage_percent",
  "operator": ">",
  "value": 90.0
}
```

### 复杂指标转换
**输入:**
```json
{
  "metricName": "resource.disk[mount_point=/].usage_percent",
  "condition": {
    "type": "GreaterThan",
    "threshold": 90.0
  }
}
```

**输出:**
```json
{
  "and": [
    {
      "stable": "disk",
      "tag": "mount_point",
      "operator": "==",
      "threshold": "/"
    },
    {
      "stable": "disk",
      "tag": "usage_percent",
      "operator": ">",
      "value": 90.0
    }
  ]
}
```

## 运行测试

### 构建和运行所有测试
```bash
cd build
cmake ..
make
make test
```

### 运行特定测试
```bash
# 运行HTTP服务器告警规则测试
./tests --gtest_filter="HttpServerAlarmRulesTest.*"

# 运行指标名称解析测试
./tests --gtest_filter="MetricNameParsingTest.*"

# 运行单个测试用例
./tests --gtest_filter="HttpServerAlarmRulesTest.BasicAlarmRuleCreation"
```

### 使用测试脚本
```bash
./run_alarm_tests.sh
```

## 测试依赖

### 外部依赖
- **TDengine**: 时序数据库，用于资源数据存储
- **MySQL**: 关系数据库，用于告警规则存储
- **GoogleTest**: 测试框架
- **httplib**: HTTP客户端/服务器库

### 内部依赖
- **ResourceStorage**: 资源数据存储组件
- **AlarmRuleStorage**: 告警规则存储组件
- **HttpServer**: HTTP服务器组件

## 测试覆盖范围

### 功能测试
- ✅ 基本告警规则创建
- ✅ 复杂指标名称解析
- ✅ 条件类型转换
- ✅ 复合条件处理
- ✅ 告警级别映射
- ✅ 持续时间计算

### 验证测试
- ✅ 必填字段验证
- ✅ 数据类型验证
- ✅ JSON格式验证
- ✅ 指标名称格式验证

### 错误处理测试
- ✅ 无效输入处理
- ✅ 数据库连接错误
- ✅ 解析错误处理
- ✅ 边界条件处理

### 集成测试
- ✅ HTTP服务器集成
- ✅ 数据库存储集成
- ✅ 完整的请求-响应流程

## 注意事项

1. **数据库连接**: 测试需要有效的TDengine和MySQL连接
2. **端口冲突**: 测试使用端口8081和8082，确保端口可用
3. **数据清理**: 测试后会自动清理测试数据
4. **并发安全**: 测试支持并发运行，但建议串行执行避免端口冲突

## 测试数据

测试使用的示例数据包括：
- CPU使用率告警规则
- 内存使用率告警规则
- 磁盘使用率告警规则
- 网络接口告警规则
- GPU使用率告警规则

所有测试数据都会在测试完成后自动清理，不会影响生产环境。