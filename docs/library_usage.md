# 告警系统库使用指南

## 概述

告警系统现在已经重构为一个完整的 C++ 库，可以轻松集成到其他应用程序中。该库提供了完整的告警监控功能，包括资源数据收集、告警规则管理、事件处理和通知功能。

## 核心特性

- 🔧 **易于集成**：简单的 API 接口，最少几行代码即可集成
- ⚙️ **高度配置化**：支持自定义数据库连接、监控间隔等所有参数
- 🔄 **异步处理**：多线程架构，不会阻塞主应用程序
- 📊 **实时监控**：提供详细的系统统计信息
- 🔔 **事件回调**：支持自定义告警事件处理逻辑
- 🛡️ **线程安全**：所有公共接口都是线程安全的

## 快速开始

### 1. 包含头文件

```cpp
#include "alarm_system.h"
```

### 2. 最简单的使用方式

```cpp
#include "alarm_system.h"

int main() {
    // 使用默认配置运行告警系统
    return runAlarmSystem();
}
```

### 3. 自定义配置

```cpp
#include "alarm_system.h"

int main() {
    AlarmSystemConfig config;
    config.enable_simulation = true;
    config.stats_interval = std::chrono::seconds(30);
    config.simulation_nodes = {"192.168.1.100", "192.168.1.101"};
    
    return runAlarmSystem(config);
}
```

## 详细 API 说明

### AlarmSystemConfig 配置结构体

```cpp
struct AlarmSystemConfig {
    // 数据库配置
    std::string tdengine_host = "127.0.0.1";        // TDengine 主机地址
    std::string mysql_host = "127.0.0.1";           // MySQL 主机地址
    int mysql_port = 3306;                          // MySQL 端口
    std::string db_user = "test";                   // 数据库用户名
    std::string db_password = "HZ715Net";           // 数据库密码
    std::string resource_db = "resource";            // 资源数据库名
    std::string alarm_db = "alarm";                 // 告警数据库名
    
    // HTTP服务器配置
    int http_port = 8080;                           // HTTP API 端口
    
    // 组播配置
    std::string multicast_ip = "239.192.168.80";    // 组播IP
    int multicast_port = 3980;                      // 组播端口
    
    // 监控配置
    std::chrono::seconds evaluation_interval = std::chrono::seconds(3);  // 告警评估间隔
    std::chrono::seconds stats_interval = std::chrono::seconds(60);      // 统计输出间隔
    
    // 模拟数据配置
    bool enable_simulation = true;                   // 是否启用模拟数据
    std::vector<std::string> simulation_nodes = {"192.168.1.100", "192.168.1.101"};  // 模拟节点列表
    std::chrono::seconds data_generation_interval = std::chrono::seconds(2);  // 数据生成间隔
    
    // 日志配置
    std::string log_config_file = "log_config.json";  // 日志配置文件
};
```

### AlarmSystem 类的主要方法

#### 构造和生命周期管理

```cpp
// 构造函数
AlarmSystem(const AlarmSystemConfig& config = AlarmSystemConfig{});

// 初始化系统（连接数据库、创建表等）
bool initialize();

// 启动系统（启动服务和监控线程）
bool start();

// 停止系统
void stop();

// 运行系统（简化的方式，包含初始化、启动和等待停止）
void run();

// 等待系统停止
void waitForStop();
```

#### 状态查询

```cpp
// 获取系统状态
AlarmSystemStatus getStatus() const;

// 获取详细统计信息
AlarmSystemStats getStats() const;

// 检查是否正在运行
bool isRunning() const;

// 获取最后的错误信息
std::string getLastError() const;
```

#### 配置和回调

```cpp
// 设置告警事件回调函数
void setAlarmEventCallback(const AlarmEventCallback& callback);

// 更新配置（需要重启才能生效）
void updateConfig(const AlarmSystemConfig& config);

// 获取当前配置
const AlarmSystemConfig& getConfig() const;
```

### 系统状态枚举

```cpp
enum class AlarmSystemStatus {
    STOPPED,    // 已停止
    STARTING,   // 启动中
    RUNNING,    // 运行中
    STOPPING,   // 停止中
    ERROR       // 错误状态
};
```

### 统计信息结构体

```cpp
struct AlarmSystemStats {
    std::chrono::seconds uptime{0};    // 运行时间
    int active_alarms = 0;             // 活跃告警数
    int total_alarms = 0;              // 总告警数
    int firing_events = 0;             // 触发事件数
    int resolved_events = 0;           // 恢复事件数
    int alarm_instances = 0;           // 告警实例数
    AlarmSystemStatus status = AlarmSystemStatus::STOPPED;  // 系统状态
};
```

## 使用模式

### 1. 独立运行模式

如果你只需要一个独立的告警系统：

```cpp
int main() {
    AlarmSystemConfig config;
    config.enable_simulation = true;
    
    // 这会一直运行直到收到 Ctrl+C 信号
    return runAlarmSystem(config);
}
```

### 2. 嵌入式集成模式

如果你需要将告警系统集成到现有应用程序中：

```cpp
class MyApplication {
private:
    AlarmSystem alarm_system_;
    
public:
    MyApplication() : alarm_system_(createConfig()) {
        // 设置告警事件处理
        alarm_system_.setAlarmEventCallback([this](const AlarmEvent& event) {
            handleAlarmEvent(event);
        });
    }
    
    bool start() {
        if (!alarm_system_.initialize()) {
            return false;
        }
        
        if (!alarm_system_.start()) {
            return false;
        }
        
        // 启动其他应用逻辑...
        return true;
    }
    
    void stop() {
        alarm_system_.stop();
        // 停止其他应用逻辑...
    }
    
private:
    void handleAlarmEvent(const AlarmEvent& event) {
        // 处理告警事件的业务逻辑
        if (event.status == "firing") {
            sendNotification(event);
        }
    }
    
    AlarmSystemConfig createConfig() {
        AlarmSystemConfig config;
        config.enable_simulation = false;  // 使用真实数据
        config.stats_interval = std::chrono::seconds(300);  // 5分钟输出一次统计
        return config;
    }
};
```

### 3. 监控和状态查询模式

如果你需要定期查询系统状态：

```cpp
AlarmSystem alarm_system(config);
alarm_system.initialize();
alarm_system.start();

// 在另一个线程中定期查询状态
std::thread monitor_thread([&]() {
    while (alarm_system.isRunning()) {
        auto stats = alarm_system.getStats();
        
        // 记录或显示统计信息
        logStats(stats);
        
        // 检查是否有严重问题
        if (stats.status == AlarmSystemStatus::ERROR) {
            handleSystemError(alarm_system.getLastError());
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
});
```

## 告警事件处理

### 事件回调函数

```cpp
alarm_system.setAlarmEventCallback([](const AlarmEvent& event) {
    std::cout << "收到告警事件: " << event.fingerprint << std::endl;
    std::cout << "状态: " << event.status << std::endl;
    
    if (event.status == "firing") {
        // 告警触发处理
        sendEmail(event);
        logToDatabase(event);
        triggerAutoRepair(event);
    } else if (event.status == "resolved") {
        // 告警恢复处理
        sendRecoveryNotification(event);
    }
});
```

### AlarmEvent 结构

```cpp
struct AlarmEvent {
    std::string fingerprint;                          // 告警唯一标识
    std::string status;                              // 状态: "firing" 或 "resolved"
    std::map<std::string, std::string> labels;       // 标签
    std::map<std::string, std::string> annotations;  // 注释
    std::chrono::system_clock::time_point starts_at; // 开始时间
    std::chrono::system_clock::time_point ends_at;   // 结束时间
    std::string generator_url;                       // 生成器URL
};
```

## 编译和链接

### CMakeLists.txt 示例

```cmake
# 链接告警系统库
target_link_libraries(your_app 
    alarm_system
    # 其他依赖库...
)

# 包含头文件路径
target_include_directories(your_app PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
)
```

### 依赖项

确保系统中已安装以下依赖：

- TDengine 客户端库
- MySQL 客户端库  
- spdlog 日志库
- nlohmann/json JSON库

## 最佳实践

### 1. 错误处理

```cpp
AlarmSystem alarm_system(config);

if (!alarm_system.initialize()) {
    std::cerr << "初始化失败: " << alarm_system.getLastError() << std::endl;
    return 1;
}

if (!alarm_system.start()) {
    std::cerr << "启动失败: " << alarm_system.getLastError() << std::endl;
    return 1;
}
```

### 2. 优雅关闭

```cpp
// 设置信号处理器
std::atomic<bool> should_stop{false};
signal(SIGINT, [](int) { should_stop = true; });

alarm_system.start();

while (!should_stop && alarm_system.isRunning()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

alarm_system.stop();  // 优雅关闭
```

### 3. 异步事件处理

```cpp
class EventProcessor {
private:
    std::queue<AlarmEvent> event_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread worker_thread_;
    std::atomic<bool> running_{true};
    
public:
    EventProcessor() : worker_thread_(&EventProcessor::processEvents, this) {}
    
    void handleEvent(const AlarmEvent& event) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        event_queue_.push(event);
        queue_cv_.notify_one();
    }
    
private:
    void processEvents() {
        while (running_) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return !event_queue_.empty() || !running_; });
            
            while (!event_queue_.empty()) {
                auto event = event_queue_.front();
                event_queue_.pop();
                lock.unlock();
                
                // 处理事件
                processEvent(event);
                
                lock.lock();
            }
        }
    }
};
```

## 常见问题

### Q: 如何禁用模拟数据生成？
A: 设置 `config.enable_simulation = false;`

### Q: 如何修改告警评估频率？
A: 设置 `config.evaluation_interval = std::chrono::seconds(你的间隔);`

### Q: 如何处理系统错误？
A: 检查 `getStatus()` 返回值，如果是 `ERROR` 状态，调用 `getLastError()` 获取详细错误信息。

### Q: 是否支持多实例？
A: 同一进程中只能有一个 AlarmSystem 实例，因为信号处理器是全局的。

### Q: 如何自定义日志配置？
A: 修改 `log_config.json` 文件或设置 `config.log_config_file` 路径。

## 示例代码

完整的示例代码请参考：

- `examples/simple_usage.cpp` - 简单使用示例
- `examples/integrated_usage.cpp` - 集成使用示例
- `src/main.cpp` - 命令行程序示例 