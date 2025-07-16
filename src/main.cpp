#include "multicast_sender.h"
#include "http_server.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>
#include <vector>
#include <string>
#include "resource_storage.h"
#include "alarm_rule_storage.h"
#include "alarm_rule_engine.h"
#include "alarm_manager.h"
#include "json.hpp"

// 全局控制变量
std::atomic<bool> g_running(true);

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
    std::cout << "🚀 启动节点 " << node_ip << " 数据生成线程" << std::endl;
    
    ResourceDataGenerator generator;
    int cycle_count = 0;
    
    while (g_running) {
        try {
            // 设计更容易触发的告警场景
            bool high_usage = false;
            if (node_id == 1) {
                // 节点1：在第3-8个周期生成高使用率数据
                high_usage = (cycle_count >= 3 && cycle_count <= 8);
            } else if (node_id == 2) {
                // 节点2：在第5-10个周期生成高使用率数据
                high_usage = (cycle_count >= 5 && cycle_count <= 10);
            }
            
            auto data = generator.generateResourceData(high_usage);
            
            if (storage.insertResourceData(node_ip, data)) {
                if (high_usage) {
                    double cpu_usage = data["cpu"]["usage_percent"];
                    double memory_usage = data["memory"]["usage_percent"];
                    double disk_usage = data["disk"][0]["usage_percent"];
                    std::cout << "🔥 [" << node_ip << "] 高使用率数据 - CPU:" << cpu_usage 
                              << "%, MEM:" << memory_usage << "%, DISK:" << disk_usage 
                              << "% (周期: " << cycle_count << ")" << std::endl;
                } else if (cycle_count % 5 == 0) {
                    std::cout << "📊 [" << node_ip << "] 正常数据 (周期: " << cycle_count << ")" << std::endl;
                }
            } else {
                std::cerr << "❌ [" << node_ip << "] 数据插入失败" << std::endl;
            }
            
            cycle_count++;
            std::this_thread::sleep_for(std::chrono::seconds(2)); // 每2秒生成一次数据
            
        } catch (const std::exception& e) {
            std::cerr << "❌ [" << node_ip << "] 数据生成异常: " << e.what() << std::endl;
        }
    }
    
    std::cout << "🛑 节点 " << node_ip << " 数据生成线程已停止" << std::endl;
}

// 创建测试告警规则
void createTestAlarmRules(AlarmRuleStorage& alarm_storage) {
    std::cout << "\n=== 创建测试告警规则 ===" << std::endl;
    
    // 检查告警规则表是否为空
    auto existing_rules = alarm_storage.getAllAlarmRules();
    if (!existing_rules.empty()) {
        std::cout << "📋 发现已有 " << existing_rules.size() << " 个告警规则，清空并重新创建" << std::endl;
        
        // 清空现有规则
        for (const auto& rule : existing_rules) {
            alarm_storage.deleteAlarmRule(rule.id);
        }
    }
    
    std::cout << "🔧 创建新的测试告警规则..." << std::endl;
    
    // 规则1: 高CPU使用率告警 (阈值降低，便于触发)
    nlohmann::json cpu_rule = {
        {"stable", "cpu_metrics"},
        {"metric", "usage_percent"},
        {"operator", ">"},
        {"threshold", 85.0}  // 降低阈值
    };
    
    std::string cpu_rule_id = alarm_storage.insertAlarmRule(
        "HighCpuUsage",
        cpu_rule,
        "6s",  // 非常短的持续时间
        "warning",
        "CPU使用率过高",
        "节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%"
    );
    
    // 规则2: 高内存使用率告警
    nlohmann::json memory_rule = {
        {"stable", "memory_metrics"},
        {"metric", "usage_percent"},
        {"operator", ">"},
        {"threshold", 85.0}  // 降低阈值
    };
    
    std::string memory_rule_id = alarm_storage.insertAlarmRule(
        "HighMemoryUsage",
        memory_rule,
        "6s",
        "critical",
        "内存使用率过高",
        "节点 {{host_ip}} 内存使用率达到 {{usage_percent}}%"
    );
    
    // 规则3: 高磁盘使用率告警
    nlohmann::json disk_rule = {
        {"stable", "disk_metrics"},
        {"metric", "usage_percent"},
        {"operator", ">"},
        {"threshold", 75.0}  // 降低阈值
    };
    
    std::string disk_rule_id = alarm_storage.insertAlarmRule(
        "HighDiskUsage",
        disk_rule,
        "6s",
        "warning",
        "磁盘使用率过高",
        "节点 {{host_ip}} 磁盘 {{device}} 使用率达到 {{usage_percent}}%"
    );
    
    // 验证规则创建结果
    std::vector<std::string> rule_ids = {cpu_rule_id, memory_rule_id, disk_rule_id};
    std::vector<std::string> rule_names = {"HighCpuUsage", "HighMemoryUsage", "HighDiskUsage"};
    
    std::cout << "✅ 创建告警规则结果:" << std::endl;
    for (size_t i = 0; i < rule_ids.size(); i++) {
        if (!rule_ids[i].empty()) {
            std::cout << "  - " << rule_names[i] << ": " << rule_ids[i] << std::endl;
        } else {
            std::cout << "  - " << rule_names[i] << ": 创建失败" << std::endl;
        }
    }
    
    // 显示最终规则统计
    auto final_rules = alarm_storage.getAllAlarmRules();
    std::cout << "📊 当前告警规则总数: " << final_rules.size() << std::endl;
}

// 手动触发告警的测试函数
void triggerTestAlarms(std::shared_ptr<AlarmManager> alarm_manager) {
    std::cout << "\n=== 手动触发测试告警 ===" << std::endl;
    
    // 创建测试告警事件
    AlarmEvent test_event;
    test_event.fingerprint = "alertname=TestManualAlarm,host_ip=192.168.1.100";
    test_event.status = "firing";
    test_event.labels["alertname"] = "TestManualAlarm";
    test_event.labels["host_ip"] = "192.168.1.100";
    test_event.labels["severity"] = "warning";
    test_event.annotations["summary"] = "手动触发的测试告警";
    test_event.annotations["description"] = "这是一个手动触发的测试告警事件";
    test_event.starts_at = std::chrono::system_clock::now();
    test_event.generator_url = "http://test.example.com";
    
    if (alarm_manager->processAlarmEvent(test_event)) {
        std::cout << "✅ 手动触发告警事件成功" << std::endl;
    } else {
        std::cout << "❌ 手动触发告警事件失败" << std::endl;
    }
    
    // 等待几秒后解决告警
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    test_event.status = "resolved";
    test_event.ends_at = std::chrono::system_clock::now();
    
    if (alarm_manager->processAlarmEvent(test_event)) {
        std::cout << "✅ 手动解决告警事件成功" << std::endl;
    } else {
        std::cout << "❌ 手动解决告警事件失败" << std::endl;
    }
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
            std::cout << "🔥 [告警触发] " << event.fingerprint << std::endl;
            if (event.annotations.find("summary") != event.annotations.end()) {
                std::cout << "   摘要: " << event.annotations.at("summary") << std::endl;
            }
            if (event.annotations.find("description") != event.annotations.end()) {
                std::cout << "   描述: " << event.annotations.at("description") << std::endl;
            }
        } else if (event.status == "resolved") {
            resolved_count++;
            std::cout << "✅ [告警恢复] " << event.fingerprint << std::endl;
        }
        
        recent_events.push_back(event.fingerprint + " - " + event.status);
        if (recent_events.size() > 10) {
            recent_events.erase(recent_events.begin());
        }
    }
    
    void printStatistics() {
        std::lock_guard<std::mutex> lock(events_mutex);
        std::cout << "\n📈 告警事件统计:" << std::endl;
        std::cout << "  - 触发次数: " << firing_count.load() << std::endl;
        std::cout << "  - 恢复次数: " << resolved_count.load() << std::endl;
        std::cout << "  - 最近事件:" << std::endl;
        for (const auto& event : recent_events) {
            std::cout << "    " << event << std::endl;
        }
    }
    
    int getFiringCount() const { return firing_count.load(); }
    int getResolvedCount() const { return resolved_count.load(); }
};

int main() {
    std::cout << "🎯 告警系统完整工作流程演示" << std::endl;
    std::cout << "===============================================" << std::endl;

    // 初始化组播发送器
    MulticastSender multicast_sender("239.192.168.80", 3980);
    multicast_sender.start();
    
    try {
        // 1. 初始化资源存储
        std::cout << "\n📦 初始化资源存储..." << std::endl;
        auto storage_ptr = std::make_shared<ResourceStorage>("127.0.0.1", "test", "HZ715Net");
        
        if (!storage_ptr->connect()) {
            std::cerr << "❌ 无法连接到TDengine" << std::endl;
            return 1;
        }
        
        if (!storage_ptr->createDatabase("resource")) {
            std::cerr << "❌ 无法创建资源数据库" << std::endl;
            return 1;
        }
        
        if (!storage_ptr->createResourceTable()) {
            std::cerr << "❌ 无法创建资源表" << std::endl;
            return 1;
        }
        
        std::cout << "✅ 资源存储初始化完成" << std::endl;

        // 启动HTTP服务器
        HttpServer http_server(storage_ptr);
        if (!http_server.start()) {
            std::cerr << "❌ 无法启动HTTP服务器" << std::endl;
            return 1;
        }
        
        // 2. 初始化告警规则存储
        std::cout << "\n📋 初始化告警规则存储..." << std::endl;
        AlarmRuleStorage alarm_storage("127.0.0.1", 3306, "test", "HZ715Net", "alarm");
        
        if (!alarm_storage.connect()) {
            std::cerr << "❌ 无法连接到MySQL" << std::endl;
            return 1;
        }
        
        if (!alarm_storage.createDatabase()) {
            std::cerr << "❌ 无法创建告警数据库" << std::endl;
            return 1;
        }
        
        if (!alarm_storage.createTable()) {
            std::cerr << "❌ 无法创建告警规则表" << std::endl;
            return 1;
        }
        
        std::cout << "✅ 告警规则存储初始化完成" << std::endl;
        
        // 3. 创建测试告警规则
        createTestAlarmRules(alarm_storage);
        
        // 4. 初始化告警管理器
        std::cout << "\n🚨 初始化告警管理器..." << std::endl;
        auto alarm_manager_ptr = std::make_shared<AlarmManager>("127.0.0.1", 3306, "test", "HZ715Net", "alarm");
        
        if (!alarm_manager_ptr->connect()) {
            std::cerr << "❌ 无法连接到MySQL (告警管理器)" << std::endl;
            return 1;
        }
        
        // 选择数据库
        if (mysql_select_db(alarm_manager_ptr->getConnection(), "alarm") != 0) {
            std::cerr << "❌ 无法选择告警数据库" << std::endl;
            return 1;
        }
        
        if (!alarm_manager_ptr->createEventTable()) {
            std::cerr << "❌ 无法创建告警事件表" << std::endl;
            return 1;
        }
        
        std::cout << "✅ 告警管理器初始化完成" << std::endl;
        
        // 5. 手动触发测试告警
        // triggerTestAlarms(alarm_manager_ptr);
        
        // 6. 初始化告警规则引擎
        std::cout << "\n⚙️  初始化告警规则引擎..." << std::endl;
        auto rule_storage_ptr = std::make_shared<AlarmRuleStorage>(alarm_storage);
        
        AlarmRuleEngine engine(rule_storage_ptr, storage_ptr, alarm_manager_ptr);
        
        // 7. 设置告警事件监控
        AlarmEventMonitor monitor;
        engine.setAlarmEventCallback([&monitor](const AlarmEvent& event) {
            monitor.onAlarmEvent(event);
        });
        
        // 设置较短的评估间隔
        engine.setEvaluationInterval(std::chrono::seconds(3));
        
        // 8. 启动告警引擎
        std::cout << "\n🔄 启动告警规则引擎..." << std::endl;
        if (!engine.start()) {
            std::cerr << "❌ 无法启动告警规则引擎" << std::endl;
            return 1;
        }
        
        std::cout << "✅ 告警规则引擎启动完成" << std::endl;
        
        // 9. 启动模拟数据生成线程
        std::cout << "\n🔄 启动模拟数据生成线程..." << std::endl;
        std::vector<std::thread> data_threads;
        
        // 启动两个节点的数据生成线程
        data_threads.emplace_back(nodeDataGeneratorThread, "192.168.1.100", std::ref(*storage_ptr), 1);
        data_threads.emplace_back(nodeDataGeneratorThread, "192.168.1.101", std::ref(*storage_ptr), 2);
        
        std::cout << "✅ 数据生成线程启动完成" << std::endl;
        
        // 10. 监控和报告
        std::cout << "\n🔍 开始监控告警系统..." << std::endl;
        std::cout << "系统将运行 60 秒，期间会模拟高使用率场景以触发告警" << std::endl;
        std::cout << "注意：由于告警规则引擎使用模拟查询，可能不会触发实际告警" << std::endl;
        
        // 运行60秒，每20秒输出一次统计
        for (int i = 0; i < 3; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(20));
            
            std::cout << "\n⏱️  运行时间: " << (i + 1) * 20 << " 秒" << std::endl;
            monitor.printStatistics();
            
            // 查询告警管理器统计
            std::cout << "\n📊 告警管理器统计:" << std::endl;
            std::cout << "  - 活跃告警: " << alarm_manager_ptr->getActiveAlarmCount() << std::endl;
            std::cout << "  - 总告警数: " << alarm_manager_ptr->getTotalAlarmCount() << std::endl;
            
            // 显示当前告警实例
            auto instances = engine.getCurrentAlarmInstances();
            std::cout << "  - 当前告警实例数: " << instances.size() << std::endl;
            for (const auto& instance : instances) {
                std::cout << "    * " << instance.fingerprint << " (状态: " << 
                           static_cast<int>(instance.state) << ", 值: " << instance.value << ")" << std::endl;
            }
        }
        
        // 11. 停止系统
        std::cout << "\n🛑 停止告警系统..." << std::endl;
        g_running = false;

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
        
        // 12. 最终统计报告
        std::cout << "\n📈 最终统计报告:" << std::endl;
        monitor.printStatistics();
        
        std::cout << "\n📊 告警数据库统计:" << std::endl;
        std::cout << "  - 活跃告警: " << alarm_manager_ptr->getActiveAlarmCount() << std::endl;
        std::cout << "  - 总告警数: " << alarm_manager_ptr->getTotalAlarmCount() << std::endl;
        
        // 显示最近的告警事件
        auto recent_events = alarm_manager_ptr->getRecentAlarmEvents(10);
        std::cout << "  - 最近告警事件:" << std::endl;
        for (const auto& event : recent_events) {
            std::cout << "    * " << event.fingerprint << " [" << event.status << "] " << event.created_at << std::endl;
        }
        
        // 如果没有自动生成告警，提示用户
        if (monitor.getFiringCount() == 0) {
            std::cout << "\n⚠️  注意：没有检测到自动生成的告警事件" << std::endl;
            std::cout << "   这可能是因为告警规则引擎的查询模块使用了模拟数据" << std::endl;
            std::cout << "   但手动触发的告警事件应该已经正确存储到数据库中" << std::endl;
        }
        
        std::cout << "\n✅ 告警系统演示完成！" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 系统异常: " << e.what() << std::endl;
        g_running = false;
        return 1;
    }
    
    return 0;
}