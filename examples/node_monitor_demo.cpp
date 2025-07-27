#include "alarm_system.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

/**
 * 节点状态监控演示程序
 * 
 * 演示功能：
 * 1. 启动告警系统
 * 2. 模拟节点心跳
 * 3. 观察节点离线检测
 * 4. 观察告警触发和解决
 */
class NodeMonitorDemo {
private:
    AlarmSystem alarm_system_;
    std::atomic<bool> running_{false};
    std::thread demo_thread_;
    
public:
    NodeMonitorDemo() {
        // 配置告警系统
        AlarmSystemConfig config;
        config.http_port = 8080;
        config.mysql_host = "127.0.0.1";
        config.mysql_port = 3306;
        config.db_user = "test";
        config.db_password = "HZ715Net";
        config.resource_db = "resource";
        config.alarm_db = "alarm";
        
        alarm_system_ = AlarmSystem(config);
        
        // 设置告警事件回调
        alarm_system_.setAlarmEventCallback([this](const AlarmEvent& event) {
            handleAlarmEvent(event);
        });
    }
    
    bool start() {
        std::cout << "🚀 启动节点状态监控演示..." << std::endl;
        
        if (!alarm_system_.initialize()) {
            std::cerr << "❌ 告警系统初始化失败: " << alarm_system_.getLastError() << std::endl;
            return false;
        }
        
        running_ = true;
        demo_thread_ = std::thread(&NodeMonitorDemo::demoLoop, this);
        
        std::cout << "✅ 演示程序启动成功" << std::endl;
        return true;
    }
    
    void stop() {
        std::cout << "🛑 停止演示程序..." << std::endl;
        
        running_ = false;
        if (demo_thread_.joinable()) {
            demo_thread_.join();
        }
        
        alarm_system_.stop();
        std::cout << "✅ 演示程序已停止" << std::endl;
    }
    
    void run() {
        if (!start()) {
            return;
        }
        
        std::cout << "\n📋 演示流程：" << std::endl;
        std::cout << "1. 发送节点心跳" << std::endl;
        std::cout << "2. 等待节点离线检测（5秒）" << std::endl;
        std::cout << "3. 观察告警触发" << std::endl;
        std::cout << "4. 重新发送心跳" << std::endl;
        std::cout << "5. 观察告警解决" << std::endl;
        std::cout << "\n💡 按 Enter 键开始演示..." << std::endl;
        
        std::cin.get();
        
        // 开始演示
        runDemo();
        
        std::cout << "\n💡 按 Enter 键退出..." << std::endl;
        std::cin.get();
        
        stop();
    }
    
private:
    void demoLoop() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    void runDemo() {
        std::cout << "\n🎬 开始演示..." << std::endl;
        
        // 步骤1: 发送节点心跳
        std::cout << "\n1️⃣ 发送节点心跳..." << std::endl;
        sendHeartbeat();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // 步骤2: 检查节点状态
        std::cout << "\n2️⃣ 检查节点状态..." << std::endl;
        checkNodeStatus();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 步骤3: 等待离线检测
        std::cout << "\n3️⃣ 等待节点离线检测（5秒）..." << std::endl;
        for (int i = 5; i > 0; --i) {
            std::cout << "⏰ " << i << " 秒后检测离线..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // 步骤4: 检查告警
        std::cout << "\n4️⃣ 检查告警事件..." << std::endl;
        checkAlarmEvents();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // 步骤5: 重新发送心跳
        std::cout << "\n5️⃣ 重新发送心跳..." << std::endl;
        sendHeartbeat();
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // 步骤6: 检查告警解决
        std::cout << "\n6️⃣ 检查告警解决..." << std::endl;
        checkAlarmEvents();
        
        std::cout << "\n✅ 演示完成！" << std::endl;
    }
    
    void sendHeartbeat() {
        // 这里应该发送HTTP请求，但为了演示简单，我们只打印信息
        std::cout << "💓 发送心跳到节点 test-node-001 (192.168.1.100)" << std::endl;
        
        // 实际应用中，这里会发送HTTP请求：
        // curl -X POST http://localhost:8080/heartbeat -H "Content-Type: application/json" -d '...'
    }
    
    void checkNodeStatus() {
        std::cout << "🔍 检查节点状态..." << std::endl;
        std::cout << "   - 节点: test-node-001" << std::endl;
        std::cout << "   - IP: 192.168.1.100" << std::endl;
        std::cout << "   - 状态: 在线" << std::endl;
        
        // 实际应用中，这里会发送HTTP请求：
        // curl http://localhost:8080/api/v1/nodes/list
    }
    
    void checkAlarmEvents() {
        std::cout << "🚨 检查告警事件..." << std::endl;
        
        // 实际应用中，这里会发送HTTP请求：
        // curl http://localhost:8080/api/v1/alarm/events
        
        // 模拟检查结果
        std::cout << "   - 当前告警数量: 1" << std::endl;
        std::cout << "   - 告警类型: NodeOffline" << std::endl;
        std::cout << "   - 告警状态: firing" << std::endl;
        std::cout << "   - 告警描述: Node 192.168.1.100 has not sent a heartbeat for more than 5 seconds." << std::endl;
    }
    
    void handleAlarmEvent(const AlarmEvent& event) {
        std::cout << "\n🔔 收到告警事件:" << std::endl;
        std::cout << "   - 指纹: " << event.fingerprint << std::endl;
        std::cout << "   - 状态: " << event.status << std::endl;
        std::cout << "   - 摘要: " << event.annotations.at("summary") << std::endl;
        std::cout << "   - 描述: " << event.annotations.at("description") << std::endl;
    }
};

int main() {
    std::cout << "🎯 节点状态监控演示程序" << std::endl;
    std::cout << "================================" << std::endl;
    
    NodeMonitorDemo demo;
    demo.run();
    
    return 0;
} 