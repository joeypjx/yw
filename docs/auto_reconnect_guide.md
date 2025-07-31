# MySQL自动重连机制使用指南

## 概述

`AlarmRuleStorage` 类现在支持自动重连机制，当与MySQL数据库的连接断开时，系统会自动尝试重新连接，确保程序的稳定性和可靠性。

## 主要特性

### 1. 自动连接检测
- 使用 `mysql_ping()` 定期检测连接状态
- 在每次数据库操作前检查连接是否有效
- 自动标记连接状态

### 2. 后台重连线程
- 独立的线程处理重连逻辑
- 不阻塞主业务线程
- 支持可配置的重连间隔

### 3. 智能重连策略
- 可配置最大重连尝试次数
- 指数退避重连间隔
- 重连失败后自动停止

### 4. 线程安全
- 使用互斥锁保护重连过程
- 原子操作管理状态变量
- 避免并发重连冲突

## 配置参数

### 启用/禁用自动重连
```cpp
storage.enableAutoReconnect(true);  // 启用自动重连
storage.enableAutoReconnect(false); // 禁用自动重连
```

### 设置重连间隔
```cpp
storage.setReconnectInterval(5);  // 设置重连间隔为5秒
```

### 设置最大重连尝试次数
```cpp
storage.setMaxReconnectAttempts(10);  // 最多尝试10次重连
```

### 性能优化配置
```cpp
// 设置连接检查间隔（毫秒）
storage.setConnectionCheckInterval(10000);  // 10秒检查一次

// 启用指数退避
storage.enableExponentialBackoff(true);

// 设置最大退避时间（秒）
storage.setMaxBackoffSeconds(30);
```

### 查询重连状态
```cpp
bool enabled = storage.isAutoReconnectEnabled();
int attempts = storage.getReconnectAttempts();
int check_interval = storage.getConnectionCheckInterval();
bool backoff_enabled = storage.isExponentialBackoffEnabled();
```

## 使用示例

### 基本使用
```cpp
#include "resource/alarm_rule_storage.h"

int main() {
    // 创建存储对象
    AlarmRuleStorage storage("localhost", 3306, "user", "password", "database");
    
    // 配置自动重连
    storage.enableAutoReconnect(true);
    storage.setReconnectInterval(3);
    storage.setMaxReconnectAttempts(5);
    
    // 连接数据库
    if (storage.connect()) {
        // 正常使用数据库操作
        auto rules = storage.getAllAlarmRules();
        // ...
    }
    
    return 0;
}
```

### 高级配置
```cpp
// 创建存储对象
AlarmRuleStorage storage("localhost", 3306, "user", "password", "database");

// 配置自动重连参数
storage.enableAutoReconnect(true);
storage.setReconnectInterval(2);      // 2秒重连间隔
storage.setMaxReconnectAttempts(15);  // 最多15次重连尝试

// 配置性能优化参数
storage.setConnectionCheckInterval(10000);  // 10秒检查一次连接
storage.enableExponentialBackoff(true);    // 启用指数退避
storage.setMaxBackoffSeconds(30);          // 最大退避30秒

// 连接数据库
if (storage.connect()) {
    // 数据库操作会自动处理连接断开的情况
    std::string id = storage.insertAlarmRule(...);
    auto rules = storage.getAllAlarmRules();
}
```

## 工作原理

### 1. 连接检测
每次数据库操作前，系统会调用 `checkConnection()` 方法：
- 检查 `m_connected` 状态
- 使用 `mysql_ping()` 验证连接有效性
- 如果连接断开，设置 `m_connected = false`

### 2. 重连触发
当检测到连接断开时：
- 后台重连线程会定期检查连接状态
- 如果连接断开且满足重连条件，开始重连过程
- 重连过程使用互斥锁防止并发重连

### 3. 重连过程
```cpp
bool tryReconnect() {
    // 1. 断开现有连接
    // 2. 重新初始化MySQL连接
    // 3. 重新连接到数据库
    // 4. 重新选择数据库
    // 5. 更新连接状态
    // 6. 重置重连计数器
}
```

### 4. 重连循环
```cpp
void reconnectLoop() {
    while (!m_stop_reconnect_thread) {
        if (需要重连) {
            if (重连成功) {
                等待间隔时间;
            } else {
                增加尝试次数;
                if (达到最大尝试次数) {
                    停止自动重连;
                }
                等待间隔时间;
            }
        } else {
            短暂休眠;
        }
    }
}
```

## 错误处理

### 1. 连接失败
- 记录详细的错误日志
- 增加重连尝试次数
- 如果达到最大尝试次数，停止自动重连

### 2. 数据库操作失败
- 返回空结果或默认值
- 记录操作失败日志
- 不抛出异常，保证程序稳定性

### 3. 线程异常
- 使用RAII管理线程资源
- 析构函数中正确停止重连线程
- 避免线程泄漏

## 性能考虑

### 1. 内存使用
- 使用智能指针管理资源
- 及时释放MySQL结果集
- 避免内存泄漏

### 2. CPU使用
- 重连线程使用适当的休眠间隔（1秒）
- 避免频繁的连接检测（可配置检查间隔）
- 使用指数退避减少重连频率

### 3. 网络开销
- `mysql_ping()` 是轻量级操作，但可配置检查频率
- 重连间隔可配置，避免过于频繁
- 支持连接池扩展

### 4. 性能优化策略

#### 连接检查优化
```cpp
// 默认每5秒检查一次连接状态
storage.setConnectionCheckInterval(5000);

// 在稳定网络环境下可以增加到30秒
storage.setConnectionCheckInterval(30000);
```

#### 指数退避策略
```cpp
// 启用指数退避，减少重连频率
storage.enableExponentialBackoff(true);
storage.setMaxBackoffSeconds(60);  // 最大退避60秒

// 重连间隔计算：基础间隔 * 2^(尝试次数-1)
// 例如：5秒 -> 10秒 -> 20秒 -> 40秒 -> 60秒（最大）
```

#### 线程休眠优化
- 连接正常时：1秒休眠间隔
- 重连失败时：使用指数退避计算休眠时间
- 避免频繁的线程唤醒

## 最佳实践

### 1. 配置建议
```cpp
// 生产环境推荐配置
storage.setReconnectInterval(5);      // 5秒重连间隔
storage.setMaxReconnectAttempts(10);  // 最多10次重连
storage.enableAutoReconnect(true);    // 启用自动重连

// 性能优化配置
storage.setConnectionCheckInterval(10000);  // 10秒检查一次连接
storage.enableExponentialBackoff(true);    // 启用指数退避
storage.setMaxBackoffSeconds(60);          // 最大退避60秒
```

### 2. 不同环境配置建议

#### 稳定网络环境
```cpp
storage.setConnectionCheckInterval(30000);  // 30秒检查一次
storage.setReconnectInterval(10);           // 10秒重连间隔
```

#### 不稳定网络环境
```cpp
storage.setConnectionCheckInterval(5000);   // 5秒检查一次
storage.setReconnectInterval(3);            // 3秒重连间隔
```

#### 高负载环境
```cpp
storage.setConnectionCheckInterval(60000);  // 1分钟检查一次
storage.setReconnectInterval(15);           // 15秒重连间隔
```

### 2. 监控建议
- 定期检查重连尝试次数
- 监控连接状态变化
- 记录重连成功/失败统计

### 3. 故障排查
- 检查MySQL服务器状态
- 验证网络连接
- 查看详细错误日志

## 注意事项

1. **线程安全**：所有数据库操作都是线程安全的
2. **资源管理**：析构函数会自动清理所有资源
3. **错误恢复**：连接断开不会导致程序崩溃
4. **性能影响**：重连机制对性能影响很小
5. **配置灵活**：可以根据需要调整重连参数

## 故障排除

### 常见问题

1. **重连失败**
   - 检查MySQL服务器是否运行
   - 验证连接参数是否正确
   - 查看网络连接状态

2. **频繁重连**
   - 检查网络稳定性
   - 调整重连间隔参数
   - 监控MySQL服务器负载

3. **内存泄漏**
   - 确保正确调用析构函数
   - 检查线程是否正确停止
   - 验证资源释放逻辑

### 调试技巧

1. **启用详细日志**
   ```cpp
   LogManager::init("logs/debug.log", "debug");
   ```

2. **监控重连状态**
   ```cpp
   std::cout << "重连尝试次数: " << storage.getReconnectAttempts() << std::endl;
   ```

3. **手动触发重连**
   ```cpp
   // 可以通过断开网络连接来测试重连机制
   ```

## 总结

自动重连机制大大提高了程序的稳定性和可靠性，特别是在网络不稳定的环境中。通过合理的配置和监控，可以确保数据库操作的连续性和数据的完整性。 