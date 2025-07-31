#include "resource/alarm_rule_storage.h"
#include "log_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // 初始化日志
    LogManager::init("logs/auto_reconnect_example.log", "debug");
    
    // 创建数据库存储对象
    AlarmRuleStorage storage("localhost", 3306, "root", "password", "alarm_system");
    
    // 配置自动重连参数
    storage.enableAutoReconnect(true);           // 启用自动重连
    storage.setReconnectInterval(3);             // 设置重连间隔为3秒
    storage.setMaxReconnectAttempts(5);          // 设置最大重连尝试次数为5次
    
    // 配置性能优化参数
    storage.setConnectionCheckInterval(10000);    // 10秒检查一次连接状态
    storage.enableExponentialBackoff(true);      // 启用指数退避
    storage.setMaxBackoffSeconds(30);            // 最大退避30秒
    
    std::cout << "自动重连配置:" << std::endl;
    std::cout << "- 启用状态: " << (storage.isAutoReconnectEnabled() ? "是" : "否") << std::endl;
    std::cout << "- 重连间隔: " << storage.getReconnectAttempts() << " 秒" << std::endl;
    std::cout << "- 当前尝试次数: " << storage.getReconnectAttempts() << std::endl;
    std::cout << "- 连接检查间隔: " << storage.getConnectionCheckInterval() << " 毫秒" << std::endl;
    std::cout << "- 指数退避: " << (storage.isExponentialBackoffEnabled() ? "启用" : "禁用") << std::endl;
    
    // 连接到数据库
    if (!storage.connect()) {
        std::cerr << "初始连接失败" << std::endl;
        return 1;
    }
    
    // 创建数据库和表
    if (!storage.createDatabase()) {
        std::cerr << "创建数据库失败" << std::endl;
        return 1;
    }
    
    if (!storage.createTable()) {
        std::cerr << "创建表失败" << std::endl;
        return 1;
    }
    
    std::cout << "数据库连接成功，开始测试自动重连机制..." << std::endl;
    
    // 模拟数据库操作
    for (int i = 0; i < 10; ++i) {
        try {
            // 尝试插入一条测试规则
            nlohmann::json expression = {
                {"metric", "cpu_usage"},
                {"operator", ">"},
                {"threshold", 80}
            };
            
            std::string id = storage.insertAlarmRule(
                "测试规则_" + std::to_string(i),
                expression,
                "5m",
                "warning",
                "CPU使用率过高",
                "CPU使用率超过80%",
                "硬件状态"
            );
            
            if (!id.empty()) {
                std::cout << "成功插入规则: " << id << std::endl;
            } else {
                std::cout << "插入规则失败" << std::endl;
            }
            
            // 查询所有规则
            auto rules = storage.getAllAlarmRules();
            std::cout << "当前规则数量: " << rules.size() << std::endl;
            
            // 检查重连状态
            std::cout << "重连尝试次数: " << storage.getReconnectAttempts() << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "操作异常: " << e.what() << std::endl;
        }
        
        // 等待一段时间
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    std::cout << "测试完成" << std::endl;
    
    return 0;
} 