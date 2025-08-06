#include "component_status_monitor.h"
#include "node_storage.h"
#include "alarm_manager.h"
#include "log_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

// 模拟组件状态变化回调函数
void onComponentStatusChange(const std::string& host_ip, 
                           const std::string& instance_id, 
                           const std::string& uuid,
                           int index,
                           const std::string& old_state, 
                           const std::string& new_state) {
    std::cout << "Component status change detected:" << std::endl;
    std::cout << "  Host IP: " << host_ip << std::endl;
    std::cout << "  Instance ID: " << instance_id << std::endl;
    std::cout << "  UUID: " << uuid << std::endl;
    std::cout << "  Index: " << index << std::endl;
    std::cout << "  State change: " << old_state << " -> " << new_state << std::endl;
    std::cout << "----------------------------------------" << std::endl;
}

int main() {
    // 初始化日志管理器
    LogManager::init("component_monitor_test.log", "debug");
    
    // 创建依赖组件
    auto node_storage = std::make_shared<NodeStorage>();
    auto alarm_manager = std::make_shared<AlarmManager>();
    
    // 创建组件状态监控器
    auto component_monitor = std::make_shared<ComponentStatusMonitor>(node_storage, alarm_manager);
    
    // 设置回调函数
    component_monitor->setComponentStatusChangeCallback(onComponentStatusChange);
    
    // 设置检查间隔为10秒（用于测试）
    component_monitor->setCheckInterval(std::chrono::seconds(10));
    
    // 设置FAILED状态阈值为30秒（用于测试）
    component_monitor->setFailedThreshold(std::chrono::seconds(30));
    
    // 启动监控器
    component_monitor->start();
    
    std::cout << "Component Status Monitor started." << std::endl;
    std::cout << "Check interval: " << component_monitor->getCheckInterval().count() << " seconds" << std::endl;
    std::cout << "Failed threshold: " << component_monitor->getFailedThreshold().count() << " seconds" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 模拟添加一些测试数据
    std::cout << "Adding test nodes with components..." << std::endl;
    
    // 创建测试节点1
    auto node1 = std::make_shared<NodeData>();
    node1->host_ip = "192.168.1.100";
    node1->hostname = "test-node-1";
    node1->box_id = 1;
    node1->slot_id = 1;
    
    // 添加组件到节点1
    node::ComponentInfo comp1;
    comp1.instance_id = "instance-001";
    comp1.uuid = "uuid-001";
    comp1.index = 0;
    comp1.name = "web-service";
    comp1.id = "web-001";
    comp1.state = "RUNNING";
    
    node::ComponentInfo comp2;
    comp2.instance_id = "instance-002";
    comp2.uuid = "uuid-002";
    comp2.index = 1;
    comp2.name = "database-service";
    comp2.id = "db-001";
    comp2.state = "FAILED";  // 这个组件是FAILED状态
    
    node1->component.push_back(comp1);
    node1->component.push_back(comp2);
    
    // 创建测试节点2
    auto node2 = std::make_shared<NodeData>();
    node2->host_ip = "192.168.1.101";
    node2->hostname = "test-node-2";
    node2->box_id = 1;
    node2->slot_id = 2;
    
    // 添加组件到节点2
    node::ComponentInfo comp3;
    comp3.instance_id = "instance-003";
    comp3.uuid = "uuid-003";
    comp3.index = 0;
    comp3.name = "cache-service";
    comp3.id = "cache-001";
    comp3.state = "PENDING";
    
    node2->component.push_back(comp3);
    
    // 将节点添加到存储中
    node_storage->storeBoxInfo(node::BoxInfo{
        node1->box_id, node1->slot_id, node1->cpu_id, node1->srio_id,
        node1->host_ip, node1->hostname, node1->service_port,
        node1->box_type, node1->board_type, node1->cpu_type,
        node1->os_type, node1->resource_type, node1->cpu_arch
    });
    
    node_storage->storeBoxInfo(node::BoxInfo{
        node2->box_id, node2->slot_id, node2->cpu_id, node2->srio_id,
        node2->host_ip, node2->hostname, node2->service_port,
        node2->box_type, node2->board_type, node2->cpu_type,
        node2->os_type, node2->resource_type, node2->cpu_arch
    });
    
    // 存储组件信息
    std::vector<node::ComponentInfo> components1 = {comp1, comp2};
    std::vector<node::ComponentInfo> components2 = {comp3};
    
    node_storage->storeComponentInfo(node1->host_ip, components1);
    node_storage->storeComponentInfo(node2->host_ip, components2);
    
    std::cout << "Test data added. Monitoring for 60 seconds..." << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // 运行60秒，观察监控结果
    for (int i = 0; i < 6; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::cout << "Monitoring... (" << (i + 1) * 10 << " seconds elapsed)" << std::endl;
        
        // 模拟状态变化（在第30秒时改变一个组件的状态）
        if (i == 2) {  // 30秒后
            std::cout << "Simulating component state change..." << std::endl;
            
            // 将web-service的状态从RUNNING改为FAILED
            comp1.state = "FAILED";
            std::vector<node::ComponentInfo> updated_components1 = {comp1, comp2};
            node_storage->storeComponentInfo(node1->host_ip, updated_components1);
            
            std::cout << "Changed web-service state from RUNNING to FAILED" << std::endl;
        }
        
        // 在第50秒时恢复状态
        if (i == 4) {  // 50秒后
            std::cout << "Simulating component recovery..." << std::endl;
            
            // 将database-service的状态从FAILED改为RUNNING
            comp2.state = "RUNNING";
            std::vector<node::ComponentInfo> updated_components1 = {comp1, comp2};
            node_storage->storeComponentInfo(node1->host_ip, updated_components1);
            
            std::cout << "Changed database-service state from FAILED to RUNNING" << std::endl;
        }
    }
    
    // 停止监控器
    component_monitor->stop();
    
    std::cout << "Component Status Monitor stopped." << std::endl;
    std::cout << "Test completed." << std::endl;
    
    return 0;
} 