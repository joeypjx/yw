# TDengine连接池使用指南

## 概述

TDengine连接池是一个高性能、线程安全的数据库连接池实现，专门为TDengine时序数据库设计。它提供了连接复用、自动重连、健康检查等功能，可以显著提高应用程序的数据库访问性能。

## 主要特性

### 🚀 核心功能
- **连接池管理**: 自动管理最小/最大连接数
- **RAII资源管理**: 自动获取和释放连接
- **线程安全**: 支持多线程并发访问
- **连接复用**: 减少连接创建/销毁开销
- **健康检查**: 自动检测和清理无效连接
- **统计监控**: 详细的连接池使用统计

### ⚙️ 高级特性
- **连接生命周期管理**: 支持连接超时和最大生存时间
- **动态配置更新**: 运行时更新连接池配置
- **连接池管理器**: 支持多个命名连接池
- **自定义日志回调**: 集成自定义日志系统
- **优雅关闭**: 等待活跃连接完成后再关闭

## 快速开始

### 1. 基本使用

```cpp
#include "tdengine_connection_pool.h"

// 创建连接池配置
TDenginePoolConfig config;
config.host = "localhost";
config.port = 6030;
config.user = "root";
config.password = "taosdata";
config.database = "mydb";
config.min_connections = 2;
config.max_connections = 10;
config.initial_connections = 5;

// 创建并初始化连接池
TDengineConnectionPool pool(config);
if (!pool.initialize()) {
    std::cerr << "连接池初始化失败" << std::endl;
    return -1;
}

// 使用RAII守卫获取连接
TDengineConnectionGuard guard(std::make_shared<TDengineConnectionPool>(std::move(pool)));
if (guard.isValid()) {
    TAOS* taos = guard->get();
    // 使用连接执行查询
    TAOS_RES* result = taos_query(taos, "SELECT * FROM mytable");
    // ... 处理结果
    taos_free_result(result);
} // 连接自动释放
```

### 2. 与ResourceStorage集成使用

```cpp
#include "resource_storage.h"

// 使用连接池配置构造函数
TDenginePoolConfig config;
config.host = "localhost";
config.user = "root";
config.password = "taosdata";
config.min_connections = 3;
config.max_connections = 15;

ResourceStorage storage(config);

// 初始化
if (!storage.initialize()) {
    std::cerr << "ResourceStorage初始化失败" << std::endl;
    return -1;
}

// 创建数据库和表
storage.createDatabase("test_db");
storage.createResourceTable();

// 执行查询
auto results = storage.executeQuerySQL("SELECT NOW()");

// 获取连接池统计
auto stats = storage.getConnectionPoolStats();
std::cout << "总连接数: " << stats.total_connections << std::endl;
std::cout << "活跃连接数: " << stats.active_connections << std::endl;
```

## 配置说明

### TDenginePoolConfig参数详解

```cpp
struct TDenginePoolConfig {
    // 连接参数
    std::string host = "localhost";         // TDengine服务器地址
    int port = 6030;                       // TDengine服务器端口
    std::string user = "root";             // 用户名
    std::string password = "taosdata";     // 密码
    std::string database = "";             // 默认数据库
    
    // 连接池大小配置
    int min_connections = 3;               // 最小连接数
    int max_connections = 10;              // 最大连接数
    int initial_connections = 5;           // 初始连接数
    
    // 超时配置（秒）
    int connection_timeout = 30;           // 连接超时
    int idle_timeout = 600;                // 空闲超时（10分钟）
    int max_lifetime = 3600;               // 连接最大生存时间（1小时）
    int acquire_timeout = 10;              // 获取连接超时
    
    // 健康检查配置
    int health_check_interval = 60;        // 健康检查间隔（秒）
    std::string health_check_query = "SELECT SERVER_VERSION()"; // 健康检查SQL
    
    // TDengine特定配置
    std::string locale = "C";              // 本地化设置
    std::string charset = "UTF-8";         // 字符集
    std::string timezone = "";             // 时区
    
    // 其他配置
    bool auto_reconnect = true;            // 自动重连
    int max_sql_length = 1048576;          // 最大SQL长度（1MB）
};
```

### 推荐配置

#### 小型应用
```cpp
config.min_connections = 2;
config.max_connections = 5;
config.initial_connections = 3;
config.acquire_timeout = 5;
```

#### 中型应用
```cpp
config.min_connections = 5;
config.max_connections = 15;
config.initial_connections = 8;
config.acquire_timeout = 10;
```

#### 大型应用
```cpp
config.min_connections = 10;
config.max_connections = 50;
config.initial_connections = 20;
config.acquire_timeout = 15;
```

## 高级用法

### 1. 连接池管理器

```cpp
// 获取管理器单例
auto& manager = TDengineConnectionPoolManager::getInstance();

// 创建多个命名连接池
TDenginePoolConfig config1, config2;
config1.database = "db1";
config2.database = "db2";

manager.createPool("pool1", config1);
manager.createPool("pool2", config2);

// 使用不同的连接池
auto pool1 = manager.getPool("pool1");
auto pool2 = manager.getPool("pool2");

if (pool1) {
    TDengineConnectionGuard guard1(pool1);
    // 使用pool1的连接
}

if (pool2) {
    TDengineConnectionGuard guard2(pool2);
    // 使用pool2的连接
}

// 清理所有连接池
manager.destroyAllPools();
```

### 2. 连接池监控

```cpp
// 获取连接池统计信息
auto stats = pool.getStats();

std::cout << "连接池监控信息:" << std::endl;
std::cout << "  总连接数: " << stats.total_connections << std::endl;
std::cout << "  活跃连接数: " << stats.active_connections << std::endl;
std::cout << "  空闲连接数: " << stats.idle_connections << std::endl;
std::cout << "  等待请求数: " << stats.pending_requests << std::endl;
std::cout << "  已创建连接数: " << stats.created_connections << std::endl;
std::cout << "  已销毁连接数: " << stats.destroyed_connections << std::endl;
std::cout << "  平均等待时间: " << stats.average_wait_time << "ms" << std::endl;

// 检查连接池健康状态
if (pool.isHealthy()) {
    std::cout << "连接池状态正常" << std::endl;
} else {
    std::cout << "连接池状态异常" << std::endl;
}
```

### 3. 自定义日志

```cpp
// 设置自定义日志回调
pool.setLogCallback([](const std::string& level, const std::string& message) {
    std::cout << "[" << level << "] " << message << std::endl;
});
```

### 4. 动态配置更新

```cpp
// 运行时更新配置
TDenginePoolConfig new_config = config;
new_config.max_connections = 20;
new_config.health_check_interval = 30;

pool.updateConfig(new_config);
```

### 5. 并发使用

```cpp
#include <thread>
#include <vector>

auto pool_shared = std::make_shared<TDengineConnectionPool>(config);
pool_shared->initialize();

std::vector<std::thread> threads;

// 启动多个线程并发使用连接池
for (int i = 0; i < 10; ++i) {
    threads.emplace_back([pool_shared, i]() {
        TDengineConnectionGuard guard(pool_shared);
        if (guard.isValid()) {
            TAOS* taos = guard->get();
            // 执行查询
            std::string sql = "SELECT * FROM table" + std::to_string(i);
            TAOS_RES* result = taos_query(taos, sql.c_str());
            // 处理结果...
            taos_free_result(result);
        }
    });
}

// 等待所有线程完成
for (auto& thread : threads) {
    thread.join();
}
```

## 错误处理

### 1. 连接获取失败

```cpp
TDengineConnectionGuard guard(pool, 5000); // 5秒超时
if (!guard.isValid()) {
    std::cerr << "无法获取数据库连接，可能原因:" << std::endl;
    std::cerr << "  - 连接池已满且达到超时时间" << std::endl;
    std::cerr << "  - 数据库服务器不可用" << std::endl;
    std::cerr << "  - 网络连接问题" << std::endl;
    return -1;
}
```

### 2. 初始化失败

```cpp
if (!pool.initialize()) {
    std::cerr << "连接池初始化失败，可能原因:" << std::endl;
    std::cerr << "  - TDengine服务器连接失败" << std::endl;
    std::cerr << "  - 认证信息错误" << std::endl;
    std::cerr << "  - 配置参数无效" << std::endl;
    return -1;
}
```

### 3. 查询错误处理

```cpp
try {
    auto results = storage.executeQuerySQL("SELECT * FROM mytable");
    // 处理结果...
} catch (const std::runtime_error& e) {
    std::cerr << "查询执行失败: " << e.what() << std::endl;
    // 错误恢复逻辑...
}
```

## 性能优化建议

### 1. 连接池大小调优
- **最小连接数**: 设置为应用的基础并发数
- **最大连接数**: 不要设置过大，避免数据库压力
- **初始连接数**: 在最小和最大之间，减少冷启动时间

### 2. 超时参数调优
- **获取连接超时**: 根据应用响应时间要求设置
- **空闲超时**: 平衡资源利用和连接开销
- **连接生存时间**: 定期更新连接，避免长时间连接问题

### 3. 健康检查优化
- **检查间隔**: 不要设置过频繁，避免额外开销
- **检查查询**: 使用轻量级查询，如 `SELECT SERVER_VERSION()`

### 4. 并发优化
- 使用RAII守卫确保连接及时释放
- 避免在事务中长时间持有连接
- 合理设计数据库操作的粒度

## 最佳实践

### 1. 资源管理
```cpp
// ✅ 正确：使用RAII守卫
{
    TDengineConnectionGuard guard(pool);
    if (guard.isValid()) {
        // 使用连接
    }
} // 连接自动释放

// ❌ 错误：手动管理连接
auto connection = pool->getConnection();
// 忘记释放连接会导致连接泄漏
```

### 2. 错误处理
```cpp
// ✅ 正确：检查连接有效性
TDengineConnectionGuard guard(pool);
if (!guard.isValid()) {
    // 处理连接获取失败
    return false;
}

// ❌ 错误：不检查连接状态
TDengineConnectionGuard guard(pool);
TAOS* taos = guard->get(); // 可能为nullptr
```

### 3. 配置管理
```cpp
// ✅ 正确：根据环境调整配置
TDenginePoolConfig config;
if (isProduction()) {
    config.max_connections = 50;
    config.health_check_interval = 60;
} else {
    config.max_connections = 10;
    config.health_check_interval = 30;
}
```

### 4. 监控和诊断
```cpp
// 定期检查连接池状态
void monitorConnectionPool(const TDengineConnectionPool& pool) {
    auto stats = pool.getStats();
    
    // 检查连接池使用率
    double usage_rate = static_cast<double>(stats.active_connections) / stats.total_connections;
    if (usage_rate > 0.8) {
        // 连接池使用率过高，考虑调整配置
        std::cout << "警告：连接池使用率过高 " << usage_rate * 100 << "%" << std::endl;
    }
    
    // 检查等待时间
    if (stats.average_wait_time > 100) {
        std::cout << "警告：平均等待时间过长 " << stats.average_wait_time << "ms" << std::endl;
    }
}
```

## 故障排除

### 常见问题

1. **连接池初始化失败**
   - 检查TDengine服务器是否运行
   - 验证连接参数（主机、端口、用户名、密码）
   - 检查网络连接

2. **获取连接超时**
   - 增加最大连接数
   - 检查连接释放是否及时
   - 优化数据库操作时间

3. **连接泄漏**
   - 确保使用RAII守卫
   - 检查异常处理中的资源释放
   - 监控连接池统计信息

4. **性能问题**
   - 调整连接池大小配置
   - 优化SQL查询效率
   - 检查网络延迟

### 调试技巧

1. **启用详细日志**
```cpp
pool.setLogCallback([](const std::string& level, const std::string& message) {
    std::cout << "[" << level << "] " << message << std::endl;
});
```

2. **监控连接池状态**
```cpp
void printPoolStats(const TDengineConnectionPool& pool) {
    auto stats = pool.getStats();
    std::cout << "Pool Stats: " 
              << "Total=" << stats.total_connections
              << ", Active=" << stats.active_connections
              << ", Idle=" << stats.idle_connections
              << ", Pending=" << stats.pending_requests << std::endl;
}
```

## 更新日志

### v1.0.0
- 基础连接池功能
- RAII资源管理
- 线程安全支持
- 健康检查机制
- 统计监控功能
- 连接池管理器
- ResourceStorage集成

## 许可证

本项目采用 MIT 许可证。详情请参见 LICENSE 文件。