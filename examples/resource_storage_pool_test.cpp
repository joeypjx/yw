#include "resource_storage.h"
#include "log_manager.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

void testBasicFunctionality() {
    std::cout << "\n=== ResourceStorage 基本功能测试 ===" << std::endl;
    
    try {
        std::cout << "1. 测试兼容性构造函数..." << std::endl;
        ResourceStorage storage("localhost", "root", "taosdata");
        
        std::cout << "2. 测试初始化..." << std::endl;
        if (!storage.initialize()) {
            std::cerr << "❌ 初始化失败（可能是因为没有可用的TDengine服务器）" << std::endl;
            return;
        }
        std::cout << "✅ 初始化成功" << std::endl;
        
        std::cout << "\n3. 测试连接池统计..." << std::endl;
        auto stats = storage.getConnectionPoolStats();
        std::cout << "📊 连接池统计信息:" << std::endl;
        std::cout << "   - 总连接数: " << stats.total_connections << std::endl;
        std::cout << "   - 活跃连接数: " << stats.active_connections << std::endl;
        std::cout << "   - 空闲连接数: " << stats.idle_connections << std::endl;
        std::cout << "   - 等待请求数: " << stats.pending_requests << std::endl;
        std::cout << "   - 已创建连接数: " << stats.created_connections << std::endl;
        std::cout << "   - 已销毁连接数: " << stats.destroyed_connections << std::endl;
        std::cout << "   - 平均等待时间: " << stats.average_wait_time << "ms" << std::endl;
        
        std::cout << "\n4. 测试数据库操作..." << std::endl;
        
        // 测试创建数据库
        if (storage.createDatabase("test_resource_db")) {
            std::cout << "✅ 数据库创建成功" << std::endl;
        } else {
            std::cout << "❌ 数据库创建失败" << std::endl;
        }
        
        // 测试创建资源表
        if (storage.createResourceTable()) {
            std::cout << "✅ 资源表创建成功" << std::endl;
        } else {
            std::cout << "❌ 资源表创建失败" << std::endl;
        }
        
        std::cout << "\n5. 测试查询操作..." << std::endl;
        try {
            // 执行一个简单的查询
            auto results = storage.executeQuerySQL("SELECT SERVER_VERSION()");
            if (!results.empty()) {
                std::cout << "✅ 查询执行成功，返回 " << results.size() << " 条结果" << std::endl;
            } else {
                std::cout << "✅ 查询执行成功，但无结果返回（可能是DDL语句）" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "❌ 查询执行失败: " << e.what() << std::endl;
        }
        
        std::cout << "\n6. 最终连接池统计..." << std::endl;
        stats = storage.getConnectionPoolStats();
        std::cout << "📊 最终统计:" << std::endl;
        std::cout << "   - 总连接数: " << stats.total_connections << std::endl;
        std::cout << "   - 活跃连接数: " << stats.active_connections << std::endl;
        std::cout << "   - 空闲连接数: " << stats.idle_connections << std::endl;
        
        std::cout << "\n7. 测试关闭..." << std::endl;
        storage.shutdown();
        std::cout << "✅ ResourceStorage 关闭成功" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 测试过程中发生异常: " << e.what() << std::endl;
    }
}

void testPoolConfigConstructor() {
    std::cout << "\n=== 连接池配置构造函数测试 ===" << std::endl;
    
    try {
        std::cout << "1. 创建自定义连接池配置..." << std::endl;
        TDenginePoolConfig config;
        config.host = "localhost";
        config.port = 6030;
        config.user = "root";
        config.password = "taosdata";
        config.database = "test_pool_db";
        config.min_connections = 2;
        config.max_connections = 6;
        config.initial_connections = 3;
        config.connection_timeout = 30;
        config.idle_timeout = 600;
        config.max_lifetime = 3600;
        config.acquire_timeout = 10;
        config.health_check_interval = 60;
        config.health_check_query = "SELECT SERVER_VERSION()";
        config.locale = "C";
        config.charset = "UTF-8";
        config.timezone = "";
        
        std::cout << "✅ 连接池配置创建成功" << std::endl;
        
        std::cout << "2. 使用连接池配置创建ResourceStorage..." << std::endl;
        ResourceStorage storage(config);
        
        if (storage.initialize()) {
            std::cout << "✅ 连接池配置构造函数测试成功" << std::endl;
            
            auto stats = storage.getConnectionPoolStats();
            std::cout << "📊 连接池统计:" << std::endl;
            std::cout << "   - 初始连接数: " << stats.total_connections << std::endl;
            std::cout << "   - 配置的最小连接数: " << config.min_connections << std::endl;
            std::cout << "   - 配置的最大连接数: " << config.max_connections << std::endl;
            
            storage.shutdown();
        } else {
            std::cout << "❌ 连接池配置构造函数测试失败" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 测试过程中发生异常: " << e.what() << std::endl;
    }
}

void testConcurrentAccess() {
    std::cout << "\n=== 并发访问测试 ===" << std::endl;
    
    try {
        TDenginePoolConfig config;
        config.host = "localhost";
        config.user = "root";
        config.password = "taosdata";
        config.min_connections = 2;
        config.max_connections = 5;
        config.initial_connections = 3;
        config.acquire_timeout = 5;
        
        ResourceStorage storage(config);
        
        if (!storage.initialize()) {
            std::cout << "❌ 并发测试初始化失败" << std::endl;
            return;
        }
        
        std::cout << "1. 启动多线程并发访问..." << std::endl;
        
        std::vector<std::thread> threads;
        std::atomic<int> success_count(0);
        std::atomic<int> failure_count(0);
        
        // 启动5个线程同时执行查询
        for (int i = 0; i < 5; ++i) {
            threads.emplace_back([&storage, &success_count, &failure_count, i]() {
                std::cout << "线程 " << i << " 开始执行查询..." << std::endl;
                
                try {
                    // 执行查询
                    auto results = storage.executeQuerySQL("SELECT NOW()");
                    success_count++;
                    std::cout << "✅ 线程 " << i << " 查询成功" << std::endl;
                    
                    // 模拟一些处理时间
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    
                } catch (const std::exception& e) {
                    failure_count++;
                    std::cout << "❌ 线程 " << i << " 查询失败: " << e.what() << std::endl;
                }
            });
        }
        
        // 等待所有线程完成
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "📊 并发测试结果:" << std::endl;
        std::cout << "   - 成功查询: " << success_count.load() << std::endl;
        std::cout << "   - 失败查询: " << failure_count.load() << std::endl;
        
        auto stats = storage.getConnectionPoolStats();
        std::cout << "📊 最终连接池统计:" << std::endl;
        std::cout << "   - 总连接数: " << stats.total_connections << std::endl;
        std::cout << "   - 活跃连接数: " << stats.active_connections << std::endl;
        std::cout << "   - 平均等待时间: " << stats.average_wait_time << "ms" << std::endl;
        
        storage.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 并发测试过程中发生异常: " << e.what() << std::endl;
    }
}

void testConfigurationUpdate() {
    std::cout << "\n=== 配置更新测试 ===" << std::endl;
    
    try {
        ResourceStorage storage("localhost", "root", "taosdata");
        
        if (!storage.initialize()) {
            std::cout << "❌ 配置更新测试初始化失败" << std::endl;
            return;
        }
        
        std::cout << "1. 获取初始配置统计..." << std::endl;
        auto initial_stats = storage.getConnectionPoolStats();
        std::cout << "📊 初始统计:" << std::endl;
        std::cout << "   - 总连接数: " << initial_stats.total_connections << std::endl;
        
        std::cout << "\n2. 更新连接池配置..." << std::endl;
        TDenginePoolConfig new_config;
        new_config.host = "localhost";
        new_config.user = "root";
        new_config.password = "taosdata";
        new_config.min_connections = 5;
        new_config.max_connections = 15;
        new_config.health_check_interval = 30;
        
        storage.updateConnectionPoolConfig(new_config);
        std::cout << "✅ 配置更新成功" << std::endl;
        
        std::cout << "\n3. 检查更新后的状态..." << std::endl;
        auto updated_stats = storage.getConnectionPoolStats();
        std::cout << "📊 更新后统计:" << std::endl;
        std::cout << "   - 总连接数: " << updated_stats.total_connections << std::endl;
        
        storage.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 配置更新测试过程中发生异常: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=== ResourceStorage TDengine连接池集成测试 ===" << std::endl;
    
    try {
        // 基本功能测试
        testBasicFunctionality();
        
        // 连接池配置构造函数测试
        testPoolConfigConstructor();
        
        // 并发访问测试
        testConcurrentAccess();
        
        // 配置更新测试
        testConfigurationUpdate();
        
        std::cout << "\n🎉 所有测试完成！" << std::endl;
        std::cout << "📝 注意：完整功能测试需要连接到真实的TDengine服务器。" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}