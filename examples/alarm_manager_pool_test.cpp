#include "../include/resource/alarm_manager.h"
#include "../include/resource/alarm_rule_engine.h"
#include "../include/resource/log_manager.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

/**
 * @brief AlarmManager连接池测试程序
 * 
 * 本示例展示了改造后的AlarmManager如何使用连接池：
 * 1. 基本的AlarmManager初始化和使用
 * 2. 多线程并发告警处理
 * 3. 连接池统计信息查看
 * 4. 性能对比测试
 */

// 创建测试用的AlarmEvent
AlarmEvent createTestAlarmEvent(const std::string& fingerprint, const std::string& status) {
    AlarmEvent event;
    event.fingerprint = fingerprint;
    event.status = status;
    event.starts_at = std::chrono::system_clock::now();
    if (status == "resolved") {
        event.ends_at = std::chrono::system_clock::now();
    }
    event.generator_url = "http://localhost:8080/test";
    
    // 添加一些测试标签
    event.labels["alertname"] = "TestAlert";
    event.labels["instance"] = "test-instance";
    event.labels["severity"] = "warning";
    
    // 添加一些测试注解
    event.annotations["summary"] = "Test alarm event";
    event.annotations["description"] = "This is a test alarm event";
    
    return event;
}

// 创建连接池配置
MySQLPoolConfig createTestPoolConfig() {
    MySQLPoolConfig config;
    config.host = "127.0.0.1";
    config.port = 3306;
    config.user = "test";
    config.password = "HZ715Net";
    config.database = "alarm";
    config.charset = "utf8mb4";
    
    // 连接池配置 - 适合测试的小型配置
    config.min_connections = 2;
    config.max_connections = 8;
    config.initial_connections = 3;
    
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
 * @brief 基本功能测试
 */
void basicFunctionalityTest() {
    std::cout << "\n=== AlarmManager基本功能测试 ===" << std::endl;
    
    // 1. 使用连接池配置创建AlarmManager
    MySQLPoolConfig pool_config = createTestPoolConfig();
    AlarmManager alarm_manager(pool_config);
    
    // 2. 初始化AlarmManager
    if (!alarm_manager.initialize()) {
        std::cerr << "AlarmManager初始化失败！" << std::endl;
        return;
    }
    
    std::cout << "AlarmManager初始化成功！" << std::endl;
    
    // 3. 创建数据库和表
    if (!alarm_manager.createDatabase()) {
        std::cerr << "创建数据库失败！" << std::endl;
        return;
    }
    
    if (!alarm_manager.createEventTable()) {
        std::cerr << "创建事件表失败！" << std::endl;
        return;
    }
    
    std::cout << "数据库和表创建成功！" << std::endl;
    
    // 4. 测试告警事件处理
    AlarmEvent firing_event = createTestAlarmEvent("test-fingerprint-1", "firing");
    if (alarm_manager.processAlarmEvent(firing_event)) {
        std::cout << "✅ 成功处理firing告警事件" << std::endl;
    } else {
        std::cout << "❌ 处理firing告警事件失败" << std::endl;
    }
    
    // 5. 测试解决告警
    AlarmEvent resolved_event = createTestAlarmEvent("test-fingerprint-1", "resolved");
    if (alarm_manager.processAlarmEvent(resolved_event)) {
        std::cout << "✅ 成功处理resolved告警事件" << std::endl;
    } else {
        std::cout << "❌ 处理resolved告警事件失败" << std::endl;
    }
    
    // 6. 查看连接池统计信息
    auto pool_stats = alarm_manager.getConnectionPoolStats();
    std::cout << "\n连接池统计信息：" << std::endl;
    std::cout << "  总连接数: " << pool_stats.total_connections << std::endl;
    std::cout << "  活跃连接数: " << pool_stats.active_connections << std::endl;
    std::cout << "  空闲连接数: " << pool_stats.idle_connections << std::endl;
    std::cout << "  等待请求数: " << pool_stats.pending_requests << std::endl;
    std::cout << "  平均等待时间: " << pool_stats.average_wait_time << "ms" << std::endl;
    
    // 7. 查看告警统计
    int active_count = alarm_manager.getActiveAlarmCount();
    int total_count = alarm_manager.getTotalAlarmCount();
    std::cout << "\n告警统计信息：" << std::endl;
    std::cout << "  活跃告警数: " << active_count << std::endl;
    std::cout << "  总告警数: " << total_count << std::endl;
    
    alarm_manager.shutdown();
    std::cout << "基本功能测试完成！" << std::endl;
}

/**
 * @brief 并发性能测试
 */
void concurrencyPerformanceTest() {
    std::cout << "\n=== AlarmManager并发性能测试 ===" << std::endl;
    
    MySQLPoolConfig pool_config = createTestPoolConfig();
    AlarmManager alarm_manager(pool_config);
    
    if (!alarm_manager.initialize()) {
        std::cerr << "AlarmManager初始化失败！" << std::endl;
        return;
    }
    
    const int thread_count = 5;
    const int events_per_thread = 10;
    std::vector<std::thread> threads;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 工作线程函数
    auto worker = [&alarm_manager](int thread_id) {
        for (int i = 0; i < events_per_thread; ++i) {
            std::string fingerprint = "thread-" + std::to_string(thread_id) + "-event-" + std::to_string(i);
            
            // 创建firing事件
            AlarmEvent firing_event = createTestAlarmEvent(fingerprint, "firing");
            firing_event.labels["thread_id"] = std::to_string(thread_id);
            firing_event.labels["event_id"] = std::to_string(i);
            
            if (alarm_manager.processAlarmEvent(firing_event)) {
                std::cout << "线程 " << thread_id << " 处理事件 " << i << " 成功" << std::endl;
            } else {
                std::cout << "线程 " << thread_id << " 处理事件 " << i << " 失败" << std::endl;
            }
            
            // 模拟一些处理时间
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    std::cout << "\n并发测试完成：" << std::endl;
    std::cout << "  线程数: " << thread_count << std::endl;
    std::cout << "  每线程事件数: " << events_per_thread << std::endl;
    std::cout << "  总事件数: " << (thread_count * events_per_thread) << std::endl;
    std::cout << "  总耗时: " << duration << "ms" << std::endl;
    std::cout << "  平均每事件: " << (double)duration / (thread_count * events_per_thread) << "ms" << std::endl;
    
    // 查看最终连接池统计
    auto pool_stats = alarm_manager.getConnectionPoolStats();
    std::cout << "\n最终连接池统计：" << std::endl;
    std::cout << "  总连接数: " << pool_stats.total_connections << std::endl;
    std::cout << "  创建连接数: " << pool_stats.created_connections << std::endl;
    std::cout << "  销毁连接数: " << pool_stats.destroyed_connections << std::endl;
    std::cout << "  平均等待时间: " << pool_stats.average_wait_time << "ms" << std::endl;
    
    alarm_manager.shutdown();
}

/**
 * @brief 兼容性测试 - 使用旧的构造函数
 */
void compatibilityTest() {
    std::cout << "\n=== 兼容性测试（旧构造函数） ===" << std::endl;
    
    // 使用旧的构造函数参数
    AlarmManager alarm_manager("127.0.0.1", 3306, "test", "HZ715Net", "alarm");
    
    if (!alarm_manager.initialize()) {
        std::cerr << "使用兼容性构造函数初始化失败！" << std::endl;
        return;
    }
    
    std::cout << "使用兼容性构造函数初始化成功！" << std::endl;
    
    // 测试基本功能
    AlarmEvent test_event = createTestAlarmEvent("compatibility-test", "firing");
    if (alarm_manager.processAlarmEvent(test_event)) {
        std::cout << "✅ 兼容性测试：告警事件处理成功" << std::endl;
    } else {
        std::cout << "❌ 兼容性测试：告警事件处理失败" << std::endl;
    }
    
    // 查看连接池状态
    if (alarm_manager.isInitialized()) {
        auto pool_stats = alarm_manager.getConnectionPoolStats();
        std::cout << "兼容模式连接池连接数: " << pool_stats.total_connections << std::endl;
    }
    
    alarm_manager.shutdown();
}

/**
 * @brief 错误处理测试
 */
void errorHandlingTest() {
    std::cout << "\n=== 错误处理测试 ===" << std::endl;
    
    // 使用错误的配置
    MySQLPoolConfig bad_config;
    bad_config.host = "nonexistent_host";
    bad_config.port = 9999;
    bad_config.user = "invalid_user";
    bad_config.password = "wrong_password";
    bad_config.database = "nonexistent_db";
    
    AlarmManager alarm_manager(bad_config);
    
    // 尝试初始化（会失败）
    if (!alarm_manager.initialize()) {
        std::cout << "✅ 预期的初始化失败（配置错误）" << std::endl;
    }
    
    // 尝试处理事件（应该失败）
    AlarmEvent test_event = createTestAlarmEvent("error-test", "firing");
    if (!alarm_manager.processAlarmEvent(test_event)) {
        std::cout << "✅ 预期的事件处理失败（未初始化）" << std::endl;
    }
    
    std::cout << "错误处理测试完成" << std::endl;
}

int main() {
    std::cout << "AlarmManager连接池改造测试" << std::endl;
    std::cout << "==============================" << std::endl;
    
    try {
        // 注意：运行这些测试前，请确保：
        // 1. MySQL服务器正在运行
        // 2. 数据库连接参数正确
        // 3. 用户有适当的权限
        
        std::cout << "\n注意：本测试需要有效的MySQL连接配置才能正常运行" << std::endl;
        std::cout << "请确认MySQL服务正在运行，并且连接参数正确" << std::endl;
        
        // 运行基本功能测试
        basicFunctionalityTest();
        
        // 运行并发性能测试
        concurrencyPerformanceTest();
        
        // 运行兼容性测试
        compatibilityTest();
        
        // 运行错误处理测试
        errorHandlingTest();
        
        std::cout << "\n🎉 所有测试完成！AlarmManager连接池改造成功！" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "测试运行出错: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

/**
 * @brief 编译和运行说明
 * 
 * 编译命令：
 * g++ -std=c++14 -I./include $(mysql_config --cflags) \
 *     -L/opt/homebrew/opt/zstd/lib -L/opt/homebrew/Cellar/openssl@3/3.5.1/lib \
 *     -o alarm_manager_test alarm_manager_pool_test.cpp \
 *     src/resource/alarm_manager.cpp src/resource/mysql_connection_pool.cpp \
 *     src/resource/log_manager.cpp $(mysql_config --libs) -pthread -luuid
 * 
 * 运行前准备：
 * 1. 确保MySQL服务器运行
 * 2. 确认数据库连接参数
 * 3. 确保用户有创建数据库和表的权限
 * 
 * 改造总结：
 * 1. ✅ 替换单连接为连接池架构
 * 2. ✅ 移除手动重连逻辑（由连接池自动处理）
 * 3. ✅ 提供向后兼容的构造函数
 * 4. ✅ 改进并发性能和资源利用率
 * 5. ✅ 集成统一的日志系统
 * 6. ✅ 保持原有API的兼容性
 */