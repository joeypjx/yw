#include "../include/resource/tdengine_connection_pool.h"
#include "../include/resource/resource_storage.h"
#include "../include/resource/log_manager.h"
#include <iostream>

int main() {
    std::cout << "=== TDengine连接池功能验证测试 ===" << std::endl;
    
    try {
        std::cout << "\n1. 测试TDengine连接池配置创建..." << std::endl;
        
        // 创建连接池配置
        TDenginePoolConfig config;
        config.host = "localhost";
        config.port = 6030;
        config.user = "root";
        config.password = "taosdata";
        config.database = "test_db";
        config.min_connections = 2;
        config.max_connections = 8;
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
        config.auto_reconnect = true;
        config.max_sql_length = 1048576;
        
        std::cout << "✅ TDengine连接池配置创建成功" << std::endl;
        
        std::cout << "\n2. 测试TDengineConnectionPool构造函数..." << std::endl;
        
        // 测试连接池构造函数
        TDengineConnectionPool pool(config);
        std::cout << "✅ TDengine连接池构造函数测试成功" << std::endl;
        
        std::cout << "\n3. 测试连接池管理器..." << std::endl;
        
        // 测试连接池管理器
        auto& manager = TDengineConnectionPoolManager::getInstance();
        std::cout << "✅ 获取连接池管理器单例成功" << std::endl;
        
        // 测试创建命名连接池（不初始化，避免实际连接）
        // manager.createPool("test_pool", config);
        
        std::cout << "\n4. 测试ResourceStorage与连接池集成..." << std::endl;
        
        // 测试新的ResourceStorage构造函数
        ResourceStorage storage_pool(config);
        std::cout << "✅ ResourceStorage连接池构造函数测试成功" << std::endl;
        
        // 测试兼容性构造函数
        ResourceStorage storage_compat("localhost", "root", "taosdata");
        std::cout << "✅ ResourceStorage兼容性构造函数测试成功" << std::endl;
        
        std::cout << "\n5. 测试状态检查..." << std::endl;
        std::cout << "   - storage_pool.isInitialized(): " << (storage_pool.isInitialized() ? "true" : "false") << std::endl;
        std::cout << "   - storage_compat.isInitialized(): " << (storage_compat.isInitialized() ? "true" : "false") << std::endl;
        
        std::cout << "\n6. 测试连接池统计获取..." << std::endl;
        auto stats = storage_pool.getConnectionPoolStats();
        std::cout << "✅ 连接池统计获取成功（即使未初始化）" << std::endl;
        std::cout << "   - 总连接数: " << stats.total_connections << std::endl;
        std::cout << "   - 活跃连接数: " << stats.active_connections << std::endl;
        std::cout << "   - 空闲连接数: " << stats.idle_connections << std::endl;
        
        std::cout << "\n7. 测试配置更新..." << std::endl;
        config.max_connections = 15;
        storage_pool.updateConnectionPoolConfig(config);
        std::cout << "✅ 配置更新成功" << std::endl;
        
        std::cout << "\n8. 测试TDengineResultRAII..." << std::endl;
        // 测试结果集RAII包装器构造（使用nullptr测试）
        TDengineResultRAII result_raii(nullptr);
        std::cout << "✅ TDengineResultRAII构造函数测试成功" << std::endl;
        
        std::cout << "\n9. 测试关闭..." << std::endl;
        storage_pool.shutdown();
        storage_compat.shutdown();
        std::cout << "✅ 关闭成功" << std::endl;
        
        std::cout << "\n🎉 所有功能验证测试完成！" << std::endl;
        std::cout << "📝 注意：此测试验证了TDengine连接池的代码结构和基本功能，" << std::endl;
        std::cout << "    实际数据库操作需要连接到真实的TDengine服务器。" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}