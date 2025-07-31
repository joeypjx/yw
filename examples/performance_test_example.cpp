#include "resource/alarm_rule_storage.h"
#include "log_manager.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

// 性能测试函数
void performanceTest(const std::string& test_name, AlarmRuleStorage& storage, int iterations) {
    std::cout << "\n=== " << test_name << " ===" << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        // 模拟数据库操作
        auto rules = storage.getAllAlarmRules();
        
        // 模拟一些处理时间
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "执行 " << iterations << " 次操作耗时: " << duration.count() << " 毫秒" << std::endl;
    std::cout << "平均每次操作: " << (duration.count() / (double)iterations) << " 毫秒" << std::endl;
}

int main() {
    // 初始化日志
    LogManager::init("logs/performance_test.log", "info");
    
    std::cout << "MySQL自动重连性能测试" << std::endl;
    std::cout << "========================" << std::endl;
    
    // 测试1：默认配置（频繁检查）
    {
        AlarmRuleStorage storage1("localhost", 3306, "root", "password", "alarm_system");
        storage1.enableAutoReconnect(true);
        storage1.setReconnectInterval(1);
        storage1.setConnectionCheckInterval(1000);  // 1秒检查一次
        storage1.enableExponentialBackoff(false);
        
        if (storage1.connect()) {
            storage1.createDatabase();
            storage1.createTable();
            performanceTest("默认配置（频繁检查）", storage1, 100);
        }
    }
    
    // 测试2：优化配置（减少检查频率）
    {
        AlarmRuleStorage storage2("localhost", 3306, "root", "password", "alarm_system");
        storage2.enableAutoReconnect(true);
        storage2.setReconnectInterval(3);
        storage2.setConnectionCheckInterval(10000);  // 10秒检查一次
        storage2.enableExponentialBackoff(true);
        storage2.setMaxBackoffSeconds(30);
        
        if (storage2.connect()) {
            storage2.createDatabase();
            storage2.createTable();
            performanceTest("优化配置（减少检查频率）", storage2, 100);
        }
    }
    
    // 测试3：高性能配置（最小检查）
    {
        AlarmRuleStorage storage3("localhost", 3306, "root", "password", "alarm_system");
        storage3.enableAutoReconnect(true);
        storage3.setReconnectInterval(5);
        storage3.setConnectionCheckInterval(30000);  // 30秒检查一次
        storage3.enableExponentialBackoff(true);
        storage3.setMaxBackoffSeconds(60);
        
        if (storage3.connect()) {
            storage3.createDatabase();
            storage3.createTable();
            performanceTest("高性能配置（最小检查）", storage3, 100);
        }
    }
    
    // 测试4：无自动重连（基准测试）
    {
        AlarmRuleStorage storage4("localhost", 3306, "root", "password", "alarm_system");
        storage4.enableAutoReconnect(false);
        
        if (storage4.connect()) {
            storage4.createDatabase();
            storage4.createTable();
            performanceTest("无自动重连（基准）", storage4, 100);
        }
    }
    
    std::cout << "\n性能测试完成！" << std::endl;
    std::cout << "\n性能优化建议：" << std::endl;
    std::cout << "1. 根据网络环境调整连接检查间隔" << std::endl;
    std::cout << "2. 启用指数退避减少重连频率" << std::endl;
    std::cout << "3. 在稳定网络环境下可以增加检查间隔" << std::endl;
    std::cout << "4. 在高负载环境下建议使用高性能配置" << std::endl;
    
    return 0;
} 