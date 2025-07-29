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
#include "json.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <signal.h>

// 全局实例指针，用于信号处理
AlarmSystem* AlarmSystem::s_instance = nullptr;



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
        // 1. 初始化资源存储
        LogManager::getLogger()->info("📦 初始化资源存储...");
        resource_storage_ = std::make_shared<ResourceStorage>(
            config_.tdengine_host, config_.db_user, config_.db_password);
        
        if (!resource_storage_->connect()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "连接TDengine失败";
            return false;
        }
        
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
        
        // 2. 初始化告警规则存储
        LogManager::getLogger()->info("📋 初始化告警规则存储...");
        alarm_rule_storage_ = std::make_shared<AlarmRuleStorage>(
            config_.mysql_host, config_.mysql_port, config_.db_user, 
            config_.db_password, config_.alarm_db);
        
        if (!alarm_rule_storage_->connect()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "连接MySQL失败";
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
        
        // 3. 初始化告警管理器
        LogManager::getLogger()->info("🚨 初始化告警管理器...");
        alarm_manager_ = std::make_shared<AlarmManager>(
            config_.mysql_host, config_.mysql_port, config_.db_user, 
            config_.db_password, config_.alarm_db);
        
        if (!alarm_manager_->connect()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "告警管理器连接数据库失败";
            return false;
        }
        
        if (!alarm_manager_->createEventTable()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "创建告警事件表失败";
            return false;
        }
        
        LogManager::getLogger()->info("✅ 告警管理器初始化成功");
        
        // 4. 初始化BMC存储
        LogManager::getLogger()->info("🗄️ 初始化BMC存储...");
        bmc_storage_ = std::make_shared<BMCStorage>(
            config_.tdengine_host, config_.db_user, config_.db_password, config_.resource_db);
        
        if (!bmc_storage_->connect()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "BMC存储连接失败: " + bmc_storage_->getLastError();
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
                        {"type", "alarm_event"},
                        {"fingerprint", event.fingerprint},
                        {"status", event.status},
                        {"starts_at", AlarmRuleEngine::formatTimestamp(event.starts_at)},
                        {"ends_at", event.status == "resolved" ? AlarmRuleEngine::formatTimestamp(event.ends_at) : ""},
                        {"generator_url", event.generator_url},
                        {"labels", event.labels},
                        {"annotations", event.annotations}
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