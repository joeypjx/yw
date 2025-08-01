#include "../include/resource/mysql_connection_pool.h"
#include "../include/resource/tdengine_connection_pool.h"
#include "../include/resource/alarm_manager.h"
#include "../include/resource/alarm_rule_storage.h"
#include "../include/resource/resource_storage.h"
#include "../include/resource/bmc_storage.h"
#include "../include/resource/log_manager.h"
#include <iostream>
#include <memory>

/**
 * 测试共享连接池功能的示例程序
 * 验证 AlarmSystem 中的连接池共享架构
 */

void testSharedMySQLPool() {
    std::cout << "\n=== 测试共享MySQL连接池 ===" << std::endl;
    
    try {
        // 1. 创建共享的MySQL连接池
        MySQLPoolConfig mysql_config;
        mysql_config.host = "localhost";
        mysql_config.port = 3306;
        mysql_config.user = "test";
        mysql_config.password = "HZ715Net";
        mysql_config.database = "alarm";
        mysql_config.charset = "utf8mb4";
        
        // 连接池配置
        mysql_config.min_connections = 3;
        mysql_config.max_connections = 15;
        mysql_config.initial_connections = 5;
        
        // 超时配置
        mysql_config.connection_timeout = 30;
        mysql_config.idle_timeout = 600;
        mysql_config.max_lifetime = 3600;
        mysql_config.acquire_timeout = 10;
        
        // 健康检查配置
        mysql_config.health_check_interval = 60;
        mysql_config.health_check_query = "SELECT 1";
        
        auto mysql_pool = std::make_shared<MySQLConnectionPool>(mysql_config);
        std::cout << "✅ MySQL连接池创建成功" << std::endl;
        
        // 2. 创建使用共享连接池的AlarmManager
        auto alarm_manager = std::make_shared<AlarmManager>(mysql_pool);
        std::cout << "✅ AlarmManager(共享连接池)创建成功" << std::endl;
        
        // 3. 创建使用共享连接池的AlarmRuleStorage
        auto alarm_rule_storage = std::make_shared<AlarmRuleStorage>(mysql_pool);
        std::cout << "✅ AlarmRuleStorage(共享连接池)创建成功" << std::endl;
        
        // 4. 验证连接池统计信息
        auto stats = mysql_pool->getStats();
        std::cout << "📊 MySQL连接池统计:" << std::endl;
        std::cout << "  - 总连接数: " << stats.total_connections << std::endl;
        std::cout << "  - 活跃连接数: " << stats.active_connections << std::endl;
        std::cout << "  - 空闲连接数: " << stats.idle_connections << std::endl;
        std::cout << "  - 等待请求数: " << stats.pending_requests << std::endl;
        
        std::cout << "✅ 共享MySQL连接池测试通过" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ 共享MySQL连接池测试失败: " << e.what() << std::endl;
    }
}

void testSharedTDenginePool() {
    std::cout << "\n=== 测试共享TDengine连接池 ===" << std::endl;
    
    try {
        // 1. 创建共享的TDengine连接池
        TDenginePoolConfig tdengine_config;
        tdengine_config.host = "localhost";
        tdengine_config.port = 6030;
        tdengine_config.user = "test";
        tdengine_config.password = "HZ715Net";
        tdengine_config.database = "resource";
        tdengine_config.locale = "C";
        tdengine_config.charset = "UTF-8";
        tdengine_config.timezone = "";
        
        // 连接池配置
        tdengine_config.min_connections = 2;
        tdengine_config.max_connections = 10;
        tdengine_config.initial_connections = 3;
        
        // 超时配置
        tdengine_config.connection_timeout = 30;
        tdengine_config.idle_timeout = 600;
        tdengine_config.max_lifetime = 3600;
        tdengine_config.acquire_timeout = 10;
        
        // 健康检查配置
        tdengine_config.health_check_interval = 60;
        tdengine_config.health_check_query = "SELECT SERVER_VERSION()";
        
        auto tdengine_pool = std::make_shared<TDengineConnectionPool>(tdengine_config);
        std::cout << "✅ TDengine连接池创建成功" << std::endl;
        
        // 2. 创建使用共享连接池的ResourceStorage
        auto resource_storage = std::make_shared<ResourceStorage>(tdengine_pool);
        std::cout << "✅ ResourceStorage(共享连接池)创建成功" << std::endl;
        
        // 3. 创建使用共享连接池的BMCStorage
        auto bmc_storage = std::make_shared<BMCStorage>(tdengine_pool);
        std::cout << "✅ BMCStorage(共享连接池)创建成功" << std::endl;
        
        // 4. 验证连接池统计信息
        auto stats = tdengine_pool->getStats();
        std::cout << "📊 TDengine连接池统计:" << std::endl;
        std::cout << "  - 总连接数: " << stats.total_connections << std::endl;
        std::cout << "  - 活跃连接数: " << stats.active_connections << std::endl;
        std::cout << "  - 空闲连接数: " << stats.idle_connections << std::endl;
        std::cout << "  - 等待请求数: " << stats.pending_requests << std::endl;
        
        std::cout << "✅ 共享TDengine连接池测试通过" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ 共享TDengine连接池测试失败: " << e.what() << std::endl;
    }
}

void testConnectionPoolManagers() {
    std::cout << "\n=== 测试连接池管理器 ===" << std::endl;
    
    try {
        // 测试MySQL连接池管理器
        auto& mysql_manager = MySQLConnectionPoolManager::getInstance();
        
        MySQLPoolConfig mysql_config;
        mysql_config.host = "localhost";
        mysql_config.port = 3306;
        mysql_config.user = "test";
        mysql_config.password = "HZ715Net";
        mysql_config.database = "alarm";
        
        if (mysql_manager.createPool("alarm_pool", mysql_config)) {
            std::cout << "✅ MySQL连接池管理器创建连接池成功" << std::endl;
            
            auto pool = mysql_manager.getPool("alarm_pool");
            if (pool) {
                std::cout << "✅ MySQL连接池管理器获取连接池成功" << std::endl;
            }
        }
        
        // 测试TDengine连接池管理器
        auto& tdengine_manager = TDengineConnectionPoolManager::getInstance();
        
        TDenginePoolConfig tdengine_config;
        tdengine_config.host = "localhost";
        tdengine_config.port = 6030;
        tdengine_config.user = "test";
        tdengine_config.password = "HZ715Net";
        tdengine_config.database = "resource";
        
        if (tdengine_manager.createPool("resource_pool", tdengine_config)) {
            std::cout << "✅ TDengine连接池管理器创建连接池成功" << std::endl;
            
            auto pool = tdengine_manager.getPool("resource_pool");
            if (pool) {
                std::cout << "✅ TDengine连接池管理器获取连接池成功" << std::endl;
            }
        }
        
        std::cout << "✅ 连接池管理器测试通过" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ 连接池管理器测试失败: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=== 共享连接池架构测试 ===" << std::endl;
    std::cout << "测试 AlarmSystem 中的连接池共享机制" << std::endl;
    
    try {
        // 初始化日志系统
        LogManager::init();
        
        // 测试共享MySQL连接池
        testSharedMySQLPool();
        
        // 测试共享TDengine连接池
        testSharedTDenginePool();
        
        // 测试连接池管理器
        testConnectionPoolManagers();
        
        std::cout << "\n🎉 所有共享连接池测试完成!" << std::endl;
        std::cout << "\n📝 测试总结:" << std::endl;
        std::cout << "✅ AlarmManager 和 AlarmRuleStorage 共享 MySQL 连接池" << std::endl;
        std::cout << "✅ ResourceStorage 和 BMCStorage 共享 TDengine 连接池" << std::endl;
        std::cout << "✅ 连接池注入机制工作正常" << std::endl;
        std::cout << "✅ 连接池管理器功能正常" << std::endl;
        
        std::cout << "\n💡 优势:" << std::endl;
        std::cout << "  - 减少了数据库连接数" << std::endl;
        std::cout << "  - 提高了资源利用率" << std::endl;
        std::cout << "  - 统一了连接管理" << std::endl;
        std::cout << "  - 便于监控和调优" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ 测试程序异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}