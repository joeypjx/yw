#include "alarm_rule_storage.h"
#include "log_manager.h"
#include <iostream>
#include <vector>

int main() {
    std::cout << "=== AlarmRuleStorage 连接池集成测试 ===" << std::endl;
    
    try {
        // 初始化日志系统（使用静态方法）
        // LogManager::getLogger()->set_level(spdlog::level::debug);
        std::cout << "日志系统已准备就绪" << std::endl;
        
        std::cout << "\n1. 测试兼容性构造函数..." << std::endl;
        AlarmRuleStorage storage("localhost", 3306, "root", "password", "test_alarm_rules");
        
        std::cout << "2. 测试初始化..." << std::endl;
        if (!storage.initialize()) {
            std::cerr << "❌ 初始化失败" << std::endl;
            return 1;
        }
        std::cout << "✅ 初始化成功" << std::endl;
        
        std::cout << "\n3. 测试创建数据库..." << std::endl;
        if (!storage.createDatabase()) {
            std::cerr << "❌ 创建数据库失败" << std::endl;
        } else {
            std::cout << "✅ 数据库创建成功" << std::endl;
        }
        
        std::cout << "\n4. 测试创建表..." << std::endl;
        if (!storage.createTable()) {
            std::cerr << "❌ 创建表失败" << std::endl;
        } else {
            std::cout << "✅ 表创建成功" << std::endl;
        }
        
        std::cout << "\n5. 测试连接池统计..." << std::endl;
        auto stats = storage.getConnectionPoolStats();
        std::cout << "📊 连接池统计信息:" << std::endl;
        std::cout << "   - 总连接数: " << stats.total_connections << std::endl;
        std::cout << "   - 活跃连接数: " << stats.active_connections << std::endl;
        std::cout << "   - 空闲连接数: " << stats.idle_connections << std::endl;
        std::cout << "   - 等待请求数: " << stats.pending_requests << std::endl;
        std::cout << "   - 已创建连接数: " << stats.created_connections << std::endl;
        std::cout << "   - 已销毁连接数: " << stats.destroyed_connections << std::endl;
        std::cout << "   - 平均等待时间: " << stats.average_wait_time << "ms" << std::endl;
        
        std::cout << "\n6. 测试数据操作..." << std::endl;
        
        // 创建测试规则的JSON表达式
        nlohmann::json expression;
        expression["metric"] = "cpu_usage";
        expression["operator"] = ">";
        expression["threshold"] = 90;
        
        // 测试插入告警规则
        std::string rule_id = storage.insertAlarmRule(
            "high_cpu_usage",
            expression,
            "5m",
            "critical",
            "CPU使用率过高",
            "CPU使用率超过90%，持续5分钟",
            "硬件状态",
            true
        );
        
        if (!rule_id.empty()) {
            std::cout << "✅ 告警规则插入成功，ID: " << rule_id << std::endl;
            
            // 测试查询单个规则
            auto rule = storage.getAlarmRule(rule_id);
            if (!rule.id.empty()) {
                std::cout << "✅ 规则查询成功: " << rule.alert_name << std::endl;
            }
            
            // 测试查询所有规则
            auto all_rules = storage.getAllAlarmRules();
            std::cout << "✅ 查询到 " << all_rules.size() << " 条告警规则" << std::endl;
            
            // 测试分页查询
            auto paginated = storage.getPaginatedAlarmRules(1, 10, false);
            std::cout << "✅ 分页查询成功，共 " << paginated.total_count << " 条记录，" 
                      << paginated.total_pages << " 页" << std::endl;
            
            // 测试更新规则
            expression["threshold"] = 95;
            if (storage.updateAlarmRule(rule_id, "high_cpu_usage_updated", expression, 
                                       "10m", "warning", "CPU使用率更新", "更新的描述", "硬件状态", true)) {
                std::cout << "✅ 规则更新成功" << std::endl;
            }
            
            // 测试删除规则
            if (storage.deleteAlarmRule(rule_id)) {
                std::cout << "✅ 规则删除成功" << std::endl;
            }
        }
        
        std::cout << "\n7. 最终连接池统计..." << std::endl;
        stats = storage.getConnectionPoolStats();
        std::cout << "📊 最终统计:" << std::endl;
        std::cout << "   - 总连接数: " << stats.total_connections << std::endl;
        std::cout << "   - 活跃连接数: " << stats.active_connections << std::endl;
        std::cout << "   - 空闲连接数: " << stats.idle_connections << std::endl;
        
        std::cout << "\n8. 测试关闭..." << std::endl;
        storage.shutdown();
        std::cout << "✅ AlarmRuleStorage 关闭成功" << std::endl;
        
        std::cout << "\n🎉 所有测试完成！" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}