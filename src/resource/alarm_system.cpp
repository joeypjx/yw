#include "alarm_system.h"
#include "multicast_sender.h"
#include "http_server.h"
#include "node_storage.h"
#include "log_manager.h"
#include "resource_storage.h"
#include "node_status_monitor.h"
#include "resource_manager.h"
#include "alarm_rule_storage.h"
#include "alarm_rule_engine.h"
#include "alarm_manager.h"
#include "bmc_listener.h"
#include "bmc_storage.h"
#include "websocket_server.h"
#include "mysql_connection_pool.h"
#include "tdengine_connection_pool.h"
#include "json.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <signal.h>
#include <sstream>
#include <iomanip>

// 全局实例指针，用于信号处理
AlarmSystem* AlarmSystem::s_instance = nullptr;

// 辅助函数：格式化时间戳为ISO 8601字符串
static std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    
    // 添加8小时偏移量（中国时区 UTC+8）
    time_t += 8 * 3600;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// AlarmSystem 类的实现

AlarmSystem::AlarmSystem(const AlarmSystemConfig& config) 
    : config_(config)
    , status_(AlarmSystemStatus::STOPPED)
    , running_(false) {
    s_instance = this;
}

AlarmSystem::~AlarmSystem() {
    stop();
    s_instance = nullptr;
}

bool AlarmSystem::initialize() {
    status_ = AlarmSystemStatus::STARTING;
    
    if (!initializeLogger()) {
        status_ = AlarmSystemStatus::ERROR;
        return false;
    }
    
    if (!initializeSignalHandlers()) {
        status_ = AlarmSystemStatus::ERROR;
        return false;
    }
    
    if (!initializeDatabase()) {
        status_ = AlarmSystemStatus::ERROR;
        return false;
    }
    
    if (!initializeServices()) {
        status_ = AlarmSystemStatus::ERROR;
        return false;
    }
    
    // 启动系统
    running_ = true;
    start_time_ = std::chrono::steady_clock::now();
    status_ = AlarmSystemStatus::RUNNING;
    
    LogManager::getLogger()->info("✅ 告警系统初始化并启动完成");
    return true;
}


void AlarmSystem::stop() {
    if (status_ == AlarmSystemStatus::STOPPED) {
        return;
    }
    
    LogManager::getLogger()->info("🛑 正在停止告警系统...");
    status_ = AlarmSystemStatus::STOPPING;
    running_ = false;
    
    
    // 停止服务
    if (node_status_monitor_) {
        node_status_monitor_->stop();
    }
    if (multicast_sender_) {
        multicast_sender_->stop();
    }
    if (http_server_) {
        http_server_->stop();
    }
    if (alarm_rule_engine_) {
        alarm_rule_engine_->stop();
    }
    if (websocket_server_) {
        websocket_server_->stop();
    }
    
    // 停止BMC监听器
    bmc_listener_stop();
    bmc_listener_cleanup();
    
    
    status_ = AlarmSystemStatus::STOPPED;
    LogManager::getLogger()->info("✅ 告警系统已完全停止");
}

void AlarmSystem::waitForStop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void AlarmSystem::run() {
    if (!initialize()) {
        LogManager::getLogger()->critical("❌ 系统初始化失败: {}", getLastError());
        return;
    }
    
    LogManager::getLogger()->info("🔄 系统正在运行中，每{}秒输出一次统计信息...", config_.stats_interval.count());
    LogManager::getLogger()->info("💡 按 Ctrl+C 可以优雅停止程序");
    
    waitForStop();
    
    // 最终统计报告
    LogManager::getLogger()->info("🏁 系统已优雅停止，最终统计报告：");
    auto stats = getStats();
    LogManager::getLogger()->info("  - 活跃告警: {}", stats.active_alarms);
    LogManager::getLogger()->info("  - 总告警数: {}", stats.total_alarms);
    
    LogManager::getLogger()->info("✅ 告警系统已完全退出");
}

AlarmSystemStatus AlarmSystem::getStatus() const {
    return status_;
}

AlarmSystemStats AlarmSystem::getStats() const {
    AlarmSystemStats stats;
    stats.status = status_;
    
    if (running_) {
        auto current_time = std::chrono::steady_clock::now();
        stats.uptime = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - start_time_);
    }
    
    if (alarm_manager_) {
        stats.active_alarms = alarm_manager_->getActiveAlarmCount();
        stats.total_alarms = alarm_manager_->getTotalAlarmCount();
    }
    
    if (alarm_rule_engine_) {
        stats.alarm_instances = alarm_rule_engine_->getCurrentAlarmInstances().size();
    }
    
    return stats;
}

bool AlarmSystem::isRunning() const {
    return running_;
}

void AlarmSystem::setAlarmEventCallback(const AlarmEventCallback& callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    alarm_event_callback_ = callback;
}

std::string AlarmSystem::getLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

void AlarmSystem::updateConfig(const AlarmSystemConfig& config) {
    config_ = config;
}

const AlarmSystemConfig& AlarmSystem::getConfig() const {
    return config_;
}

// 私有方法实现

bool AlarmSystem::initializeLogger() {
    try {
        LogManager::init(config_.log_config_file);
        LogManager::getLogger()->info("📝 日志系统初始化完成");
        return true;
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = "日志系统初始化失败: " + std::string(e.what());
        return false;
    }
}

bool AlarmSystem::initializeSignalHandlers() {
    try {
        signal(SIGINT, signalHandler);  // Ctrl+C
        signal(SIGTERM, signalHandler); // 终止信号
        LogManager::getLogger()->info("📡 信号处理器已设置");
        return true;
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = "信号处理器设置失败: " + std::string(e.what());
        return false;
    }
}

bool AlarmSystem::initializeDatabase() {
    try {
        // 0. 创建共享的MySQL连接池
        LogManager::getLogger()->info("🗃️ 创建共享MySQL连接池...");
        MySQLPoolConfig mysql_config;
        mysql_config.host = config_.mysql_host;
        mysql_config.port = config_.mysql_port;
        mysql_config.user = config_.db_user;
        mysql_config.password = config_.db_password;
        mysql_config.database = config_.alarm_db;
        mysql_config.charset = "utf8mb4";
        
        // 连接池配置
        mysql_config.min_connections = 3;
        mysql_config.max_connections = 15;
        mysql_config.initial_connections = 5;
        
        // 超时配置
        mysql_config.connection_timeout = 30;
        mysql_config.idle_timeout = 600;      // 10分钟
        mysql_config.max_lifetime = 3600;     // 1小时
        mysql_config.acquire_timeout = 10;
        
        // 健康检查配置
        mysql_config.health_check_interval = 60;
        mysql_config.health_check_query = "SELECT 1";
        
        mysql_connection_pool_ = std::make_shared<MySQLConnectionPool>(mysql_config);
        if (!mysql_connection_pool_->initialize()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "MySQL连接池初始化失败";
            return false;
        }
        
        LogManager::getLogger()->info("✅ 共享MySQL连接池创建成功");
        
        // 0.2. 创建共享的TDengine连接池
        LogManager::getLogger()->info("🗃️ 创建共享TDengine连接池...");
        TDenginePoolConfig tdengine_config;
        tdengine_config.host = config_.tdengine_host;
        tdengine_config.port = 6030;
        tdengine_config.user = config_.db_user;
        tdengine_config.password = config_.db_password;
        tdengine_config.database = config_.resource_db;
        tdengine_config.locale = "C";
        tdengine_config.charset = "UTF-8";
        tdengine_config.timezone = "";
        
        // 连接池配置
        tdengine_config.min_connections = 2;
        tdengine_config.max_connections = 10;
        tdengine_config.initial_connections = 3;
        
        // 超时配置
        tdengine_config.connection_timeout = 30;
        tdengine_config.idle_timeout = 600;      // 10分钟
        tdengine_config.max_lifetime = 3600;     // 1小时
        tdengine_config.acquire_timeout = 10;
        
        // 健康检查配置
        tdengine_config.health_check_interval = 60;
        tdengine_config.health_check_query = "SELECT SERVER_VERSION()";
        
        tdengine_connection_pool_ = std::make_shared<TDengineConnectionPool>(tdengine_config);
        if (!tdengine_connection_pool_->initialize()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "TDengine连接池初始化失败";
            return false;
        }
        
        LogManager::getLogger()->info("✅ 共享TDengine连接池创建成功");
        
        // 1. 初始化资源存储（使用共享TDengine连接池）
        LogManager::getLogger()->info("📦 初始化资源存储...");
        resource_storage_ = std::make_shared<ResourceStorage>(tdengine_connection_pool_);
        
        if (!resource_storage_->createDatabase(config_.resource_db)) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "创建资源数据库失败";
            return false;
        }
        
        if (!resource_storage_->createResourceTable()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "创建资源表失败";
            return false;
        }
        
        LogManager::getLogger()->info("✅ 资源存储初始化成功");
        
        // 2. 初始化告警规则存储（使用共享连接池）
        LogManager::getLogger()->info("📋 初始化告警规则存储...");
        alarm_rule_storage_ = std::make_shared<AlarmRuleStorage>(mysql_connection_pool_);
        
        if (!alarm_rule_storage_->initialize()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "告警规则存储初始化失败";
            return false;
        }
        
        if (!alarm_rule_storage_->createDatabase()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "创建告警数据库失败";
            return false;
        }
        
        if (!alarm_rule_storage_->createTable()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "创建告警规则表失败";
            return false;
        }
        
        LogManager::getLogger()->info("✅ 告警规则存储初始化成功");
        
        // 3. 初始化告警管理器（使用共享连接池）
        LogManager::getLogger()->info("🚨 初始化告警管理器...");
        alarm_manager_ = std::make_shared<AlarmManager>(mysql_connection_pool_);
        
        if (!alarm_manager_->initialize()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "告警管理器初始化失败";
            return false;
        }
        
        // 创建数据库（如果不存在）
        if (!alarm_manager_->createDatabase()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "创建告警数据库失败";
            return false;
        }
        
        if (!alarm_manager_->createEventTable()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "创建告警事件表失败";
            return false;
        }
        
        LogManager::getLogger()->info("✅ 告警管理器初始化成功");
        
        // 4. 初始化BMC存储（使用共享TDengine连接池）
        LogManager::getLogger()->info("🗄️ 初始化BMC存储...");
        bmc_storage_ = std::make_shared<BMCStorage>(tdengine_connection_pool_);
        
        if (!bmc_storage_->initialize()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "BMC存储初始化失败: " + bmc_storage_->getLastError();
            return false;
        }
        
        if (!bmc_storage_->createBMCTables()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "创建BMC表失败: " + bmc_storage_->getLastError();
            return false;
        }
        
        LogManager::getLogger()->info("✅ BMC存储初始化成功");
        
        return true;
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = "数据库初始化异常: " + std::string(e.what());
        return false;
    }
}

bool AlarmSystem::initializeServices() {
    try {
        // 1. 初始化组播发送器
        LogManager::getLogger()->info("📡 初始化组播发送器...");
        multicast_sender_ = std::make_shared<MulticastSender>(
            config_.multicast_ip, config_.multicast_port);
        multicast_sender_->start();
        LogManager::getLogger()->info("✅ 组播发送器启动成功");
        
        // 2. 初始化节点存储和资源管理器
        LogManager::getLogger()->info("📦 初始化节点存储...");
        node_storage_ = std::make_shared<NodeStorage>();
        LogManager::getLogger()->info("✅ 节点存储初始化成功");
        
        LogManager::getLogger()->info("📊 初始化资源管理器...");
        resource_manager_ = std::make_shared<ResourceManager>(resource_storage_, node_storage_, bmc_storage_);
        LogManager::getLogger()->info("✅ 资源管理器初始化成功");
        
        // 3. 启动HTTP服务器
        LogManager::getLogger()->info("🌐 启动HTTP服务器...");
        http_server_ = std::make_shared<HttpServer>(resource_storage_, alarm_rule_storage_, alarm_manager_, node_storage_, resource_manager_, bmc_storage_);
        if (!http_server_->start()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "HTTP服务器启动失败";
            return false;
        }
        LogManager::getLogger()->info("✅ HTTP服务器启动成功，端口: {}", config_.http_port);
        
        // 4. 初始化告警规则引擎
        LogManager::getLogger()->info("⚙️ 初始化告警规则引擎...");
        alarm_rule_engine_ = std::make_shared<AlarmRuleEngine>(
            alarm_rule_storage_, resource_storage_);
        
        // 5. 设置告警事件回调
        alarm_rule_engine_->setAlarmEventCallback([this](const AlarmEvent& event) {
            // 处理告警事件到 AlarmManager
            if (alarm_manager_) {
                alarm_manager_->processAlarmEvent(event);
            }
            
            // 通过WebSocket广播告警事件
            if (websocket_server_) {
                try {
                    // 将告警事件转换为JSON格式
                    nlohmann::json alarm_json = {
                        {"fingerprint", event.fingerprint},
                        {"status", event.status},
                        {"labels", event.labels},
                        {"annotations", event.annotations},
                        {"starts_at", formatTimestamp(event.starts_at)},
                        {"ends_at", formatTimestamp(event.ends_at)}
                    };
                    
                    // 广播告警事件
                    websocket_server_->broadcast(alarm_json.dump());
                    
                    // 从labels中获取告警名称用于日志
                    std::string alert_name = event.labels.count("alertname") ? event.labels.at("alertname") : "unknown";
                    LogManager::getLogger()->debug("告警事件已通过WebSocket广播: {}", alert_name);
                } catch (const std::exception& e) {
                    LogManager::getLogger()->error("WebSocket广播告警失败: {}", e.what());
                }
            }
            
            // 调用用户设置的回调函数
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (alarm_event_callback_) {
                alarm_event_callback_(event);
            }
        });
        
        // 设置评估间隔
        alarm_rule_engine_->setEvaluationInterval(config_.evaluation_interval);
        
        // 6. 启动告警引擎
        if (!alarm_rule_engine_->start()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "告警引擎启动失败";
            return false;
        }
        LogManager::getLogger()->info("✅ 告警规则引擎启动成功");
        
        // 7. 初始化节点状态监控器
        LogManager::getLogger()->info("👁️ 初始化节点状态监控器...");
        node_status_monitor_ = std::make_shared<NodeStatusMonitor>(node_storage_, alarm_manager_);
        node_status_monitor_->setNodeStatusChangeCallback([this](const std::string& host_ip, const std::string& old_status, const std::string& new_status) {

            std::string fingerprint = alarm_manager_->calculateFingerprint("NodeOffline", {
                {"host_ip", host_ip}
            });

            if (new_status == "offline") {
                // 1) 直接构造 AlarmEvent 并提交给 AlarmManager
                AlarmEvent event;
                event.fingerprint = fingerprint;
                event.status = "firing";
                event.starts_at = std::chrono::system_clock::now();
                event.labels = {
                    {"alert_name", "节点离线"},
                    {"host_ip", host_ip},
                    {"severity", "严重"},
                    {"alert_type", "硬件资源"}
                };
                event.annotations = {
                    {"summary", "节点离线"},
                    {"description", std::string("与节点 ") + host_ip + " 失联。"}
                };
                alarm_manager_->processAlarmEvent(event);

                // 2) 仍保留原有的WebSocket广播
                nlohmann::json labels_json = event.labels;
                nlohmann::json annotations = event.annotations;
                nlohmann::json alarm_json = {
                    {"labels", labels_json},
                    {"annotations", annotations}
                };
                websocket_server_->broadcast(alarm_json.dump());

                LogManager::getLogger()->warn("Node '{}' is offline.", host_ip);
            } else if (new_status == "online") {
                // 直接构造已解决的 AlarmEvent 并提交
                AlarmEvent event;
                event.fingerprint = fingerprint;
                event.status = "resolved";
                event.ends_at = std::chrono::system_clock::now();
                alarm_manager_->processAlarmEvent(event);

                LogManager::getLogger()->info("Node '{}' is back online.", host_ip);
            }
        });
        node_status_monitor_->start();
        LogManager::getLogger()->info("✅ 节点状态监控器启动成功");
        
        // 6. 启动BMC监听器
        LogManager::getLogger()->info("🔊 初始化BMC监听器...");
        if (bmc_listener_init(config_.bmc_multicast_ip.c_str(), config_.bmc_multicast_port) != 0) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "BMC监听器初始化失败";
            return false;
        }
        
        // 设置BMC数据回调函数
        bmc_listener_set_callback([this](const UdpInfo& data) {
            LogManager::getLogger()->debug("收到BMC数据");
            
            // 直接存储BMC数据到数据库
            if (bmc_storage_) {
                if (!bmc_storage_->storeBMCData(data)) {
                    LogManager::getLogger()->warn("BMC数据存储失败");
                }
            }

            // 存储到节点存储
            if (node_storage_) {
                if (!node_storage_->storeUDPInfo(data)) {
                    LogManager::getLogger()->warn("节点存储失败");
                }
            }
        });
        
        bmc_listener_start();
        LogManager::getLogger()->info("✅ BMC监听器启动成功");
        
        // 8. 初始化WebSocket服务器
        LogManager::getLogger()->info("🔌 初始化WebSocket服务器...");
        websocket_server_ = std::make_shared<WebSocketServer>();
        websocket_server_->start(config_.websocket_port);
        LogManager::getLogger()->info("✅ WebSocket服务器启动成功，端口: {}", config_.websocket_port);
        
        return true;
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = "服务初始化异常: " + std::string(e.what());
        return false;
    }
}



void AlarmSystem::signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        if (s_instance) {
            LogManager::getLogger()->info("🛑 接收到停止信号，正在优雅关闭系统...");
            s_instance->running_ = false;
        }
    }
}

// 便利函数实现
int runAlarmSystem() {
    try {
        AlarmSystem system;
        system.run();
        return 0;
    } catch (const std::exception& e) {
        if (LogManager::getLogger()) {
            LogManager::getLogger()->critical("❌ 系统异常: {}", e.what());
        } else {
            std::cerr << "❌ 系统异常: " << e.what() << std::endl;
        }
        return 1;
    }
} 