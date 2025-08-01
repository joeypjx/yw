#include "../include/resource/mysql_connection_pool.h"
#include "../include/resource/log_manager.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

/**
 * @brief MySQL连接池使用示例
 * 
 * 本示例展示了如何使用MySQL连接池进行数据库操作，包括：
 * 1. 基本的连接池配置和初始化
 * 2. 单个连接的获取和使用
 * 3. 多线程并发访问
 * 4. 连接池管理器的使用
 * 5. 性能监控和统计
 */

// 示例数据库配置
MySQLPoolConfig createExampleConfig() {
    // std::string mysql_host = "127.0.0.1";
    // int mysql_port = 3306;
    // std::string db_user = "test";
    // std::string db_password = "HZ715Net";
    // std::string resource_db = "resource";
    // std::string alarm_db = "alarm";

    MySQLPoolConfig config;
    config.host = "127.0.0.1";
    config.port = 3306;
    config.user = "test";
    config.password = "HZ715Net";
    config.database = "alarm";
    config.charset = "utf8mb4";
    
    // 连接池配置
    config.min_connections = 3;
    config.max_connections = 10;
    config.initial_connections = 5;
    
    // 超时配置
    config.connection_timeout = 10;
    config.idle_timeout = 300;      // 5分钟
    config.max_lifetime = 1800;     // 30分钟
    config.acquire_timeout = 5;
    
    // 健康检查配置
    config.health_check_interval = 30;
    config.health_check_query = "SELECT 1";
    
    return config;
}

/**
 * @brief 基本使用示例
 */
void basicUsageExample() {
    std::cout << "\n=== 基本使用示例 ===" << std::endl;
    
    // 1. 创建连接池配置
    MySQLPoolConfig config = createExampleConfig();
    
    // 2. 创建连接池
    MySQLConnectionPool pool(config);
    
    // 3. 初始化连接池
    if (!pool.initialize()) {
        std::cerr << "连接池初始化失败！" << std::endl;
        return;
    }
    
    std::cout << "连接池初始化成功！" << std::endl;
    
    // 4. 获取连接
    auto connection = pool.getConnection();
    if (!connection) {
        std::cerr << "获取连接失败！" << std::endl;
        return;
    }
    
    std::cout << "成功获取数据库连接！" << std::endl;
    
    // 5. 使用连接执行查询
    MYSQL* mysql = connection->get();
    if (mysql_query(mysql, "SELECT DATABASE(), USER(), VERSION()") == 0) {
        MYSQL_RES* result = mysql_store_result(mysql);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row) {
                std::cout << "数据库: " << (row[0] ? row[0] : "NULL") << std::endl;
                std::cout << "用户: " << (row[1] ? row[1] : "NULL") << std::endl;
                std::cout << "版本: " << (row[2] ? row[2] : "NULL") << std::endl;
            }
            mysql_free_result(result);
        }
    } else {
        std::cerr << "查询失败: " << mysql_error(mysql) << std::endl;
    }
    
    // 6. 连接会在connection析构时自动释放
    std::cout << "连接将自动释放回连接池" << std::endl;
    
    // 7. 查看连接池统计信息
    auto stats = pool.getStats();
    std::cout << "连接池统计:" << std::endl;
    std::cout << "  总连接数: " << stats.total_connections << std::endl;
    std::cout << "  活跃连接数: " << stats.active_connections << std::endl;
    std::cout << "  空闲连接数: " << stats.idle_connections << std::endl;
    std::cout << "  等待请求数: " << stats.pending_requests << std::endl;
}

/**
 * @brief RAII连接守护示例
 */
void raii_guard_example() {
    std::cout << "\n=== RAII连接守护示例 ===" << std::endl;
    
    MySQLPoolConfig config = createExampleConfig();
    auto pool = std::make_shared<MySQLConnectionPool>(config);
    
    if (!pool->initialize()) {
        std::cerr << "连接池初始化失败！" << std::endl;
        return;
    }
    
    // 使用MySQLConnectionGuard自动管理连接
    {
        MySQLConnectionGuard guard(pool, 5000); // 5秒超时
        
        if (!guard.isValid()) {
            std::cerr << "获取连接失败！" << std::endl;
            return;
        }
        
        std::cout << "通过RAII守护获取连接成功！" << std::endl;
        
        // 使用连接
        MYSQL* mysql = guard->get();
        if (mysql_query(mysql, "SELECT 'Hello from connection guard!'") == 0) {
            MYSQL_RES* result = mysql_store_result(mysql);
            if (result) {
                MYSQL_ROW row = mysql_fetch_row(result);
                if (row && row[0]) {
                    std::cout << "查询结果: " << row[0] << std::endl;
                }
                mysql_free_result(result);
            }
        }
        
        // guard析构时会自动释放连接
    }
    
    std::cout << "连接已通过RAII自动释放" << std::endl;
}

/**
 * @brief 多线程并发访问示例
 */
void concurrencyExample() {
    std::cout << "\n=== 多线程并发访问示例 ===" << std::endl;
    
    MySQLPoolConfig config = createExampleConfig();
    config.max_connections = 5; // 限制最大连接数以测试并发
    
    auto pool = std::make_shared<MySQLConnectionPool>(config);
    if (!pool->initialize()) {
        std::cerr << "连接池初始化失败！" << std::endl;
        return;
    }
    
    const int thread_count = 8;
    const int queries_per_thread = 3;
    std::vector<std::thread> threads;
    
    auto worker = [pool](int thread_id) {
        for (int i = 0; i < queries_per_thread; ++i) {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // 获取连接
            auto connection = pool->getConnection(10000); // 10秒超时
            if (!connection) {
                std::cerr << "线程 " << thread_id << " 获取连接失败！" << std::endl;
                continue;
            }
            
            auto get_time = std::chrono::high_resolution_clock::now();
            
            // 模拟数据库操作
            MYSQL* mysql = connection->get();
            std::string query = "SELECT " + std::to_string(thread_id) + " as thread_id, " + 
                               std::to_string(i) + " as query_index, NOW() as current_time";
            
            if (mysql_query(mysql, query.c_str()) == 0) {
                MYSQL_RES* result = mysql_store_result(mysql);
                if (result) {
                    MYSQL_ROW row = mysql_fetch_row(result);
                    if (row) {
                        auto end_time = std::chrono::high_resolution_clock::now();
                        auto get_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            get_time - start_time).count();
                        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_time - start_time).count();
                        
                        std::cout << "线程 " << thread_id << " 查询 " << i 
                                  << " - 获取连接: " << get_duration << "ms"
                                  << ", 总时间: " << total_duration << "ms"
                                  << ", 结果: thread_id=" << (row[0] ? row[0] : "NULL") << std::endl;
                    }
                    mysql_free_result(result);
                }
            } else {
                std::cerr << "线程 " << thread_id << " 查询失败: " << mysql_error(mysql) << std::endl;
            }
            
            // 模拟处理时间
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    };
    
    // 启动多个线程
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(worker, i);
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    // 显示最终统计信息
    auto stats = pool->getStats();
    std::cout << "\n并发测试完成，最终统计:" << std::endl;
    std::cout << "  总连接数: " << stats.total_connections << std::endl;
    std::cout << "  活跃连接数: " << stats.active_connections << std::endl;
    std::cout << "  空闲连接数: " << stats.idle_connections << std::endl;
    std::cout << "  创建的连接总数: " << stats.created_connections << std::endl;
    std::cout << "  销毁的连接总数: " << stats.destroyed_connections << std::endl;
    std::cout << "  平均等待时间: " << stats.average_wait_time << "ms" << std::endl;
}

/**
 * @brief 连接池管理器示例
 */
void poolManagerExample() {
    std::cout << "\n=== 连接池管理器示例 ===" << std::endl;
    
    auto& manager = MySQLConnectionPoolManager::getInstance();
    
    // 创建多个连接池
    MySQLPoolConfig config1 = createExampleConfig();
    config1.database = "db1";
    
    MySQLPoolConfig config2 = createExampleConfig();
    config2.database = "db2";
    config2.max_connections = 15;
    
    // 创建连接池
    if (!manager.createPool("pool1", config1)) {
        std::cerr << "创建连接池 pool1 失败！" << std::endl;
        return;
    }
    
    if (!manager.createPool("pool2", config2)) {
        std::cerr << "创建连接池 pool2 失败！" << std::endl;
        return;
    }
    
    std::cout << "成功创建了两个连接池！" << std::endl;
    
    // 获取连接池
    auto pool1 = manager.getPool("pool1");
    auto pool2 = manager.getPool("pool2");
    
    if (pool1 && pool2) {
        std::cout << "成功获取连接池引用！" << std::endl;
        
        // 显示统计信息
        auto stats1 = pool1->getStats();
        auto stats2 = pool2->getStats();
        
        std::cout << "Pool1 统计: 总连接=" << stats1.total_connections 
                  << ", 空闲=" << stats1.idle_connections << std::endl;
        std::cout << "Pool2 统计: 总连接=" << stats2.total_connections 
                  << ", 空闲=" << stats2.idle_connections << std::endl;
    }
    
    // 列出所有连接池
    auto pool_names = manager.getAllPoolNames();
    std::cout << "所有连接池: ";
    for (const auto& name : pool_names) {
        std::cout << name << " ";
    }
    std::cout << std::endl;
    
    // 销毁连接池
    manager.destroyPool("pool1");
    std::cout << "已销毁 pool1" << std::endl;
    
    // 最终清理
    manager.destroyAllPools();
    std::cout << "已销毁所有连接池" << std::endl;
}

/**
 * @brief 错误处理示例
 */
void errorHandlingExample() {
    std::cout << "\n=== 错误处理示例 ===" << std::endl;
    
    // 使用错误的配置
    MySQLPoolConfig bad_config;
    bad_config.host = "nonexistent_host";
    bad_config.port = 9999;
    bad_config.user = "invalid_user";
    bad_config.password = "wrong_password";
    bad_config.database = "nonexistent_db";
    
    MySQLConnectionPool pool(bad_config);
    
    // 尝试初始化（会失败）
    if (!pool.initialize()) {
        std::cout << "预期的初始化失败（配置错误）" << std::endl;
    }
    
    // 尝试获取连接（会失败）
    auto connection = pool.getConnection(1000); // 1秒超时
    if (!connection) {
        std::cout << "预期的连接获取失败" << std::endl;
    }
    
    // 检查连接池健康状态
    if (!pool.isHealthy()) {
        std::cout << "连接池状态不健康（符合预期）" << std::endl;
    }
}

/**
 * @brief 性能测试示例
 */
void performanceTest() {
    std::cout << "\n=== 性能测试示例 ===" << std::endl;
    
    MySQLPoolConfig config = createExampleConfig();
    config.min_connections = 5;
    config.max_connections = 20;
    
    MySQLConnectionPool pool(config);
    if (!pool.initialize()) {
        std::cerr << "连接池初始化失败！" << std::endl;
        return;
    }
    
    const int total_operations = 100;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 串行执行测试
    for (int i = 0; i < total_operations; ++i) {
        auto connection = pool.getConnection();
        if (connection) {
            MYSQL* mysql = connection->get();
            mysql_query(mysql, "SELECT 1");
            MYSQL_RES* result = mysql_store_result(mysql);
            if (result) {
                mysql_free_result(result);
            }
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    std::cout << "完成 " << total_operations << " 次数据库操作" << std::endl;
    std::cout << "总耗时: " << duration << "ms" << std::endl;
    std::cout << "平均每次操作: " << (double)duration / total_operations << "ms" << std::endl;
    
    auto stats = pool.getStats();
    std::cout << "最终统计:" << std::endl;
    std::cout << "  创建连接总数: " << stats.created_connections << std::endl;
    std::cout << "  销毁连接总数: " << stats.destroyed_connections << std::endl;
    std::cout << "  平均等待时间: " << stats.average_wait_time << "ms" << std::endl;
}

int main() {
    std::cout << "MySQL连接池使用示例" << std::endl;
    std::cout << "===================" << std::endl;
    
    try {
        // 注意：运行这些示例前，请确保：
        // 1. 已安装MySQL客户端库
        // 2. MySQL服务器正在运行
        // 3. 修改createExampleConfig()中的数据库连接参数
        
        std::cout << "\n注意：本示例需要有效的MySQL连接配置才能正常运行" << std::endl;
        std::cout << "请修改createExampleConfig()函数中的数据库连接参数" << std::endl;
        
        // 运行真实数据库连接的示例
        basicUsageExample();
        raii_guard_example();
        concurrencyExample();
        poolManagerExample();
        performanceTest();
        
        // 运行不需要数据库连接的示例
        errorHandlingExample();
        
        std::cout << "\n如需运行完整示例，请配置有效的MySQL连接参数并取消注释相关代码" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "示例运行出错: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

/**
 * @brief 编译和运行说明
 * 
 * 编译命令示例：
 * g++ -std=c++14 -I../include -L/usr/lib/mysql -lmysqlclient \
 *     mysql_connection_pool_example.cpp ../src/resource/mysql_connection_pool.cpp \
 *     ../src/resource/log_manager.cpp -o mysql_pool_example
 * 
 * 运行前准备：
 * 1. 确保MySQL服务器运行
 * 2. 创建测试数据库：CREATE DATABASE test_db;
 * 3. 修改createExampleConfig()中的连接参数
 * 4. 取消注释main()函数中的示例调用
 * 
 * 性能调优建议：
 * 1. 根据应用负载调整min_connections和max_connections
 * 2. 设置合理的超时时间
 * 3. 定期监控连接池统计信息
 * 4. 考虑使用连接池管理器管理多个数据库的连接池
 */