# 告警系统架构简化

## 概述

通过重构 `AlarmRuleEngine` 和 `AlarmSystem` 的交互方式，我们成功移除了 `AlarmRuleEngine` 对 `AlarmManager` 的直接依赖，实现了更清晰的架构分层。

## 架构变更

### 变更前

```cpp
// AlarmRuleEngine 直接依赖 AlarmManager
class AlarmRuleEngine {
private:
    std::shared_ptr<AlarmManager> m_alarm_manager;  // 直接依赖
    
    void generateAlarmEvent(const AlarmInstance& instance, 
                          const AlarmRule& rule, 
                          const std::string& status) {
        // ... 创建事件 ...
        
        // 直接调用 AlarmManager
        if (m_alarm_manager) {
            m_alarm_manager->processAlarmEvent(event);
        }
        
        // 然后调用回调
        if (m_alarm_event_callback) {
            m_alarm_event_callback(event);
        }
    }
};

// 在 AlarmSystem 中需要传入 AlarmManager
alarm_rule_engine_ = std::make_shared<AlarmRuleEngine>(
    alarm_rule_storage_, resource_storage_, alarm_manager_);
```

### 变更后

```cpp
// AlarmRuleEngine 不再直接依赖 AlarmManager
class AlarmRuleEngine {
private:
    // 移除了 m_alarm_manager 成员变量
    
    void generateAlarmEvent(const AlarmInstance& instance, 
                          const AlarmRule& rule, 
                          const std::string& status) {
        // ... 创建事件 ...
        
        // 只调用回调，让调用方处理
        if (m_alarm_event_callback) {
            m_alarm_event_callback(event);
        }
    }
};

// 在 AlarmSystem 中通过回调处理 AlarmManager
alarm_rule_engine_->setAlarmEventCallback([this](const AlarmEvent& event) {
    // AlarmSystem 负责调用 AlarmManager
    if (alarm_manager_) {
        alarm_manager_->processAlarmEvent(event);
    }
    
    // 其他处理逻辑...
});
```

## 架构优势

### 1. 降低耦合度
- `AlarmRuleEngine` 不再直接依赖 `AlarmManager`
- 职责更加清晰：`AlarmRuleEngine` 专注于规则评估和事件生成
- `AlarmSystem` 作为协调器负责组件间的交互

### 2. 简化依赖注入
- `AlarmRuleEngine` 构造函数参数减少
- 不需要在多个地方传递 `AlarmManager` 实例
- 依赖关系更加线性和清晰

### 3. 提高灵活性
- 可以在 `AlarmSystem` 层面灵活控制事件处理流程
- 便于添加事件处理中间件（日志、过滤、转换等）
- 更容易进行单元测试

### 4. 更好的架构分层
```
┌─────────────────┐
│   AlarmSystem   │  ← 协调器层：管理组件交互
├─────────────────┤
│ AlarmRuleEngine │  ← 业务逻辑层：规则评估
│ AlarmManager    │  ← 数据持久化层：事件存储
│ ResourceStorage │  ← 数据访问层：资源查询
└─────────────────┘
```

## 实现细节

### AlarmRuleEngine 简化

**构造函数:**
```cpp
// 移除了 AlarmManager 参数
AlarmRuleEngine(std::shared_ptr<AlarmRuleStorage> rule_storage,
               std::shared_ptr<ResourceStorage> resource_storage);
```

**事件生成:**
```cpp
void AlarmRuleEngine::generateAlarmEvent(const AlarmInstance& instance, 
                                       const AlarmRule& rule, 
                                       const std::string& status) {
    AlarmEvent event;
    // ... 构建事件 ...
    
    // 只通过回调传递事件，不直接调用 AlarmManager
    if (m_alarm_event_callback) {
        m_alarm_event_callback(event);
    }
}
```

### AlarmSystem 统一处理

**在 initializeServices() 中:**
```cpp
// 创建不依赖 AlarmManager 的 AlarmRuleEngine
alarm_rule_engine_ = std::make_shared<AlarmRuleEngine>(
    alarm_rule_storage_, resource_storage_);

// 设置统一的事件处理回调
alarm_rule_engine_->setAlarmEventCallback([this](const AlarmEvent& event) {
    // 1. 处理告警事件到 AlarmManager
    if (alarm_manager_) {
        alarm_manager_->processAlarmEvent(event);
    }
    
    // 2. 处理告警事件监控
    if (alarm_monitor_) {
        alarm_monitor_->onAlarmEvent(event);
    }
    
    // 3. 调用用户设置的回调函数
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (alarm_event_callback_) {
        alarm_event_callback_(event);
    }
});
```

## 事件处理流程

### 新的事件流
```
1. AlarmRuleEngine 检测到告警条件
   ↓
2. AlarmRuleEngine 生成 AlarmEvent
   ↓
3. AlarmRuleEngine 调用 m_alarm_event_callback
   ↓
4. AlarmSystem 接收回调，依次调用：
   - alarm_manager_->processAlarmEvent()  ← 数据持久化
   - alarm_monitor_->onAlarmEvent()       ← 监控统计
   - user_callback()                      ← 用户自定义处理
```

### 优势对比
- **变更前**: AlarmRuleEngine → AlarmManager + Callback
- **变更后**: AlarmRuleEngine → Callback → AlarmSystem → AlarmManager

新流程让 AlarmSystem 完全控制事件处理顺序和逻辑。

## 测试友好性

### 单元测试 AlarmRuleEngine
```cpp
// 现在可以更容易地测试 AlarmRuleEngine
auto rule_engine = std::make_shared<AlarmRuleEngine>(
    mock_rule_storage, mock_resource_storage);

// 设置测试回调
std::vector<AlarmEvent> captured_events;
rule_engine->setAlarmEventCallback([&](const AlarmEvent& event) {
    captured_events.push_back(event);
});

// 运行测试...
// 验证 captured_events 的内容
```

### 集成测试
```cpp
// AlarmSystem 的集成测试更加直观
AlarmSystem system(config);
system.initialize();

// 可以通过 AlarmSystem 的回调验证完整流程
system.setAlarmEventCallback([&](const AlarmEvent& event) {
    // 验证事件已经被正确处理
});
```

## 迁移影响

### 向后兼容性
- ✅ AlarmSystem 的公共 API 没有变化
- ✅ 所有现有功能继续正常工作
- ✅ 用户代码无需修改

### 内部变化
- ✅ AlarmRuleEngine 构造函数参数减少
- ✅ 依赖关系更加清晰
- ✅ 代码维护性提高

## 总结

这次重构实现了：

1. **🎯 职责分离**: AlarmRuleEngine 专注规则评估，AlarmSystem 负责组件协调
2. **📉 降低耦合**: 移除了 AlarmRuleEngine 对 AlarmManager 的直接依赖
3. **🔧 简化构造**: 构造函数参数更少，依赖注入更简单
4. **🧪 测试友好**: 更容易进行单元测试和集成测试
5. **🏗️ 架构清晰**: 分层更加合理，职责边界更加清楚

这种设计模式符合依赖倒置原则，让高层模块（AlarmSystem）控制低层模块（AlarmRuleEngine）的行为，而不是让低层模块直接依赖其他低层模块。