# 节点状态监控功能实现总结

## 🎯 实现目标

根据用户需求，我们实现了一个完整的节点状态监控机制，能够在5秒内检测到节点离线并触发相应措施。

## 🏗️ 架构设计

### 核心组件

1. **NodeStatusMonitor** - 节点状态监控器
   - 独立的后台线程，每秒检查一次所有节点
   - 计算节点最后心跳时间与当前时间的差距
   - 根据5秒阈值判断节点在线/离线状态

2. **AlarmManager** - 告警管理器增强
   - 新增 `calculateFingerprint()` 方法
   - 新增 `createOrUpdateAlarm()` 方法
   - 新增 `resolveAlarm()` 方法

3. **AlarmSystem** - 系统集成
   - 自动初始化和启动 NodeStatusMonitor
   - 优雅关闭时停止监控器

## 🔧 实现细节

### 1. NodeStatusMonitor 类

**头文件**: `include/resource/node_status_monitor.h`
**实现文件**: `src/resource/node_status_monitor.cpp`

```cpp
class NodeStatusMonitor {
public:
    NodeStatusMonitor(std::shared_ptr<NodeStorage> node_storage, 
                     std::shared_ptr<AlarmManager> alarm_manager);
    void start();
    void stop();
    
private:
    void run();                    // 后台线程主循环
    void checkNodeStatus();        // 检查节点状态
    
    std::shared_ptr<NodeStorage> m_node_storage;
    std::shared_ptr<AlarmManager> m_alarm_manager;
    std::atomic<bool> m_running;
    std::thread m_monitor_thread;
    
    const std::chrono::seconds m_check_interval{1};      // 检查间隔
    const std::chrono::seconds m_offline_threshold{5};   // 离线阈值
};
```

### 2. 工作流程

```
1. 获取所有节点列表 (NodeStorage::getAllNodes())
   ↓
2. 遍历每个节点
   ↓
3. 计算时间差: now - node->last_heartbeat
   ↓
4. 判断状态:
   ├─ 时间差 ≤ 5秒: 节点在线 → 解决相关告警
   └─ 时间差 > 5秒: 节点离线 → 创建离线告警
   ↓
5. 等待1秒后重复检查
```

### 3. 告警机制

#### 离线告警
- **告警名称**: `NodeOffline`
- **告警级别**: `critical`
- **触发条件**: 节点超过5秒无心跳
- **告警内容**: 包含节点IP、主机名、离线时长等信息

#### 恢复告警
- **状态**: `resolved`
- **触发条件**: 节点重新发送心跳
- **自动解决**: 系统自动解决相关告警

### 4. 告警指纹

每个节点的告警都有唯一的指纹标识：
```
alertname=NodeOffline,host_ip=192.168.1.100,hostname=node-001
```

确保：
- 同一节点的多个告警不会重复
- 不同节点的告警相互独立
- 告警解决时能准确找到对应告警

## 🚀 使用方法

### 1. 自动使用（推荐）

节点状态监控已集成到 `AlarmSystem` 中，系统启动时自动启动：

```cpp
AlarmSystemConfig config;
// ... 配置数据库等参数

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

## 📊 API 接口

### 获取节点列表
```bash
GET /api/v1/nodes/list
```

响应中的节点状态是动态计算的：
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

可以看到节点离线告警：
```json
{
  "status": "success",
  "data": {
    "events": [
      {
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
        }
      }
    ]
  }
}
```

## 🧪 测试验证

### 1. 测试脚本
```bash
./scripts/test_node_monitor.sh
```

### 2. 手动测试
```bash
# 启动系统
./MyProject &

# 发送心跳
curl -X POST http://localhost:8080/heartbeat \
  -H "Content-Type: application/json" \
  -d '{"api_version": "v1", "data": {...}}'

# 检查节点状态
curl http://localhost:8080/api/v1/nodes/list

# 等待5秒后检查告警
curl http://localhost:8080/api/v1/alarm/events
```

### 3. 演示程序
```bash
# 编译演示程序
g++ -std=c++14 -Iinclude examples/node_monitor_demo.cpp -o node_monitor_demo

# 运行演示
./node_monitor_demo
```

## 📈 性能特点

### 1. 轻量级设计
- **检查频率**: 每秒一次，可根据需要调整
- **内存使用**: 只存储节点引用，不复制数据
- **CPU 使用**: 轻量级检查，对系统性能影响很小

### 2. 高效告警
- **数据库负载**: 只在状态变化时写入数据库
- **告警去重**: 使用指纹机制避免重复告警
- **自动恢复**: 节点恢复时自动解决告警

### 3. 可扩展性
- **配置灵活**: 检查间隔和离线阈值可配置
- **模块化设计**: 可独立使用或集成到其他系统
- **接口标准化**: 使用标准告警事件格式

## 🔧 配置选项

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

## 📝 日志输出

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

## 🎯 功能验证

### ✅ 已实现功能

1. **主动监控**: ✅ 每秒检查节点状态
2. **离线检测**: ✅ 5秒无心跳自动检测离线
3. **告警触发**: ✅ 节点离线时自动创建告警
4. **自动恢复**: ✅ 节点恢复时自动解决告警
5. **唯一标识**: ✅ 每个节点告警有唯一指纹
6. **系统集成**: ✅ 已集成到 AlarmSystem 中
7. **优雅关闭**: ✅ 系统关闭时停止监控器

### 🔮 扩展功能（未来计划）

1. **可配置阈值**: 支持不同节点类型的不同离线阈值
2. **告警抑制**: 支持告警抑制和静默功能
3. **通知集成**: 集成邮件、短信等通知方式
4. **历史记录**: 记录节点状态变化历史
5. **健康检查**: 支持自定义健康检查规则

## 📚 相关文档

- [节点状态监控详细文档](docs/node_status_monitor.md)
- [告警系统架构文档](docs/alarm.md)
- [API 接口文档](docs/apiv1.md)

## 🎉 总结

我们成功实现了一个完整的节点状态监控系统，具备以下特点：

1. **实时性**: 每秒检查一次，5秒内检测到离线
2. **可靠性**: 使用指纹机制确保告警准确性
3. **自动化**: 无需人工干预，自动检测和恢复
4. **集成性**: 完美集成到现有告警系统中
5. **可扩展性**: 支持未来功能扩展

该系统能够有效监控集群中节点的健康状态，及时发现和处理节点离线问题，为系统运维提供了强有力的支持。 