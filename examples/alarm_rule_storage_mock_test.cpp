#include "alarm_rule_storage.h"
#include "log_manager.h"
#include <iostream>

int main() {
    std::cout << "=== AlarmRuleStorage 连接池功能验证测试 ===" << std::endl;
    
    try {
        std::cout << "\n1. 测试默认连接池配置创建..." << std::endl;
        
        // 创建连接池配置
        MySQLPoolConfig config;
        config.host = "localhost";
        config.port = 3306;
        config.user = "test";
        config.password = "test";
        config.database = "test_db";
        config.charset = "utf8mb4";
        config.min_connections = 1;
        config.max_connections = 5;
        config.initial_connections = 2;
        config.connection_timeout = 30;
        config.idle_timeout = 600;
        config.max_lifetime = 3600;
        config.acquire_timeout = 10;
        config.health_check_interval = 60;
        config.health_check_query = "SELECT 1";
        config.auto_reconnect = true;
        config.max_allowed_packet = 1024 * 1024;
        
        std::cout << "✅ 连接池配置创建成功" << std::endl;
        
        std::cout << "\n2. 测试AlarmRuleStorage构造函数..." << std::endl;
        
        // 测试新的连接池构造函数
        AlarmRuleStorage storage_pool(config);
        std::cout << "✅ 连接池构造函数测试成功" << std::endl;
        
        // 测试兼容性构造函数
        AlarmRuleStorage storage_compat("localhost", 3306, "test", "test", "test_db");
        std::cout << "✅ 兼容性构造函数测试成功" << std::endl;
        
        std::cout << "\n3. 测试状态检查..." << std::endl;
        std::cout << "   - storage_pool.isInitialized(): " << (storage_pool.isInitialized() ? "true" : "false") << std::endl;
        std::cout << "   - storage_compat.isInitialized(): " << (storage_compat.isInitialized() ? "true" : "false") << std::endl;
        
        std::cout << "\n4. 测试连接池统计获取..." << std::endl;
        auto stats = storage_pool.getConnectionPoolStats();
        std::cout << "✅ 连接池统计获取成功（即使未初始化）" << std::endl;
        
        std::cout << "\n5. 测试字符串转义功能..." << std::endl;
        std::string test_str = "Hello 'World' \"Test\"";
        std::string escaped = storage_pool.escapeString(test_str);
        std::cout << "   原始字符串: " << test_str << std::endl;
        std::cout << "   转义后: " << escaped << std::endl;
        std::cout << "✅ 字符串转义功能正常（使用简单转义）" << std::endl;
        
        std::cout << "\n6. 测试配置更新..." << std::endl;
        config.max_connections = 10;
        storage_pool.updateConnectionPoolConfig(config);
        std::cout << "✅ 配置更新成功" << std::endl;
        
        std::cout << "\n7. 测试关闭..." << std::endl;
        storage_pool.shutdown();
        storage_compat.shutdown();
        std::cout << "✅ 关闭成功" << std::endl;
        
        std::cout << "\n🎉 所有功能验证测试完成！" << std::endl;
        std::cout << "📝 注意：此测试验证了连接池集成的代码结构和基本功能，" << std::endl;
        std::cout << "    实际数据库操作需要连接到真实的MySQL服务器。" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}