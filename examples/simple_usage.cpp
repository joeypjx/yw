#include "alarm_system.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "========== 告警系统库使用示例 ==========" << std::endl;
    
    // 1. 创建自定义配置
    AlarmSystemConfig config;
    config.stats_interval = std::chrono::seconds(30); // 30秒输出一次统计
    
    // 2. 创建告警系统实例
    AlarmSystem alarm_system(config);
    
    // 3. 设置告警事件回调函数
    alarm_system.setAlarmEventCallback([](const AlarmEvent& event) {
        std::cout << "📨 收到告警事件: " << event.fingerprint 
                  << " 状态: " << event.status << std::endl;
    });
    
    try {
        // 4. 初始化并启动系统
        std::cout << "⏳ 正在初始化并启动告警系统..." << std::endl;
        if (!alarm_system.initialize()) {
            std::cerr << "❌ 初始化失败: " << alarm_system.getLastError() << std::endl;
            return 1;
        }
        std::cout << "✅ 系统初始化并启动成功" << std::endl;
        
        // 5. 运行一段时间并监控状态
        std::cout << "🔄 系统运行中，将运行2分钟..." << std::endl;
        for (int i = 0; i < 4; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            
            auto stats = alarm_system.getStats();
            std::cout << "\n📊 [" << (i+1)*30 << "秒] 系统统计:" << std::endl;
            std::cout << "   运行时间: " << stats.uptime.count() << " 秒" << std::endl;
            std::cout << "   状态: " << static_cast<int>(stats.status) << std::endl;
            std::cout << "   活跃告警: " << stats.active_alarms << std::endl;
            std::cout << "   总告警数: " << stats.total_alarms << std::endl;
            std::cout << "   触发事件: " << stats.firing_events << std::endl;
            std::cout << "   恢复事件: " << stats.resolved_events << std::endl;
        }
        
        // 6. 停止系统
        std::cout << "\n🛑 正在停止系统..." << std::endl;
        alarm_system.stop();
        std::cout << "✅ 系统已停止" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 系统异常: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "========== 示例程序结束 ==========" << std::endl;
    return 0;
} 