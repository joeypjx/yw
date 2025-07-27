#include "alarm_system.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

/**
 * 模拟一个现有的应用程序，集成告警系统
 */
class MyApplication {
private:
    AlarmSystem alarm_system_;
    std::thread app_thread_;
    std::atomic<bool> running_{false};
    
    // 告警事件队列
    std::queue<AlarmEvent> alarm_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
public:
    explicit MyApplication(const AlarmSystemConfig& config) : alarm_system_(config) {
        // 设置告警事件处理回调
        alarm_system_.setAlarmEventCallback([this](const AlarmEvent& event) {
            handleAlarmEvent(event);
        });
    }
    
    bool start() {
        std::cout << "🚀 启动应用程序..." << std::endl;
        
        if (!alarm_system_.initialize()) {
            std::cerr << "❌ 告警系统初始化失败: " << alarm_system_.getLastError() << std::endl;
            return false;
        }
        
        running_ = true;
        app_thread_ = std::thread(&MyApplication::applicationLoop, this);
        
        std::cout << "✅ 应用程序启动成功" << std::endl;
        return true;
    }
    
    void stop() {
        std::cout << "🛑 停止应用程序..." << std::endl;
        
        running_ = false;
        queue_cv_.notify_all();
        
        if (app_thread_.joinable()) {
            app_thread_.join();
        }
        
        alarm_system_.stop();
        std::cout << "✅ 应用程序已停止" << std::endl;
    }
    
    void run() {
        if (!start()) {
            return;
        }
        
        // 模拟应用程序运行
        std::cout << "🔄 应用程序运行中..." << std::endl;
        std::cout << "💡 按 Enter 键停止程序" << std::endl;
        
        std::cin.get(); // 等待用户输入
        
        stop();
    }
    
    AlarmSystemStats getSystemStats() const {
        return alarm_system_.getStats();
    }
    
private:
    void handleAlarmEvent(const AlarmEvent& event) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        alarm_queue_.push(event);
        queue_cv_.notify_one();
    }
    
    void applicationLoop() {
        std::cout << "🔄 应用程序主循环启动" << std::endl;
        
        while (running_) {
            // 处理告警事件
            processAlarmEvents();
            
            // 执行其他应用程序逻辑
            performBusinessLogic();
            
            // 定期输出状态
            static int counter = 0;
            if (++counter % 6 == 0) { // 每30秒输出一次
                printApplicationStatus();
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        
        std::cout << "🛑 应用程序主循环已停止" << std::endl;
    }
    
    void processAlarmEvents() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        while (!alarm_queue_.empty()) {
            AlarmEvent event = alarm_queue_.front();
            alarm_queue_.pop();
            lock.unlock();
            
            // 处理告警事件的业务逻辑
            handleBusinessAlarmLogic(event);
            
            lock.lock();
        }
    }
    
    void handleBusinessAlarmLogic(const AlarmEvent& event) {
        std::cout << "\n🔔 业务层处理告警事件:" << std::endl;
        std::cout << "   指纹: " << event.fingerprint << std::endl;
        std::cout << "   状态: " << event.status << std::endl;
        
        if (event.status == "firing") {
            // 告警触发时的业务逻辑
            std::cout << "   ⚠️  执行告警响应策略..." << std::endl;
            
            // 例如：发送邮件、短信、钉钉通知等
            sendNotification(event);
            
            // 例如：记录到业务日志
            logToBusinessSystem(event);
            
            // 例如：触发自动修复流程
            triggerAutoRepair(event);
            
        } else if (event.status == "resolved") {
            // 告警恢复时的业务逻辑
            std::cout << "   ✅ 执行告警恢复策略..." << std::endl;
            
            // 例如：发送恢复通知
            sendRecoveryNotification(event);
        }
    }
    
    void sendNotification(const AlarmEvent& event) {
        std::cout << "     📧 发送告警通知 (邮件/短信/钉钉)" << std::endl;
        // 实际实现：调用通知服务API
    }
    
    void logToBusinessSystem(const AlarmEvent& event) {
        std::cout << "     📝 记录到业务系统日志" << std::endl;
        // 实际实现：写入业务数据库或日志系统
    }
    
    void triggerAutoRepair(const AlarmEvent& event) {
        std::cout << "     🔧 触发自动修复流程" << std::endl;
        // 实际实现：调用自动化运维工具
    }
    
    void sendRecoveryNotification(const AlarmEvent& event) {
        std::cout << "     📤 发送恢复通知" << std::endl;
        // 实际实现：发送恢复通知
    }
    
    void performBusinessLogic() {
        // 模拟其他业务逻辑
        static int task_counter = 0;
        if (++task_counter % 12 == 0) { // 每分钟输出一次
            std::cout << "⚙️  执行业务任务 #" << task_counter/12 << std::endl;
        }
    }
    
    void printApplicationStatus() {
        auto stats = alarm_system_.getStats();
        std::cout << "\n📊 应用程序状态报告:" << std::endl;
        std::cout << "   ⏱️  运行时间: " << stats.uptime.count() << " 秒" << std::endl;
        std::cout << "   🚨 活跃告警: " << stats.active_alarms << std::endl;
        std::cout << "   📈 总告警数: " << stats.total_alarms << std::endl;
        std::cout << "   🔥 触发事件: " << stats.firing_events << std::endl;
        std::cout << "   ✅恢复事件: " << stats.resolved_events << std::endl;
        std::cout << "   💾 告警实例: " << stats.alarm_instances << std::endl;
    }
};

int main() {
    std::cout << "========== 告警系统集成示例 ==========" << std::endl;
    
    // 配置告警系统
    AlarmSystemConfig config;
    config.stats_interval = std::chrono::seconds(60);
    config.evaluation_interval = std::chrono::seconds(5);
    
    // 创建并运行应用程序
    MyApplication app(config);
    
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "❌ 应用程序异常: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "========== 集成示例程序结束 ==========" << std::endl;
    return 0;
} 