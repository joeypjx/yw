# 节点状态监控功能

## 概述

节点状态监控功能是一个主动监控系统，用于实时检测集群中节点的在线状态。当节点在5秒内没有发送心跳时，系统会自动将其标记为离线并触发告警。

## 功能特性

### 1. 主动监控
- **实时检测**: 每秒检查一次所有节点的最后心跳时间
- **自动告警**: 当节点超过5秒无心跳时自动触发告警
- **自动恢复**: 当节点重新发送心跳时自动解决告警

### 2. 告警机制
- **离线告警**: 节点离线时创建 "NodeOffline" 告警
- **恢复通知**: 节点恢复时自动解决告警
- **唯一标识**: 每个节点的告警都有唯一的指纹标识

### 3. 配置参数
- **检查间隔**: 1秒（可配置）
- **离线阈值**: 5秒（可配置）
- **告警级别**: Critical

## 架构设计

### 核心组件

#### NodeStatusMonitor
```cpp
class NodeStatusMonitor {
public:
    NodeStatusMonitor(std::shared_ptr<NodeStorage> node_storage, 
                     std::shared_ptr<AlarmManager> alarm_manager);
    
    void start();  // 启动监控
    void stop();   // 停止监控
};
```

#### 工作流程
```
1. 获取所有节点列表
   ↓
2. 检查每个节点的最后心跳时间
   ↓
3. 计算时间差
   ↓
4. 判断节点状态
   ├─ 在线: 解决相关告警
   └─ 离线: 创建离线告警
```

### 告警事件结构

#### 离线告警
```json
{
  "fingerprint": "alertname=NodeOffline,host_ip=192.168.1.100,hostname=node-001",
  "status": "firing",
  "labels": {
    "alert_name": "NodeOffline",
    "host_ip": "192.168.1.100",
    "hostname": "node-001",
    "severity": "critical"
  },
  "annotations": {
    "summary": "Node is offline",
    "description": "Node 192.168.1.100 has not sent a heartbeat for more than 5 seconds."
  }
}
```

#### 恢复告警
```json
{
  "fingerprint": "alertname=NodeOffline,host_ip=192.168.1.100,hostname=node-001",
  "status": "resolved",
  "labels": {
    "alert_name": "NodeOffline",
    "host_ip": "192.168.1.100",
    "hostname": "node-001",
    "severity": "critical"
  },
  "annotations": {
    "summary": "Node is offline",
    "description": "Node 192.168.1.100 has not sent a heartbeat for more than 5 seconds."
  }
}
```

## 使用方法

### 1. 自动集成
节点状态监控器已集成到 `AlarmSystem` 中，系统启动时会自动启动监控：

```cpp
AlarmSystem system(config);
system.initialize();  // 自动启动节点状态监控
```

### 2. 手动使用
```cpp
auto node_storage = std::make_shared<NodeStorage>();
auto alarm_manager = std::make_shared<AlarmManager>(...);

auto monitor = std::make_shared<NodeStatusMonitor>(node_storage, alarm_manager);
monitor->start();

// ... 系统运行 ...

monitor->stop();
```

## API 接口

### 获取节点列表
```bash
GET /api/v1/nodes/list
```

响应示例：
```json
{
  "status": "success",
  "data": [
    {
      "api_version": "v1",
      "data": {
        "box_id": "node-001",
        "host_ip": "192.168.1.100",
        "hostname": "node-001",
        "status": "online"  // 动态计算的状态
      }
    }
  ]
}
```

### 获取告警事件
```bash
GET /api/v1/alarm/events
```

响应示例：
```json
{
  "status": "success",
  "data": {
    "events": [
      {
        "id": "uuid-123",
        "fingerprint": "alertname=NodeOffline,host_ip=192.168.1.100",
        "status": "firing",
        "labels": {
          "alert_name": "NodeOffline",
          "host_ip": "192.168.1.100",
          "severity": "critical"
        },
        "annotations": {
          "summary": "Node is offline",
          "description": "Node 192.168.1.100 has not sent a heartbeat for more than 5 seconds."
        },
        "starts_at": "2024-01-15 10:30:00",
        "ends_at": null
      }
    ]
  }
}
```

## 测试

### 运行测试脚本
```bash
./scripts/test_node_monitor.sh
```

测试流程：
1. 启动告警系统
2. 发送节点心跳
3. 检查节点在线状态
4. 等待节点离线检测
5. 检查离线告警
6. 重新发送心跳
7. 检查告警解决

### 手动测试
```bash
# 1. 启动系统
./MyProject &

# 2. 发送心跳
curl -X POST http://localhost:8080/heartbeat \
  -H "Content-Type: application/json" \
  -d '{"api_version": "v1", "data": {...}}'

# 3. 检查节点状态
curl http://localhost:8080/api/v1/nodes/list

# 4. 等待5秒后检查告警
curl http://localhost:8080/api/v1/alarm/events
```

## 配置选项

### 修改检查间隔
```cpp
// 在 NodeStatusMonitor 构造函数中修改
const std::chrono::seconds m_check_interval{1}; // 每秒检查
```

### 修改离线阈值
```cpp
// 在 NodeStatusMonitor 构造函数中修改
const std::chrono::seconds m_offline_threshold{5}; // 5秒无心跳认为离线
```

## 日志输出

### 启动日志
```
👁️ 初始化节点状态监控器...
✅ 节点状态监控器启动成功
```

### 监控日志
```
Node '192.168.1.100' is offline. Last heartbeat 6 seconds ago.
```

### 告警日志
```
AlarmManager: Processing alarm event: alertname=NodeOffline,host_ip=192.168.1.100 - firing
AlarmManager: Alarm event inserted successfully: alertname=NodeOffline,host_ip=192.168.1.100
```

## 故障排除

### 常见问题

1. **节点状态不准确**
   - 检查心跳时间戳是否正确
   - 确认系统时间同步

2. **告警不触发**
   - 检查 AlarmManager 数据库连接
   - 查看日志中的错误信息

3. **监控器不启动**
   - 确认 NodeStorage 和 AlarmManager 已正确初始化
   - 检查线程创建是否成功

### 调试模式
启用详细日志：
```cpp
LogManager::getLogger()->set_level(spdlog::level::debug);
```

## 性能考虑

- **检查频率**: 每秒检查一次，可根据需要调整
- **内存使用**: 监控器只存储节点引用，不复制数据
- **CPU 使用**: 轻量级检查，对系统性能影响很小
- **数据库负载**: 只在状态变化时写入数据库

## 扩展功能

### 未来计划
1. **可配置阈值**: 支持不同节点类型的不同离线阈值
2. **告警抑制**: 支持告警抑制和静默功能
3. **通知集成**: 集成邮件、短信等通知方式
4. **历史记录**: 记录节点状态变化历史
5. **健康检查**: 支持自定义健康检查规则 