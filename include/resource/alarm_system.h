#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>

// 前向声明
class ResourceStorage;
class AlarmRuleStorage;
class AlarmManager;
class AlarmRuleEngine;
class HttpServer;
class MulticastSender;
class NodeStorage;
class ResourceManager;
class NodeStatusMonitor;
class BMCListener;
class BMCStorage;
class WebSocketServer;
struct AlarmEvent;

/**
 * 告警系统配置结构体
 */
struct AlarmSystemConfig {
    // 数据库配置
    std::string tdengine_host = "127.0.0.1";
    std::string mysql_host = "127.0.0.1";
    int mysql_port = 3306;
    std::string db_user = "test";
    std::string db_password = "HZ715Net";
    std::string resource_db = "resource";
    std::string alarm_db = "alarm";
    
    // HTTP服务器配置
    int http_port = 8080;
    
    // 组播配置
    std::string multicast_ip = "239.192.168.80";
    int multicast_port = 3980;
    
    // BMC监听配置
    std::string bmc_multicast_ip = "224.100.200.15";
    int bmc_multicast_port = 5715;
    
    // WebSocket服务器配置
    int websocket_port = 8081;
    
    // 监控配置
    std::chrono::seconds evaluation_interval = std::chrono::seconds(3);
    std::chrono::seconds stats_interval = std::chrono::seconds(60);
    
    
    // 日志配置
    std::string log_config_file = "log_config.json";
};

/**
 * 告警系统状态枚举
 */
enum class AlarmSystemStatus {
    STOPPED,
    STARTING,
    RUNNING,
    STOPPING,
    ERROR
};

/**
 * 告警系统统计信息
 */
struct AlarmSystemStats {
    std::chrono::seconds uptime{0};
    int active_alarms = 0;
    int total_alarms = 0;
    int firing_events = 0;
    int resolved_events = 0;
    int alarm_instances = 0;
    AlarmSystemStatus status = AlarmSystemStatus::STOPPED;
};

/**
 * 告警事件回调函数类型
 */
using AlarmEventCallback = std::function<void(const AlarmEvent&)>;

/**
 * 告警系统主类
 * 
 * 提供完整的告警监控系统功能，包括：
 * - 资源数据收集和存储
 * - 告警规则管理
 * - 告警事件处理
 * - HTTP API服务
 * - 组播通知
 */
class AlarmSystem {
public:
    /**
     * 构造函数
     * @param config 系统配置
     */
    explicit AlarmSystem(const AlarmSystemConfig& config = AlarmSystemConfig{});
    
    /**
     * 析构函数
     */
    ~AlarmSystem();
    
    // 禁用拷贝构造和赋值
    AlarmSystem(const AlarmSystem&) = delete;
    AlarmSystem& operator=(const AlarmSystem&) = delete;
    
    /**
     * 初始化并启动告警系统
     * @return 成功返回true，失败返回false
     */
    bool initialize();
    
    /**
     * 停止告警系统
     */
    void stop();
    
    /**
     * 等待系统停止（阻塞调用）
     */
    void waitForStop();
    
    /**
     * 运行系统（阻塞调用，内部处理信号）
     * 这是最简单的使用方式，会一直运行直到收到停止信号
     */
    void run();
    
    /**
     * 获取系统状态
     */
    AlarmSystemStatus getStatus() const;
    
    /**
     * 获取系统统计信息
     */
    AlarmSystemStats getStats() const;
    
    /**
     * 是否正在运行
     */
    bool isRunning() const;
    
    /**
     * 设置告警事件回调函数
     * @param callback 回调函数
     */
    void setAlarmEventCallback(const AlarmEventCallback& callback);
    
    /**
     * 获取最后的错误信息
     */
    std::string getLastError() const;
    
    /**
     * 更新配置（需要重启才能生效）
     * @param config 新配置
     */
    void updateConfig(const AlarmSystemConfig& config);
    
    /**
     * 获取当前配置
     */
    const AlarmSystemConfig& getConfig() const;

private:
    // 内部实现方法
    bool initializeLogger();
    bool initializeSignalHandlers();
    bool initializeDatabase();
    bool initializeServices();
    
    // 信号处理
    static void signalHandler(int signal);
    static AlarmSystem* s_instance;
    
    // 配置和状态
    AlarmSystemConfig config_;
    std::atomic<AlarmSystemStatus> status_;
    std::atomic<bool> running_;
    std::string last_error_;
    mutable std::mutex error_mutex_;
    
    // 系统组件
    std::shared_ptr<ResourceStorage> resource_storage_;
    std::shared_ptr<AlarmRuleStorage> alarm_rule_storage_;
    std::shared_ptr<AlarmManager> alarm_manager_;
    std::shared_ptr<AlarmRuleEngine> alarm_rule_engine_;
    std::shared_ptr<HttpServer> http_server_;
    std::shared_ptr<MulticastSender> multicast_sender_;
    std::shared_ptr<NodeStorage> node_storage_;
    std::shared_ptr<ResourceManager> resource_manager_;
    std::shared_ptr<NodeStatusMonitor> node_status_monitor_;
    std::shared_ptr<BMCListener> bmc_listener_;
    std::shared_ptr<BMCStorage> bmc_storage_;
    std::shared_ptr<WebSocketServer> websocket_server_;
    
    
    // 时间记录
    std::chrono::steady_clock::time_point start_time_;
    
    // 回调函数
    AlarmEventCallback alarm_event_callback_;
    std::mutex callback_mutex_;
};

/**
 * 便利函数：创建默认配置的告警系统并运行
 * @param config 可选的配置参数
 * @return 退出码
 */
int runAlarmSystem(); 