#include "multicast_sender.h"
#include "http_server.h"
#include "log_manager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>
#include <vector>
#include <string>
#include <map>
#include "resource_storage.h"
#include "alarm_rule_storage.h"
#include "alarm_rule_engine.h"
#include "alarm_manager.h"
#include "json.hpp"
#include <signal.h>

// 全局控制变量
std::atomic<bool> g_running(true);

// 信号处理函数
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        LogManager::getLogger()->info("🛑 接收到停止信号，正在优雅关闭系统...");
        g_running = false;
    }
}

// 模拟资源数据生成器
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

// 模拟节点数据生成线程
void nodeDataGeneratorThread(const std::string& node_ip, ResourceStorage& storage, int node_id) {
    LogManager::getLogger()->info("🚀 启动节点 {} 数据生成线程", node_ip);
    
    ResourceDataGenerator generator;
    int cycle_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    while (g_running) {
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
            std::this_thread::sleep_for(std::chrono::seconds(2)); // 每2秒生成一次数据
            
        } catch (const std::exception& e) {
            LogManager::getLogger()->error("❌ [{}] 数据生成异常: {}", node_ip, e.what());
        }
    }
    
    LogManager::getLogger()->info("🛑 节点 {} 数据生成线程已停止", node_ip);
}



// 告警事件监控器
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

int main(int argc, char* argv[]) {
    // Initialize the logger as the first step
    LogManager::init("log_config.json");

    bool start_simulation = true;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--no-simulation") {
            start_simulation = false;
            break;
        }
    }

    LogManager::getLogger()->info("🎯 Alarm system starting up...");

    // 设置信号处理器
    signal(SIGINT, signalHandler);  // Ctrl+C
    signal(SIGTERM, signalHandler); // 终止信号
    LogManager::getLogger()->info("📡 信号处理器已设置，使用 Ctrl+C 可以优雅停止程序");

    // 初始化组播发送器
    MulticastSender multicast_sender("239.192.168.80", 3980);
    multicast_sender.start();
    
    try {
        // 1. 初始化资源存储
        LogManager::getLogger()->info("📦 Initializing resource storage...");
        auto storage_ptr = std::make_shared<ResourceStorage>("127.0.0.1", "test", "HZ715Net");
        
        if (!storage_ptr->connect()) {
            LogManager::getLogger()->critical("❌ Failed to connect to TDengine");
            return 1;
        }
        
        if (!storage_ptr->createDatabase("resource")) {
            LogManager::getLogger()->critical("❌ Failed to create resource database");
            return 1;
        }
        
        if (!storage_ptr->createResourceTable()) {
            LogManager::getLogger()->critical("❌ Failed to create resource tables");
            return 1;
        }
        
        LogManager::getLogger()->info("✅ Resource storage initialized successfully.");

        // 2. 初始化告警规则存储
        LogManager::getLogger()->info("📋 Initializing alarm rule storage...");
        auto alarm_storage_ptr = std::make_shared<AlarmRuleStorage>("127.0.0.1", 3306, "test", "HZ715Net", "alarm");
        
        if (!alarm_storage_ptr->connect()) {
            LogManager::getLogger()->critical("❌ Failed to connect to MySQL for alarm rules");
            return 1;
        }
        
        if (!alarm_storage_ptr->createDatabase()) {
            LogManager::getLogger()->critical("❌ Failed to create alarm database");
            return 1;
        }
        
        if (!alarm_storage_ptr->createTable()) {
            LogManager::getLogger()->critical("❌ Failed to create alarm rule table");
            return 1;
        }
        
        LogManager::getLogger()->info("✅ Alarm rule storage initialized successfully.");
        
        // 3. 启动HTTP服务器
        LogManager::getLogger()->info("🌐 Starting HTTP server...");
        HttpServer http_server(storage_ptr, alarm_storage_ptr);
        if (!http_server.start()) {
            LogManager::getLogger()->critical("❌ Failed to start HTTP server");
            return 1;
        }
        LogManager::getLogger()->info("✅ HTTP server started successfully on port 8080");
        
        // 4. 初始化告警管理器
        auto alarm_manager_ptr = std::make_shared<AlarmManager>("127.0.0.1", 3306, "test", "HZ715Net", "alarm");
        
        if (!alarm_manager_ptr->connect()) {
            LogManager::getLogger()->error("{}");
            return 1;
        }
        
        // 选择数据库
        if (mysql_select_db(alarm_manager_ptr->getConnection(), "alarm") != 0) {
            LogManager::getLogger()->error("{}");
            return 1;
        }
        
        if (!alarm_manager_ptr->createEventTable()) {
            LogManager::getLogger()->error("{}");
            return 1;
        }
        
        // 5. 初始化告警规则引擎
        AlarmRuleEngine engine(alarm_storage_ptr, storage_ptr, alarm_manager_ptr);
        
        // 6. 设置告警事件监控
        AlarmEventMonitor monitor;
        engine.setAlarmEventCallback([&monitor](const AlarmEvent& event) {
            monitor.onAlarmEvent(event);
        });
        
        // 设置较短的评估间隔
        engine.setEvaluationInterval(std::chrono::seconds(3));
        
        // 7. 启动告警引擎
        if (!engine.start()) {
            LogManager::getLogger()->error("{}");
            return 1;
        }
        
        // 8. 启动模拟数据生成线程
        std::vector<std::thread> data_threads;
        if (start_simulation) {
            // 启动两个节点的数据生成线程
            data_threads.emplace_back(nodeDataGeneratorThread, "192.168.1.100", std::ref(*storage_ptr), 1);
            data_threads.emplace_back(nodeDataGeneratorThread, "192.168.1.101", std::ref(*storage_ptr), 2);
            
        } else {
            
        }
        
        // 9. 监控和报告
        LogManager::getLogger()->info("🔄 系统正在运行中，每60秒输出一次统计信息...");
        LogManager::getLogger()->info("💡 按 Ctrl+C 可以优雅停止程序");
        
        auto start_time = std::chrono::system_clock::now();
        int stats_counter = 0;
        
        // 持续运行直到收到停止信号
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(5)); // 每5秒检查一次
            
            // 每60秒输出一次详细统计
            stats_counter++;
            if (stats_counter >= 12) { // 5秒 * 12 = 60秒
                auto current_time = std::chrono::system_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                    current_time - start_time).count();
                
                LogManager::getLogger()->info("\n⏱️  系统运行时间: {} 秒", duration);
                monitor.printStatistics();
                
                // 查询告警管理器统计
                LogManager::getLogger()->info("  - 活跃告警: {}", alarm_manager_ptr->getActiveAlarmCount());
                LogManager::getLogger()->info("  - 总告警数: {}", alarm_manager_ptr->getTotalAlarmCount());
                
                // 显示当前告警实例
                auto instances = engine.getCurrentAlarmInstances();
                LogManager::getLogger()->info("  - 当前告警实例数: {}", instances.size());
                for (const auto& instance : instances) {
                    LogManager::getLogger()->info("    * {} (状态: {}, 值: {})", 
                               instance.fingerprint, static_cast<int>(instance.state), instance.value);
                }
                
                stats_counter = 0; // 重置计数器
            }
        }
        
        // 10. 停止系统

        // 停止组播发送器
        multicast_sender.stop();
        http_server.stop();
        
        // 等待数据生成线程结束
        for (auto& thread : data_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        // 停止告警引擎
        engine.stop();
        
        // 11. 最终统计报告
        LogManager::getLogger()->info("🏁 系统已优雅停止，最终统计报告：");
        monitor.printStatistics();
        
        LogManager::getLogger()->info("  - 活跃告警: {}", alarm_manager_ptr->getActiveAlarmCount());
        LogManager::getLogger()->info("  - 总告警数: {}", alarm_manager_ptr->getTotalAlarmCount());
        
        // 显示最近的告警事件
        auto recent_events = alarm_manager_ptr->getRecentAlarmEvents(10);
        
        for (const auto& event : recent_events) {
            LogManager::getLogger()->info("    * {} [{}] {}", event.fingerprint, event.status, event.created_at);
        }
        
        // 如果没有自动生成告警，提示用户
        if (monitor.getFiringCount() == 0) {
            // 空的if块，可能在未来添加提示逻辑
        }
        
    } catch (const std::exception& e) {
        LogManager::getLogger()->critical("❌ 系统异常: {}", e.what());
        g_running = false;
        spdlog::shutdown(); // Ensure logs are flushed in case of exception
        return 1;
    }
    
    LogManager::getLogger()->info("✅ 告警系统已完全退出");
    spdlog::shutdown(); // Ensure logs are flushed before exiting
    return 0;
}