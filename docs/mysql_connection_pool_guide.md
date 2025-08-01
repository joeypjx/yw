# MySQL连接池使用指南

## 概述

本MySQL连接池实现为您的应用程序提供了高效、线程安全的数据库连接管理功能。它支持连接复用、自动重连、健康检查、性能监控等特性。

## 核心特性

### 🚀 性能优化
- **连接复用**: 避免频繁创建/销毁连接的开销
- **连接池大小管理**: 可配置最小/最大连接数
- **智能超时管理**: 连接、空闲、生存时间等多种超时策略

### 🔒 线程安全
- **完全线程安全**: 支持多线程并发访问
- **RAII语义**: 自动管理连接的获取和释放
- **连接守护**: `MySQLConnectionGuard`提供异常安全保证

### 🛡️ 可靠性保证
- **自动健康检查**: 定期检测连接有效性
- **自动重连**: 网络中断后自动恢复
- **连接生命周期管理**: 防止连接过期和资源泄漏

### 📊 监控和调试
- **详细统计信息**: 连接数量、等待时间等指标
- **灵活日志系统**: 可自定义日志输出
- **性能指标**: 平均等待时间、连接创建/销毁计数

## 快速开始

### 1. 基本配置

```cpp
#include "resource/mysql_connection_pool.h"

// 创建连接池配置
MySQLPoolConfig config;
config.host = "localhost";
config.port = 3306;
config.user = "your_username";
config.password = "your_password";
config.database = "your_database";

// 连接池大小配置
config.min_connections = 5;
config.max_connections = 20;
config.initial_connections = 10;

// 超时配置
config.connection_timeout = 30;  // 连接超时30秒
config.idle_timeout = 600;       // 空闲10分钟后回收
config.max_lifetime = 3600;      // 连接最大生存1小时
```

### 2. 创建和初始化连接池

```cpp
// 创建连接池
MySQLConnectionPool pool(config);

// 初始化连接池
if (!pool.initialize()) {
    std::cerr << "连接池初始化失败!" << std::endl;
    return -1;
}

std::cout << "连接池初始化成功!" << std::endl;
```

### 3. 使用连接

#### 方式一：手动管理连接

```cpp
// 获取连接
auto connection = pool.getConnection();
if (!connection) {
    std::cerr << "获取连接失败!" << std::endl;
    return;
}

// 使用连接
MYSQL* mysql = connection->get();
if (mysql_query(mysql, "SELECT * FROM users LIMIT 10") == 0) {
    MYSQL_RES* result = mysql_store_result(mysql);
    if (result) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            // 处理查询结果
            std::cout << "User: " << (row[0] ? row[0] : "NULL") << std::endl;
        }
        mysql_free_result(result);
    }
} else {
    std::cerr << "查询失败: " << mysql_error(mysql) << std::endl;
}

// 连接会在connection析构时自动释放回连接池
```

#### 方式二：使用RAII连接守护（推荐）

```cpp
auto pool_ptr = std::make_shared<MySQLConnectionPool>(config);
pool_ptr->initialize();

{
    // 使用RAII连接守护，自动管理连接生命周期
    MySQLConnectionGuard guard(pool_ptr, 5000); // 5秒超时
    
    if (!guard.isValid()) {
        std::cerr << "获取连接失败!" << std::endl;
        return;
    }
    
    // 使用连接
    MYSQL* mysql = guard->get();
    // ... 执行数据库操作
    
    // guard析构时自动释放连接
}
```

### 4. 多线程使用示例

```cpp
void database_worker(std::shared_ptr<MySQLConnectionPool> pool, int worker_id) {
    for (int i = 0; i < 10; ++i) {
        MySQLConnectionGuard guard(pool);
        
        if (!guard.isValid()) {
            std::cerr << "Worker " << worker_id << " 获取连接失败!" << std::endl;
            continue;
        }
        
        // 执行数据库操作
        MYSQL* mysql = guard->get();
        std::string query = "SELECT " + std::to_string(worker_id) + 
                           " as worker_id, " + std::to_string(i) + " as iteration";
        
        if (mysql_query(mysql, query.c_str()) == 0) {
            MYSQL_RES* result = mysql_store_result(mysql);
            if (result) {
                mysql_free_result(result);
                std::cout << "Worker " << worker_id << " 完成查询 " << i << std::endl;
            }
        }
        
        // 模拟处理时间
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// 启动多个工作线程
std::vector<std::thread> workers;
for (int i = 0; i < 5; ++i) {
    workers.emplace_back(database_worker, pool_ptr, i);
}

// 等待所有线程完成
for (auto& worker : workers) {
    worker.join();
}
```

## 高级特性

### 1. 连接池管理器

对于需要连接多个数据库的应用，可以使用连接池管理器：

```cpp
// 获取管理器实例
auto& manager = MySQLConnectionPoolManager::getInstance();

// 创建多个连接池
MySQLPoolConfig config1, config2;
config1.database = "database1";
config2.database = "database2";

manager.createPool("pool1", config1);
manager.createPool("pool2", config2);

// 使用不同的连接池
auto pool1 = manager.getPool("pool1");
auto pool2 = manager.getPool("pool2");

if (pool1 && pool2) {
    MySQLConnectionGuard guard1(pool1);
    MySQLConnectionGuard guard2(pool2);
    
    // 同时操作两个数据库
    // ...
}

// 清理
manager.destroyAllPools();
```

### 2. 性能监控

```cpp
// 获取连接池统计信息
auto stats = pool.getStats();

std::cout << "连接池统计信息:" << std::endl;
std::cout << "  总连接数: " << stats.total_connections << std::endl;
std::cout << "  活跃连接数: " << stats.active_connections << std::endl;
std::cout << "  空闲连接数: " << stats.idle_connections << std::endl;
std::cout << "  等待请求数: " << stats.pending_requests << std::endl;
std::cout << "  创建连接总数: " << stats.created_connections << std::endl;
std::cout << "  销毁连接总数: " << stats.destroyed_connections << std::endl;
std::cout << "  平均等待时间: " << stats.average_wait_time << "ms" << std::endl;

// 检查连接池健康状态
if (pool.isHealthy()) {
    std::cout << "连接池状态正常" << std::endl;
} else {
    std::cout << "连接池状态异常，需要检查" << std::endl;
}
```

### 3. 自定义日志

```cpp
// 设置自定义日志回调
pool.setLogCallback([](const std::string& level, const std::string& message) {
    std::cout << "[" << level << "] " << message << std::endl;
});
```

## 配置参数详解

### 连接参数
- `host`: 数据库服务器地址
- `port`: 数据库端口（默认3306）
- `user`: 数据库用户名
- `password`: 数据库密码
- `database`: 数据库名称
- `charset`: 字符集（推荐使用utf8mb4）

### 连接池大小
- `min_connections`: 最小连接数（默认5）
- `max_connections`: 最大连接数（默认20）
- `initial_connections`: 初始连接数（默认5）

### 超时配置
- `connection_timeout`: 连接超时时间（秒，默认30）
- `idle_timeout`: 空闲超时时间（秒，默认600）
- `max_lifetime`: 连接最大生存时间（秒，默认3600）
- `acquire_timeout`: 获取连接超时时间（秒，默认10）

### 健康检查
- `health_check_interval`: 健康检查间隔（秒，默认60）
- `health_check_query`: 健康检查SQL（默认"SELECT 1"）

### 其他选项
- `auto_reconnect`: 是否启用自动重连（默认true）
- `use_ssl`: 是否使用SSL连接（默认false）
- `max_allowed_packet`: 最大数据包大小（字节，默认16MB）

## 最佳实践

### 1. 连接池大小设置
```cpp
// 根据应用负载调整连接池大小
// 经验公式：max_connections = CPU核心数 * 2 到 CPU核心数 * 4
int cpu_cores = std::thread::hardware_concurrency();
config.min_connections = cpu_cores;
config.max_connections = cpu_cores * 3;
config.initial_connections = cpu_cores * 2;
```

### 2. 错误处理
```cpp
auto connection = pool.getConnection(5000); // 5秒超时
if (!connection) {
    // 处理获取连接失败的情况
    // 可能的原因：网络问题、数据库服务器不可用、连接池满等
    logError("无法获取数据库连接，请检查网络和数据库服务器状态");
    return false;
}

// 检查连接有效性
if (!connection->isValid()) {
    logError("获取到无效的数据库连接");
    return false;
}
```

### 3. 资源管理
```cpp
// 总是使用RAII方式管理连接
{
    MySQLConnectionGuard guard(pool);
    if (guard.isValid()) {
        // 使用连接执行操作
        // 无需手动释放，guard析构时会自动处理
    }
} // 连接在此处自动释放
```

### 4. 性能优化
```cpp
// 预热连接池
pool.initialize();

// 定期监控连接池状态
std::thread monitor_thread([&pool]() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::minutes(1));
        auto stats = pool.getStats();
        if (stats.pending_requests > 10) {
            logWarn("连接池压力较大，考虑增加max_connections");
        }
    }
});
monitor_thread.detach();
```

## 故障排除

### 常见问题

1. **连接失败**
   - 检查MySQL服务器是否运行
   - 验证连接参数（主机、端口、用户名、密码）
   - 检查网络连接和防火墙设置

2. **获取连接超时**
   - 增加`max_connections`值
   - 检查应用是否正确释放连接
   - 优化数据库查询性能

3. **内存泄漏**
   - 确保使用RAII方式管理连接
   - 检查异常处理代码中的资源释放

4. **性能问题**
   - 调整连接池大小参数
   - 优化数据库查询
   - 使用连接池统计信息分析性能瓶颈

### 调试技巧

```cpp
// 启用详细日志
pool.setLogCallback([](const std::string& level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
              << "] [" << level << "] " << message << std::endl;
});

// 定期输出统计信息
auto stats = pool.getStats();
std::cout << "Pool Status: " 
          << stats.active_connections << "/" << stats.total_connections 
          << " active, " << stats.pending_requests << " pending" << std::endl;
```

## 编译和部署

### 依赖项
- C++14 或更高版本的编译器
- MySQL客户端开发库 (libmysqlclient-dev)
- pthread库

### 编译命令
```bash
# Ubuntu/Debian
sudo apt-get install libmysqlclient-dev

# 编译示例
g++ -std=c++14 -I../include -L/usr/lib/mysql -lmysqlclient -lpthread \
    your_app.cpp mysql_connection_pool.cpp -o your_app
```

### CMake配置示例
```cmake
find_package(MySQL REQUIRED)
target_link_libraries(your_target ${MYSQL_LIBRARIES})
target_include_directories(your_target PRIVATE ${MYSQL_INCLUDE_DIRS})
```

## 许可证

本MySQL连接池实现遵循项目的整体许可证策略。

---

*最后更新: 2024年*