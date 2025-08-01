#include "tdengine_connection_pool.h"
#include "log_manager.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

void demonstrateBasicUsage() {
    std::cout << "\n=== 基本用法演示 ===" << std::endl;
    
    // 创建连接池配置
    TDenginePoolConfig config;
    config.host = "localhost";
    config.port = 6030;
    config.user = "root";
    config.password = "taosdata";
    config.database = "";
    config.min_connections = 2;
    config.max_connections = 8;
    config.initial_connections = 3;
    config.connection_timeout = 30;
    config.idle_timeout = 600;
    config.max_lifetime = 3600;
    config.acquire_timeout = 10;
    config.health_check_interval = 60;
    config.health_check_query = "SELECT SERVER_VERSION()";
    
    std::cout << "1. 创建TDengine连接池..." << std::endl;
    TDengineConnectionPool pool(config);
    
    std::cout << "2. 初始化连接池..." << std::endl;
    if (!pool.initialize()) {
        std::cout << "❌ 连接池初始化失败（可能是因为没有可用的TDengine服务器）" << std::endl;
        return;
    }
    
    std::cout << "✅ 连接池初始化成功" << std::endl;
    
    // 获取统计信息
    auto stats = pool.getStats();
    std::cout << "📊 初始统计信息:" << std::endl;
    std::cout << "   - 总连接数: " << stats.total_connections << std::endl;
    std::cout << "   - 活跃连接数: " << stats.active_connections << std::endl;
    std::cout << "   - 空闲连接数: " << stats.idle_connections << std::endl;
    std::cout << "   - 等待请求数: " << stats.pending_requests << std::endl;
    
    std::cout << "\n3. 测试获取和释放连接..." << std::endl;
    {
        // 使用RAII守卫获取连接
        TDengineConnectionGuard guard(std::make_shared<TDengineConnectionPool>(std::move(pool)));
        if (guard.isValid()) {
            std::cout << "✅ 成功获取连接" << std::endl;
            
            // 可以在这里使用连接执行查询
            TAOS* taos = guard->get();
            if (taos) {
                std::cout << "✅ 连接指针有效" << std::endl;
            }
        } else {
            std::cout << "❌ 获取连接失败" << std::endl;
        }
    } // 守卫析构时会自动释放连接
    
    std::cout << "✅ 连接已自动释放" << std::endl;
}

void demonstrateRAIIGuard() {
    std::cout << "\n=== RAII守卫演示 ===" << std::endl;
    
    TDenginePoolConfig config;
    config.host = "localhost";
    config.min_connections = 1;
    config.max_connections = 3;
    config.initial_connections = 2;
    
    auto pool = std::make_shared<TDengineConnectionPool>(config);
    
    std::cout << "1. 测试RAII自动管理..." << std::endl;
    if (pool->initialize()) {
        {
            TDengineConnectionGuard guard1(pool);
            if (guard1.isValid()) {
                std::cout << "✅ Guard1获取连接成功" << std::endl;
                
                {
                    TDengineConnectionGuard guard2(pool);
                    if (guard2.isValid()) {
                        std::cout << "✅ Guard2获取连接成功" << std::endl;
                        
                        // 显示当前统计
                        auto stats = pool->getStats();
                        std::cout << "📊 当前活跃连接: " << stats.active_connections << std::endl;
                    }
                    std::cout << "✅ Guard2自动释放连接" << std::endl;
                }
                
                auto stats = pool->getStats();
                std::cout << "📊 Guard2释放后活跃连接: " << stats.active_connections << std::endl;
            }
            std::cout << "✅ Guard1自动释放连接" << std::endl;
        }
        
        auto stats = pool->getStats();
        std::cout << "📊 所有连接释放后活跃连接: " << stats.active_connections << std::endl;
        
        pool->shutdown();
    } else {
        std::cout << "❌ 连接池初始化失败" << std::endl;
    }
}

void demonstrateConcurrency() {
    std::cout << "\n=== 并发访问演示 ===" << std::endl;
    
    TDenginePoolConfig config;
    config.host = "localhost";
    config.min_connections = 2;
    config.max_connections = 5;
    config.initial_connections = 3;
    config.acquire_timeout = 5;
    
    auto pool = std::make_shared<TDengineConnectionPool>(config);
    
    if (!pool->initialize()) {
        std::cout << "❌ 连接池初始化失败" << std::endl;
        return;
    }
    
    std::cout << "1. 启动多个线程并发获取连接..." << std::endl;
    
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);
    std::atomic<int> failure_count(0);
    
    // 启动5个线程同时请求连接
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([pool, &success_count, &failure_count, i]() {
            std::cout << "线程 " << i << " 尝试获取连接..." << std::endl;
            
            TDengineConnectionGuard guard(pool, 3000); // 3秒超时
            if (guard.isValid()) {
                success_count++;
                std::cout << "✅ 线程 " << i << " 获取连接成功" << std::endl;
                
                // 模拟使用连接
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                
                std::cout << "✅ 线程 " << i << " 完成工作，释放连接" << std::endl;
            } else {
                failure_count++;
                std::cout << "❌ 线程 " << i << " 获取连接失败" << std::endl;
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "📊 并发测试结果:" << std::endl;
    std::cout << "   - 成功获取连接: " << success_count.load() << std::endl;
    std::cout << "   - 获取连接失败: " << failure_count.load() << std::endl;
    
    auto stats = pool->getStats();
    std::cout << "📊 最终统计:" << std::endl;
    std::cout << "   - 总连接数: " << stats.total_connections << std::endl;
    std::cout << "   - 活跃连接数: " << stats.active_connections << std::endl;
    std::cout << "   - 平均等待时间: " << stats.average_wait_time << "ms" << std::endl;
    
    pool->shutdown();
}

void demonstratePoolManager() {
    std::cout << "\n=== 连接池管理器演示 ===" << std::endl;
    
    auto& manager = TDengineConnectionPoolManager::getInstance();
    
    std::cout << "1. 创建多个命名连接池..." << std::endl;
    
    // 创建不同用途的连接池
    TDenginePoolConfig config1;
    config1.host = "localhost";
    config1.database = "test_db1";
    config1.min_connections = 1;
    config1.max_connections = 3;
    
    TDenginePoolConfig config2;
    config2.host = "localhost";
    config2.database = "test_db2";
    config2.min_connections = 1;
    config2.max_connections = 3;
    
    if (manager.createPool("pool1", config1)) {
        std::cout << "✅ 创建连接池 'pool1' 成功" << std::endl;
    } else {
        std::cout << "❌ 创建连接池 'pool1' 失败" << std::endl;
    }
    
    if (manager.createPool("pool2", config2)) {
        std::cout << "✅ 创建连接池 'pool2' 成功" << std::endl;
    } else {
        std::cout << "❌ 创建连接池 'pool2' 失败" << std::endl;
    }
    
    std::cout << "\n2. 使用不同的连接池..." << std::endl;
    auto pool1 = manager.getPool("pool1");
    auto pool2 = manager.getPool("pool2");
    
    if (pool1) {
        std::cout << "✅ 获取 pool1 成功" << std::endl;
        TDengineConnectionGuard guard1(pool1);
        if (guard1.isValid()) {
            std::cout << "✅ 从 pool1 获取连接成功" << std::endl;
        }
    }
    
    if (pool2) {
        std::cout << "✅ 获取 pool2 成功" << std::endl;
        TDengineConnectionGuard guard2(pool2);
        if (guard2.isValid()) {
            std::cout << "✅ 从 pool2 获取连接成功" << std::endl;
        }
    }
    
    std::cout << "\n3. 列出所有连接池..." << std::endl;
    auto pool_names = manager.getAllPoolNames();
    for (const auto& name : pool_names) {
        std::cout << "   - " << name << std::endl;
    }
    
    std::cout << "\n4. 清理所有连接池..." << std::endl;
    manager.destroyAllPools();
    std::cout << "✅ 所有连接池已清理" << std::endl;
}

void demonstrateErrorHandling() {
    std::cout << "\n=== 错误处理演示 ===" << std::endl;
    
    std::cout << "1. 测试无效配置..." << std::endl;
    TDenginePoolConfig invalid_config;
    invalid_config.host = "invalid_host_that_does_not_exist";
    invalid_config.port = 99999;
    invalid_config.user = "invalid_user";
    invalid_config.password = "invalid_password";
    invalid_config.min_connections = 1;
    invalid_config.max_connections = 2;
    invalid_config.initial_connections = 1;
    
    TDengineConnectionPool pool(invalid_config);
    if (!pool.initialize()) {
        std::cout << "✅ 正确处理了无效配置（预期行为）" << std::endl;
    } else {
        std::cout << "❌ 应该初始化失败但成功了" << std::endl;
    }
    
    std::cout << "\n2. 测试超时获取连接..." << std::endl;
    TDenginePoolConfig timeout_config;
    timeout_config.host = "localhost";
    timeout_config.min_connections = 0;
    timeout_config.max_connections = 1;
    timeout_config.initial_connections = 0;
    
    TDengineConnectionPool timeout_pool(timeout_config);
    if (timeout_pool.initialize()) {
        auto pool_shared = std::make_shared<TDengineConnectionPool>(std::move(timeout_pool));
        
        // 先获取唯一的连接
        TDengineConnectionGuard guard1(pool_shared);
        if (guard1.isValid()) {
            std::cout << "✅ 获取第一个连接成功" << std::endl;
            
            // 尝试获取第二个连接（应该超时）
            TDengineConnectionGuard guard2(pool_shared, 1000); // 1秒超时
            if (!guard2.isValid()) {
                std::cout << "✅ 正确处理了连接获取超时（预期行为）" << std::endl;
            } else {
                std::cout << "❌ 应该超时但成功获取了连接" << std::endl;
            }
        }
        
        pool_shared->shutdown();
    }
}

int main() {
    std::cout << "=== TDengine连接池功能演示 ===" << std::endl;
    
    try {
        // 基本功能演示
        demonstrateBasicUsage();
        
        // RAII守卫演示
        demonstrateRAIIGuard();
        
        // 并发访问演示
        demonstrateConcurrency();
        
        // 连接池管理器演示
        demonstratePoolManager();
        
        // 错误处理演示
        demonstrateErrorHandling();
        
        std::cout << "\n🎉 所有演示完成！" << std::endl;
        std::cout << "📝 注意：部分功能需要连接到真实的TDengine服务器才能完全验证。" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 演示过程中发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}