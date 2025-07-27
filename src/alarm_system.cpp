#include "alarm_system.h"
#include "multicast_sender.h"
#include "http_server.h"
#include "node_storage.h"
#include "log_manager.h"
#include "resource_storage.h"
#include "resource_manager.h"
#include "alarm_rule_storage.h"
#include "alarm_rule_engine.h"
#include "alarm_manager.h"
#include "json.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>
#include <vector>
#include <string>
#include <map>
#include <signal.h>

// 全局实例指针，用于信号处理
AlarmSystem* AlarmSystem::s_instance = nullptr;

// 告警事件监控器类（从 main.cpp 移动过来）
class AlarmEventMonitor {
private:
    std::atomic<int> firing_count{0};
    std::atomic<int> resolved_count{0};
    std::vector<std::string> recent_events;
    std::mutex events_mutex;
    
public:
    void onAlarmEvent(const AlarmEvent& event) {
        std::lock_guard<std::mutex> lock(events_mutex);
        
        if (event.status == "firing") {
            firing_count++;
            LogManager::getLogger()->warn("🔥 [告警触发] {}", event.fingerprint);
            if (event.annotations.find("summary") != event.annotations.end()) {
                LogManager::getLogger()->warn("   摘要: {}", event.annotations.at("summary"));
            }
            if (event.annotations.find("description") != event.annotations.end()) {
                LogManager::getLogger()->info("   描述: {}", event.annotations.at("description"));
            }
        } else if (event.status == "resolved") {
            resolved_count++;
            LogManager::getLogger()->info("✅ [告警恢复] {}", event.fingerprint);
        }
        
        recent_events.push_back(event.fingerprint + " - " + event.status);
        if (recent_events.size() > 10) {
            recent_events.erase(recent_events.begin());
        }
    }
    
    void printStatistics() {
        std::lock_guard<std::mutex> lock(events_mutex);
        
        LogManager::getLogger()->info("  - 触发次数: {}", firing_count.load());
        LogManager::getLogger()->info("  - 恢复次数: {}", resolved_count.load());
        
        for (const auto& event : recent_events) {
            LogManager::getLogger()->info("    {}", event);
        }
    }
    
    int getFiringCount() const { return firing_count.load(); }
    int getResolvedCount() const { return resolved_count.load(); }
};

// 资源数据生成器类（从 main.cpp 移动过来）
class ResourceDataGenerator {
private:
    std::mt19937 gen;
    std::uniform_real_distribution<> cpu_dist;
    std::uniform_real_distribution<> memory_dist;
    std::uniform_real_distribution<> disk_dist;
    
public:
    ResourceDataGenerator() : gen(std::random_device{}()),
                            cpu_dist(10.0, 75.0),    // 正常CPU使用率
                            memory_dist(20.0, 80.0),  // 正常内存使用率
                            disk_dist(30.0, 70.0) {}  // 正常磁盘使用率
    
    nlohmann::json generateResourceData(bool high_usage = false) {
        // 如果需要高使用率，调整分布范围
        double cpu_usage = high_usage ? 
            std::uniform_real_distribution<>(88.0, 98.0)(gen) : cpu_dist(gen);
        double memory_usage = high_usage ? 
            std::uniform_real_distribution<>(88.0, 95.0)(gen) : memory_dist(gen);
        double disk_usage_root = high_usage ? 
            std::uniform_real_distribution<>(95.0, 98.0)(gen) : disk_dist(gen);
        double disk_usage_data = high_usage ? 
            std::uniform_real_distribution<>(90.0, 95.0)(gen) : std::uniform_real_distribution<>(70.0, 85.0)(gen);

        return {
            {"cpu", {
                {"usage_percent", cpu_usage},
                {"load_avg_1m", cpu_usage / 40.0 + 1.0},
                {"load_avg_5m", cpu_usage / 50.0 + 1.0},
                {"load_avg_15m", cpu_usage / 60.0 + 1.0},
                {"core_count", 8},
                {"core_allocated", 0},
                {"temperature", high_usage ? 75.0 + (gen() % 10) : 45.0 + (gen() % 5)},
                {"voltage", 0},
                {"current", 0},
                {"power", high_usage ? 120.0 + (gen() % 20) : 60.0 + (gen() % 10)}
            }},
            {"memory", {
                {"total", 15647768576LL},
                {"used", static_cast<long long>(15647768576LL * memory_usage / 100.0)},
                {"free", static_cast<long long>(15647768576LL * (100.0 - memory_usage) / 100.0)},
                {"usage_percent", memory_usage}
            }},
            {"network", nlohmann::json::array({
                {
                    {"interface", "bond0"},
                    {"rx_bytes", 1000000 + static_cast<long long>(gen() % 500000)},
                    {"tx_bytes", 2000000 + static_cast<long long>(gen() % 800000)},
                    {"rx_packets", 1000 + static_cast<int>(gen() % 500)},
                    {"tx_packets", 1500 + static_cast<int>(gen() % 800)},
                    {"rx_errors", gen() % 2},
                    {"tx_errors", gen() % 2},
                    {"rx_rate", static_cast<long long>(gen() % 1000)},
                    {"tx_rate", static_cast<long long>(gen() % 1000)}
                },
                {
                    {"interface", "docker0"},
                    {"rx_bytes", 1225656 + static_cast<long long>(gen() % 100000)},
                    {"tx_bytes", 1143594 + static_cast<long long>(gen() % 100000)},
                    {"rx_packets", 5204 + static_cast<int>(gen() % 100)},
                    {"tx_packets", 4407 + static_cast<int>(gen() % 100)},
                    {"rx_errors", 0},
                    {"tx_errors", 0},
                    {"rx_rate", static_cast<long long>(gen() % 1000)},
                    {"tx_rate", static_cast<long long>(gen() % 500)}
                }
            })},
            {"disk", nlohmann::json::array({
                {
                    {"device", "/dev/mapper/klas-root"},
                    {"mount_point", "/"},
                    {"total", 107321753600LL},
                    {"used", static_cast<long long>(107321753600LL * disk_usage_root / 100.0)},
                    {"free", static_cast<long long>(107321753600LL * (100.0 - disk_usage_root) / 100.0)},
                    {"usage_percent", disk_usage_root}
                },
                {
                    {"device", "/dev/mapper/klas-data"},
                    {"mount_point", "/data"},
                    {"total", 255149084672LL},
                    {"used", static_cast<long long>(255149084672LL * disk_usage_data / 100.0)},
                    {"free", static_cast<long long>(255149084672LL * (100.0 - disk_usage_data) / 100.0)},
                    {"usage_percent", disk_usage_data}
                }
            })},
            {"gpu", nlohmann::json::array({
                {
                    {"index", 0},
                    {"name", "Iluvatar MR-V50A"},
                    {"compute_usage", high_usage ? std::uniform_real_distribution<>(80.0, 95.0)(gen) : std::uniform_real_distribution<>(0.0, 10.0)(gen)},
                    {"mem_usage", high_usage ? std::uniform_real_distribution<>(70.0, 90.0)(gen) : std::uniform_real_distribution<>(0.0, 10.0)(gen)},
                    {"mem_used", static_cast<long long>(17179869184LL * (high_usage ? 0.8 : 0.1))},
                    {"mem_total", 17179869184LL},
                    {"temperature", high_usage ? 85.0 + (gen() % 5) : 46.0 + (gen() % 3)},
                    {"power", high_usage ? 200.0 + (gen() % 50) : 19.0 + (gen() % 5)}
                }
            })},
            {"gpu_allocated", 0},
            {"gpu_num", 1}
        };
    }
};

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
    
    LogManager::getLogger()->info("✅ 告警系统初始化完成");
    return true;
}

bool AlarmSystem::start() {
    if (status_ != AlarmSystemStatus::STARTING && status_ != AlarmSystemStatus::STOPPED) {
        last_error_ = "系统必须处于停止或初始化状态才能启动";
        return false;
    }
    
    running_ = true;
    start_time_ = std::chrono::steady_clock::now();
    
    LogManager::getLogger()->info("🎯 启动告警系统...");
    
    // 启动数据生成
    if (config_.enable_simulation && !startDataGeneration()) {
        running_ = false;
        status_ = AlarmSystemStatus::ERROR;
        return false;
    }
    
    // 启动监控线程
    monitoring_thread_ = std::thread(&AlarmSystem::monitoringLoop, this);
    
    status_ = AlarmSystemStatus::RUNNING;
    LogManager::getLogger()->info("✅ 告警系统启动成功");
    
    return true;
}

void AlarmSystem::stop() {
    if (status_ == AlarmSystemStatus::STOPPED) {
        return;
    }
    
    LogManager::getLogger()->info("🛑 正在停止告警系统...");
    status_ = AlarmSystemStatus::STOPPING;
    running_ = false;
    
    // 停止数据生成
    stopDataGeneration();
    
    // 停止服务
    if (multicast_sender_) {
        multicast_sender_->stop();
    }
    if (http_server_) {
        http_server_->stop();
    }
    if (alarm_rule_engine_) {
        alarm_rule_engine_->stop();
    }
    
    // 等待监控线程结束
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    
    status_ = AlarmSystemStatus::STOPPED;
    LogManager::getLogger()->info("✅ 告警系统已完全停止");
}

void AlarmSystem::waitForStop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

void AlarmSystem::run() {
    if (!initialize()) {
        LogManager::getLogger()->critical("❌ 系统初始化失败: {}", getLastError());
        return;
    }
    
    if (!start()) {
        LogManager::getLogger()->critical("❌ 系统启动失败: {}", getLastError());
        return;
    }
    
    LogManager::getLogger()->info("🔄 系统正在运行中，每{}秒输出一次统计信息...", config_.stats_interval.count());
    LogManager::getLogger()->info("💡 按 Ctrl+C 可以优雅停止程序");
    
    waitForStop();
    
    // 最终统计报告
    LogManager::getLogger()->info("🏁 系统已优雅停止，最终统计报告：");
    if (alarm_monitor_) {
        alarm_monitor_->printStatistics();
    }
    
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
    
    if (alarm_monitor_) {
        stats.firing_events = alarm_monitor_->getFiringCount();
        stats.resolved_events = alarm_monitor_->getResolvedCount();
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
        resource_manager_ = std::make_shared<ResourceManager>(resource_storage_, node_storage_);
        LogManager::getLogger()->info("✅ 资源管理器初始化成功");
        
        // 3. 启动HTTP服务器
        LogManager::getLogger()->info("🌐 启动HTTP服务器...");
        http_server_ = std::make_shared<HttpServer>(resource_storage_, alarm_rule_storage_, alarm_manager_, node_storage_, resource_manager_);
        if (!http_server_->start()) {
            std::lock_guard<std::mutex> lock(error_mutex_);
            last_error_ = "HTTP服务器启动失败";
            return false;
        }
        LogManager::getLogger()->info("✅ HTTP服务器启动成功，端口: {}", config_.http_port);
        
        // 4. 初始化告警规则引擎
        LogManager::getLogger()->info("⚙️ 初始化告警规则引擎...");
        alarm_rule_engine_ = std::make_shared<AlarmRuleEngine>(
            alarm_rule_storage_, resource_storage_, alarm_manager_);
        
        // 5. 设置告警事件监控
        alarm_monitor_ = std::make_shared<AlarmEventMonitor>();
        alarm_rule_engine_->setAlarmEventCallback([this](const AlarmEvent& event) {
            if (alarm_monitor_) {
                alarm_monitor_->onAlarmEvent(event);
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
        
        return true;
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = "服务初始化异常: " + std::string(e.what());
        return false;
    }
}

// 节点数据生成线程函数
static void nodeDataGeneratorThread(const std::string& node_ip, ResourceStorage& storage, 
                                  int node_id, const AlarmSystemConfig& config, 
                                  std::atomic<bool>& running) {
    LogManager::getLogger()->info("🚀 启动节点 {} 数据生成线程", node_ip);
    
    ResourceDataGenerator generator;
    int cycle_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    while (running) {
        try {
            // 基于时间的周期性高使用率生成策略
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                current_time - start_time).count();
            
            // 每5分钟(300秒)为一个大周期
            int cycle_position = elapsed_seconds % 300;
            
            bool high_usage = false;
            if (node_id == 1) {
                // 节点1：在每个5分钟周期的第30-90秒产生高使用率(持续60秒)
                high_usage = (cycle_position >= 30 && cycle_position < 90);
            } else if (node_id == 2) {
                // 节点2：在每个5分钟周期的第120-180秒产生高使用率(持续60秒)
                high_usage = (cycle_position >= 120 && cycle_position < 180);
            }
            
            // 记录周期状态变化（每个节点独立维护状态）
            static std::map<int, bool> last_high_usage_states;
            bool last_state = last_high_usage_states[node_id];
            
            if (high_usage != last_state) {
                if (high_usage) {
                    LogManager::getLogger()->info("🔥 [{}] 开始高使用率模式 (周期位置: {}s)", node_ip, cycle_position);
                } else {
                    LogManager::getLogger()->info("📉 [{}] 结束高使用率模式，回到正常模式", node_ip);
                }
                last_high_usage_states[node_id] = high_usage;
            }
            
            auto data = generator.generateResourceData(high_usage);
            
            if (storage.insertResourceData(node_ip, data)) {
                if (high_usage) {
                    double cpu_usage = data["cpu"]["usage_percent"];
                    double memory_usage = data["memory"]["usage_percent"];
                    double disk_usage = data["disk"][0]["usage_percent"];
                    LogManager::getLogger()->info("🔥 [{}] 高使用率数据 - CPU:{:.1f}%, MEM:{:.1f}%, DISK:{:.1f}% (运行时间: {}s)", 
                                 node_ip, cpu_usage, memory_usage, disk_usage, elapsed_seconds);
                } else if (cycle_count % 10 == 0) {
                    // 每20秒输出一次正常数据日志 (cycle_count每2秒+1，所以10次=20秒)
                    LogManager::getLogger()->info("📊 [{}] 正常数据 (运行时间: {}s, 周期位置: {}s)", 
                                 node_ip, elapsed_seconds, cycle_position);
                }
            } else {
                LogManager::getLogger()->error("❌ [{}] 数据插入失败", node_ip);
            }
            
            cycle_count++;
            std::this_thread::sleep_for(config.data_generation_interval);
            
        } catch (const std::exception& e) {
            LogManager::getLogger()->error("❌ [{}] 数据生成异常: {}", node_ip, e.what());
        }
    }
    
    LogManager::getLogger()->info("🛑 节点 {} 数据生成线程已停止", node_ip);
}

bool AlarmSystem::startDataGeneration() {
    if (config_.simulation_nodes.empty()) {
        LogManager::getLogger()->info("📊 未配置模拟节点，跳过数据生成");
        return true;
    }
    
    LogManager::getLogger()->info("📊 启动模拟数据生成线程...");
    
    for (size_t i = 0; i < config_.simulation_nodes.size(); ++i) {
        const auto& node_ip = config_.simulation_nodes[i];
        int node_id = static_cast<int>(i + 1);
        
        data_threads_.emplace_back(nodeDataGeneratorThread, 
                                 node_ip, std::ref(*resource_storage_), 
                                 node_id, config_, std::ref(running_));
    }
    
    LogManager::getLogger()->info("✅ 启动了 {} 个数据生成线程", data_threads_.size());
    return true;
}

void AlarmSystem::stopDataGeneration() {
    LogManager::getLogger()->info("🛑 停止数据生成线程...");
    
    // 等待数据生成线程结束
    for (auto& thread : data_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    data_threads_.clear();
    
    LogManager::getLogger()->info("✅ 所有数据生成线程已停止");
}

void AlarmSystem::monitoringLoop() {
    auto start_time = std::chrono::steady_clock::now();
    int stats_counter = 0;
    int stats_interval_count = config_.stats_interval.count() / 5; // 每5秒检查一次
    
    // 持续运行直到收到停止信号
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(5)); // 每5秒检查一次
        
        // 每配置的时间间隔输出一次详细统计
        stats_counter++;
        if (stats_counter >= stats_interval_count) {
            auto current_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                current_time - start_time).count();
            
            LogManager::getLogger()->info("\n⏱️  系统运行时间: {} 秒", duration);
            if (alarm_monitor_) {
                alarm_monitor_->printStatistics();
            }
            
            // 查询告警管理器统计
            if (alarm_manager_) {
                LogManager::getLogger()->info("  - 活跃告警: {}", alarm_manager_->getActiveAlarmCount());
                LogManager::getLogger()->info("  - 总告警数: {}", alarm_manager_->getTotalAlarmCount());
            }
            
            // 显示当前告警实例
            if (alarm_rule_engine_) {
                auto instances = alarm_rule_engine_->getCurrentAlarmInstances();
                LogManager::getLogger()->info("  - 当前告警实例数: {}", instances.size());
                for (const auto& instance : instances) {
                    LogManager::getLogger()->info("    * {} (状态: {}, 值: {})", 
                               instance.fingerprint, static_cast<int>(instance.state), instance.value);
                }
            }
            
            stats_counter = 0; // 重置计数器
        }
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
int runAlarmSystem(const AlarmSystemConfig& config) {
    try {
        AlarmSystem system(config);
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