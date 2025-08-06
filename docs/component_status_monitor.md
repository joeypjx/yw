# ComponentStatusMonitor 组件状态监控器

## 概述

`ComponentStatusMonitor` 是一个专门用于监控 `NodeData` 中 `ComponentInfo` 的 `state` 字段的监控器。它能够检测组件状态的变化，特别是监控 `FAILED` 状态并触发相应的告警。

## 功能特性

- **状态变化检测**: 监控组件状态从一种状态变为另一种状态
- **FAILED状态告警**: 当组件状态为 `FAILED` 且持续一定时间后触发告警
- **状态恢复处理**: 当组件从 `FAILED` 状态恢复时自动解决告警
- **回调通知**: 支持自定义回调函数来处理状态变化事件
- **历史记录管理**: 维护组件状态历史，避免重复告警
- **自动清理**: 定期清理过期的历史记录

## 支持的状态

根据 `ComponentInfo` 的定义，支持以下状态：
- `PENDING` - 等待中
- `RUNNING` - 运行中
- `FAILED` - 失败
- `STOPPED` - 已停止
- `SLEEPING` - 休眠中

## 使用方法

### 1. 基本使用

```cpp
#include "component_status_monitor.h"
#include "node_storage.h"
#include "alarm_manager.h"

// 创建依赖组件
auto node_storage = std::make_shared<NodeStorage>();
auto alarm_manager = std::make_shared<AlarmManager>();

// 创建监控器
auto component_monitor = std::make_shared<ComponentStatusMonitor>(node_storage, alarm_manager);

// 启动监控
component_monitor->start();
```

### 2. 设置回调函数

```cpp
// 定义回调函数
void onComponentStatusChange(const std::string& host_ip, 
                           const std::string& instance_id, 
                           const std::string& uuid,
                           int index,
                           const std::string& old_state, 
                           const std::string& new_state) {
    std::cout << "Component " << instance_id << " on " << host_ip 
              << " changed from " << old_state << " to " << new_state << std::endl;
}

// 设置回调
component_monitor->setComponentStatusChangeCallback(onComponentStatusChange);
```

### 3. 配置监控参数

```cpp
// 设置检查间隔（默认30秒）
component_monitor->setCheckInterval(std::chrono::seconds(10));

// 设置FAILED状态阈值（默认60秒）
component_monitor->setFailedThreshold(std::chrono::seconds(30));
```

### 4. 完整示例

```cpp
#include "component_status_monitor.h"
#include "node_storage.h"
#include "alarm_manager.h"
#include "log_manager.h"

int main() {
    // 初始化日志
    LogManager::init("component_monitor.log", "info");
    
    // 创建组件
    auto node_storage = std::make_shared<NodeStorage>();
    auto alarm_manager = std::make_shared<AlarmManager>();
    auto component_monitor = std::make_shared<ComponentStatusMonitor>(node_storage, alarm_manager);
    
    // 设置回调
    component_monitor->setComponentStatusChangeCallback([](const std::string& host_ip, 
                                                          const std::string& instance_id, 
                                                          const std::string& uuid,
                                                          int index,
                                                          const std::string& old_state, 
                                                          const std::string& new_state) {
        LogManager::getLogger()->info("Component status change: {}:{}:{}:{} ({} -> {})", 
                                     host_ip, instance_id, uuid, index, old_state, new_state);
    });
    
    // 配置参数
    component_monitor->setCheckInterval(std::chrono::seconds(30));
    component_monitor->setFailedThreshold(std::chrono::seconds(60));
    
    // 启动监控
    component_monitor->start();
    
    // 主循环
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
```

## 告警机制

### FAILED状态告警

当组件状态为 `FAILED` 且持续超过设定的阈值时间时，监控器会：

1. 创建告警事件记录
2. 设置告警标签（host_ip, instance_id, uuid, index, component_name, component_id）
3. 设置告警注释（摘要、描述、严重程度）
4. 将告警添加到告警管理器
5. 标记该组件已触发告警

### 告警解决

当组件状态从 `FAILED` 恢复为其他状态时，监控器会：

1. 计算告警指纹
2. 调用告警管理器的解决告警方法
3. 重置组件的告警状态

## 配置参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| 检查间隔 | 30秒 | 监控器检查组件状态的频率 |
| FAILED阈值 | 60秒 | FAILED状态持续多久后触发告警 |

## 组件标识

每个组件通过以下字段唯一标识：
- `host_ip`: 主机IP地址
- `instance_id`: 实例ID
- `uuid`: 组件UUID
- `index`: 组件索引

组件键格式：`host_ip:instance_id:uuid:index`

## 历史记录管理

监控器维护每个组件的状态历史记录，包括：
- 当前状态
- 最后更新时间
- 是否已触发告警

历史记录会在1小时后自动清理，避免内存泄漏。

## 线程安全

监控器使用多线程设计：
- 主监控线程：定期检查组件状态
- 回调线程：异步执行状态变化回调
- 历史记录线程安全：使用互斥锁保护历史记录

## 错误处理

- 回调函数异常不会影响监控线程
- 数据库连接失败会记录错误日志
- 组件数据格式错误会被忽略并记录警告

## 性能考虑

- 使用智能指针管理内存
- 异步回调避免阻塞监控线程
- 定期清理过期历史记录
- 使用哈希表快速查找组件历史

## 测试

运行测试程序：

```bash
cd build
make component_status_monitor_test
./component_status_monitor_test
```

测试程序会模拟组件状态变化，演示监控器的功能。 