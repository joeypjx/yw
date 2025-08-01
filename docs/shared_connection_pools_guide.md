# 共享连接池架构指南

## 概述

本文档描述了告警系统中实现的共享连接池架构，该架构优化了数据库连接的管理，提高了系统性能和资源利用率。

## 架构设计

### 设计原则

1. **连接复用**: 多个组件共享同一个连接池，减少数据库连接数
2. **统一管理**: 在 `AlarmSystem` 中集中创建和管理连接池
3. **依赖注入**: 通过构造函数注入连接池到各个组件
4. **职责分离**: 连接池负责连接管理，组件负责业务逻辑

### 架构图

```
AlarmSystem
├── MySQL连接池 (共享)
│   ├── AlarmManager (注入)
│   └── AlarmRuleStorage (注入)
└── TDengine连接池 (共享)
    ├── ResourceStorage (注入)
    └── BMCStorage (注入)
```

## 实现细节

### MySQL连接池共享

#### 创建位置
```cpp
// 在 AlarmSystem::initializeDatabase() 中
MySQLPoolConfig mysql_config;
mysql_config.host = config_.mysql_host;
mysql_config.port = config_.mysql_port;
mysql_config.user = config_.db_user;
mysql_config.password = config_.db_password;
mysql_config.database = config_.alarm_db;

mysql_connection_pool_ = std::make_shared<MySQLConnectionPool>(mysql_config);
```

#### 注入到组件
```cpp
// AlarmRuleStorage 使用共享连接池
alarm_rule_storage_ = std::make_shared<AlarmRuleStorage>(mysql_connection_pool_);

// AlarmManager 使用共享连接池
alarm_manager_ = std::make_shared<AlarmManager>(mysql_connection_pool_);
```

### TDengine连接池共享

#### 创建位置
```cpp
// 在 AlarmSystem::initializeDatabase() 中
TDenginePoolConfig tdengine_config;
tdengine_config.host = config_.tdengine_host;
tdengine_config.port = 6030;
tdengine_config.user = config_.db_user;
tdengine_config.password = config_.db_password;
tdengine_config.database = config_.resource_db;

tdengine_connection_pool_ = std::make_shared<TDengineConnectionPool>(tdengine_config);
```

#### 注入到组件
```cpp
// ResourceStorage 使用共享连接池
resource_storage_ = std::make_shared<ResourceStorage>(tdengine_connection_pool_);

// BMCStorage 使用共享连接池
bmc_storage_ = std::make_shared<BMCStorage>(tdengine_connection_pool_);
```

## 构造函数设计

每个数据存储组件现在支持三种构造方式：

### 1. 连接池注入构造函数（推荐）
```cpp
// 使用共享连接池
AlarmManager(std::shared_ptr<MySQLConnectionPool> connection_pool);
ResourceStorage(std::shared_ptr<TDengineConnectionPool> connection_pool);
```

### 2. 连接池配置构造函数
```cpp
// 创建独立连接池
AlarmManager(const MySQLPoolConfig& pool_config);
ResourceStorage(const TDenginePoolConfig& pool_config);
```

### 3. 兼容性构造函数
```cpp
// 向后兼容的单连接方式
AlarmManager(const std::string& host, int port, const std::string& user, 
             const std::string& password, const std::string& database);
```

## 连接池所有权管理

### 所有权标记
每个组件都有一个 `m_owns_connection_pool` 标记：

```cpp
private:
    std::shared_ptr<MySQLConnectionPool> m_connection_pool;
    bool m_owns_connection_pool;  // 是否拥有连接池所有权
```

### 初始化和关闭逻辑
```cpp
bool initialize() {
    // 只有拥有所有权的组件才初始化连接池
    if (m_owns_connection_pool) {
        if (!m_connection_pool->initialize()) {
            return false;
        }
    }
    // ... 其他初始化逻辑
}

void shutdown() {
    // 只有拥有所有权的组件才关闭连接池
    if (m_connection_pool && m_owns_connection_pool) {
        m_connection_pool->shutdown();
    }
    // ... 其他清理逻辑
}
```

## 配置参数

### MySQL连接池配置
```cpp
MySQLPoolConfig mysql_config;
mysql_config.host = "localhost";
mysql_config.port = 3306;
mysql_config.user = "test";
mysql_config.password = "HZ715Net";
mysql_config.database = "alarm";
mysql_config.charset = "utf8mb4";

// 连接池大小
mysql_config.min_connections = 3;
mysql_config.max_connections = 15;
mysql_config.initial_connections = 5;

// 超时配置
mysql_config.connection_timeout = 30;      // 连接超时
mysql_config.idle_timeout = 600;           // 空闲超时(10分钟)
mysql_config.max_lifetime = 3600;          // 最大生存时间(1小时)
mysql_config.acquire_timeout = 10;         // 获取连接超时

// 健康检查
mysql_config.health_check_interval = 60;   // 健康检查间隔
mysql_config.health_check_query = "SELECT 1";
```

### TDengine连接池配置
```cpp
TDenginePoolConfig tdengine_config;
tdengine_config.host = "localhost";
tdengine_config.port = 6030;
tdengine_config.user = "test";
tdengine_config.password = "HZ715Net";
tdengine_config.database = "resource";
tdengine_config.locale = "C";
tdengine_config.charset = "UTF-8";

// 连接池大小
tdengine_config.min_connections = 2;
tdengine_config.max_connections = 10;
tdengine_config.initial_connections = 3;

// 超时配置
tdengine_config.connection_timeout = 30;
tdengine_config.idle_timeout = 600;
tdengine_config.max_lifetime = 3600;
tdengine_config.acquire_timeout = 10;

// 健康检查
tdengine_config.health_check_interval = 60;
tdengine_config.health_check_query = "SELECT SERVER_VERSION()";
```

## 使用示例

### 在AlarmSystem中使用
```cpp
class AlarmSystem {
private:
    std::shared_ptr<MySQLConnectionPool> mysql_connection_pool_;
    std::shared_ptr<TDengineConnectionPool> tdengine_connection_pool_;
    
    bool initializeDatabase() {
        // 创建共享连接池
        mysql_connection_pool_ = std::make_shared<MySQLConnectionPool>(mysql_config);
        tdengine_connection_pool_ = std::make_shared<TDengineConnectionPool>(tdengine_config);
        
        // 注入到各个组件
        alarm_manager_ = std::make_shared<AlarmManager>(mysql_connection_pool_);
        resource_storage_ = std::make_shared<ResourceStorage>(tdengine_connection_pool_);
        
        return true;
    }
};
```

### 在独立应用中使用
```cpp
int main() {
    // 创建连接池
    auto mysql_pool = std::make_shared<MySQLConnectionPool>(mysql_config);
    
    // 注入到多个组件
    auto alarm_manager = std::make_shared<AlarmManager>(mysql_pool);
    auto alarm_rule_storage = std::make_shared<AlarmRuleStorage>(mysql_pool);
    
    // 共享同一个连接池
    return 0;
}
```

## 性能优势

### 1. 连接数减少
- **之前**: 每个组件独立连接池，4个组件 = 4 × 5 = 20个连接
- **现在**: 共享连接池，MySQL: 5个连接，TDengine: 3个连接，总计8个连接
- **节省**: 60%的连接数

### 2. 内存使用优化
- 减少了连接对象的内存占用
- 减少了连接池管理结构的内存开销

### 3. 连接利用率提升
- 连接在多个组件间复用，提高利用率
- 减少连接创建和销毁的开销

## 监控和调试

### 连接池统计
```cpp
// 获取MySQL连接池统计
auto mysql_stats = mysql_connection_pool_->getStats();
std::cout << "MySQL连接池统计:" << std::endl;
std::cout << "  总连接数: " << mysql_stats.total_connections << std::endl;
std::cout << "  活跃连接数: " << mysql_stats.active_connections << std::endl;
std::cout << "  空闲连接数: " << mysql_stats.idle_connections << std::endl;

// 获取TDengine连接池统计
auto tdengine_stats = tdengine_connection_pool_->getStats();
std::cout << "TDengine连接池统计:" << std::endl;
std::cout << "  总连接数: " << tdengine_stats.total_connections << std::endl;
std::cout << "  活跃连接数: " << tdengine_stats.active_connections << std::endl;
```

### 健康检查
```cpp
// 检查连接池健康状态
bool mysql_healthy = mysql_connection_pool_->isHealthy();
bool tdengine_healthy = tdengine_connection_pool_->isHealthy();
```

## 最佳实践

### 1. 配置调优
- 根据业务负载调整连接池大小
- 设置合适的超时时间
- 启用健康检查

### 2. 错误处理
- 检查连接池初始化状态
- 处理连接获取失败的情况
- 记录连接池相关的错误日志

### 3. 资源管理
- 使用RAII确保连接正确释放
- 避免长时间持有连接
- 及时释放不再使用的连接

## 故障排除

### 常见问题

1. **连接池初始化失败**
   - 检查数据库服务是否启动
   - 验证连接参数是否正确
   - 检查网络连通性

2. **连接获取超时**
   - 增加连接池大小
   - 检查是否有连接泄露
   - 调整获取超时时间

3. **连接频繁断开**
   - 检查数据库服务稳定性
   - 调整连接最大生存时间
   - 启用连接健康检查

### 调试技巧

1. **启用详细日志**
   ```cpp
   connection_pool->setLogCallback([](const std::string& level, const std::string& message) {
       std::cout << "[" << level << "] " << message << std::endl;
   });
   ```

2. **监控连接池状态**
   ```cpp
   // 定期输出连接池统计信息
   auto stats = connection_pool->getStats();
   LogManager::getLogger()->info("连接池状态: 总连接={}, 活跃={}, 空闲={}", 
                                stats.total_connections, 
                                stats.active_connections, 
                                stats.idle_connections);
   ```

## 迁移指南

### 从独立连接池迁移

1. **更新构造函数调用**
   ```cpp
   // 之前
   auto alarm_manager = std::make_shared<AlarmManager>(host, port, user, password, database);
   
   // 现在
   auto alarm_manager = std::make_shared<AlarmManager>(shared_mysql_pool);
   ```

2. **移除重复的连接池创建**
   ```cpp
   // 之前每个组件都创建连接池
   // 现在只在AlarmSystem中创建一次
   ```

3. **更新初始化流程**
   ```cpp
   // 确保连接池在组件之前初始化
   // 按正确顺序初始化各个组件
   ```

## 总结

共享连接池架构带来了以下主要优势：

1. **资源效率**: 大幅减少数据库连接数和内存使用
2. **性能提升**: 连接复用提高了整体性能
3. **管理简化**: 集中管理连接池配置和监控
4. **扩展性**: 易于添加新的数据存储组件

这种架构设计为告警系统提供了更好的可维护性和性能表现。