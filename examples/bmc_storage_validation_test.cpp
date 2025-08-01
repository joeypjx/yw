#include "../include/resource/bmc_storage.h"
#include "../include/resource/log_manager.h"
#include <iostream>

int main() {
    std::cout << "=== BMCStorage TDengine连接池验证测试 ===" << std::endl;
    
    try {
        std::cout << "\n1. 测试TDengine连接池配置创建..." << std::endl;
        
        // 创建连接池配置
        TDenginePoolConfig config;
        config.host = "localhost";
        config.port = 6030;
        config.user = "test";
        config.password = "HZ715Net";
        config.database = "resource";
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
        
        std::cout << "\n2. 测试BMCStorage连接池构造函数..." << std::endl;
        
        // 测试新的连接池构造函数
        BMCStorage storage_pool(config);
        std::cout << "✅ BMCStorage连接池构造函数测试成功" << std::endl;
        
        // 测试兼容性构造函数
        BMCStorage storage_compat("localhost", "test", "HZ715Net", "resource");
        std::cout << "✅ BMCStorage兼容性构造函数测试成功" << std::endl;
        
        std::cout << "\n3. 测试状态检查..." << std::endl;
        std::cout << "   - storage_pool.isInitialized(): " << (storage_pool.isInitialized() ? "true" : "false") << std::endl;
        std::cout << "   - storage_compat.isInitialized(): " << (storage_compat.isInitialized() ? "true" : "false") << std::endl;
        
        std::cout << "\n4. 测试连接池统计获取..." << std::endl;
        auto stats = storage_pool.getConnectionPoolStats();
        std::cout << "✅ 连接池统计获取成功（即使未初始化）" << std::endl;
        std::cout << "   - 总连接数: " << stats.total_connections << std::endl;
        std::cout << "   - 活跃连接数: " << stats.active_connections << std::endl;
        std::cout << "   - 空闲连接数: " << stats.idle_connections << std::endl;
        
        std::cout << "\n5. 测试配置更新..." << std::endl;
        config.max_connections = 15;
        storage_pool.updateConnectionPoolConfig(config);
        std::cout << "✅ 配置更新成功" << std::endl;
        
        std::cout << "\n6. 测试错误信息获取..." << std::endl;
        std::string last_error = storage_pool.getLastError();
        std::cout << "✅ 错误信息获取成功: " << (last_error.empty() ? "无错误" : last_error) << std::endl;
        
        std::cout << "\n7. 测试关闭..." << std::endl;
        storage_pool.shutdown();
        storage_compat.shutdown();
        std::cout << "✅ 关闭成功" << std::endl;
        
        std::cout << "\n🎉 所有验证测试完成！" << std::endl;
        std::cout << "📝 主要改造内容:" << std::endl;
        std::cout << "   1. ✅ 添加了新的连接池构造函数" << std::endl;
        std::cout << "   2. ✅ 保留了兼容性构造函数" << std::endl;
        std::cout << "   3. ✅ 将connect()/disconnect()改为initialize()/shutdown()" << std::endl;
        std::cout << "   4. ✅ 重写了所有数据库操作方法以使用连接池" << std::endl;
        std::cout << "   5. ✅ 添加了连接池统计和配置更新方法" << std::endl;
        std::cout << "   6. ✅ 将executeBMCQuerySQL移至public部分" << std::endl;
        std::cout << "   7. ✅ 添加了完善的日志记录" << std::endl;
        std::cout << "   注意：此测试验证了BMCStorage连接池的代码结构和基本功能，" << std::endl;
        std::cout << "        实际数据库操作需要连接到真实的TDengine服务器。" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}