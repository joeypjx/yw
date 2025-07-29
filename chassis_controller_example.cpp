#include "resource/chassis_controller.h"
#include "spdlog/spdlog.h"
#include <iostream>

std::string getResultString(ChassisController::OperationResult result) {
    switch (result) {
        case ChassisController::OperationResult::SUCCESS:
            return "完全成功";
        case ChassisController::OperationResult::PARTIAL_SUCCESS:
            return "部分成功";
        case ChassisController::OperationResult::NETWORK_ERROR:
            return "网络错误";
        case ChassisController::OperationResult::TIMEOUT_ERROR:
            return "超时错误";
        case ChassisController::OperationResult::INVALID_RESPONSE:
            return "无效响应";
        default:
            return "未知错误";
    }
}

std::string getSlotStatusString(ChassisController::SlotStatus status) {
    switch (status) {
        case ChassisController::SlotStatus::NO_OPERATION:
            return "无操作";
        case ChassisController::SlotStatus::REQUEST_OPERATION:
            return "请求操作";
        case ChassisController::SlotStatus::SUCCESS:
            return "操作成功";
        case ChassisController::SlotStatus::FAILED:
            return "操作失败";
        default:
            return "未知状态";
    }
}

void printSlotResults(const std::vector<ChassisController::SlotResult>& slot_results) {
    if (slot_results.empty()) {
        std::cout << "  没有槽位操作结果" << std::endl;
        return;
    }
    
    std::cout << "  槽位操作结果:" << std::endl;
    for (const auto& slot : slot_results) {
        std::cout << "    槽位 " << slot.slot_number << ": " 
                  << getSlotStatusString(slot.status) << std::endl;
    }
}

int main() {
    try {
        // 创建机箱控制器实例
        ChassisController controller;
        
        // 配置服务器地址和端口（根据文档使用33000端口）
        controller.setServerAddress("192.168.1.180", 33000);
        controller.setTimeout(30);
        controller.setOperationFlag("ETHSWB");
        
        std::cout << "=== 机箱板卡控制示例（按power.md规范） ===" << std::endl;
        
        // 示例1：复位单个槽位的机箱板卡
        std::cout << "\n1. 复位槽位3的机箱板卡..." << std::endl;
        auto reset_result = controller.resetChassisBoard("192.168.1.180", 3, 1001);
        
        std::cout << "复位操作结果: " << getResultString(reset_result.result) << std::endl;
        std::cout << "响应消息: " << reset_result.message << std::endl;
        printSlotResults(reset_result.slot_results);
        std::cout << "响应数据(hex): " << TcpClient::binaryToHex(reset_result.raw_response) << std::endl;
        
        // 示例2：下电多个槽位的机箱板卡
        std::cout << "\n2. 下电槽位1,2,4的机箱板卡..." << std::endl;
        std::vector<int> poweroff_slots = {1, 2, 4};
        auto poweroff_result = controller.powerOffChassisBoards("192.168.1.180", poweroff_slots, 1002);
        
        std::cout << "下电操作结果: " << getResultString(poweroff_result.result) << std::endl;
        std::cout << "响应消息: " << poweroff_result.message << std::endl;
        printSlotResults(poweroff_result.slot_results);
        
        // 示例3：上电多个槽位的机箱板卡
        std::cout << "\n3. 上电槽位1,2的机箱板卡..." << std::endl;
        std::vector<int> poweron_slots = {1, 2};
        auto poweron_result = controller.powerOnChassisBoards("192.168.1.180", poweron_slots, 1003);
        
        std::cout << "上电操作结果: " << getResultString(poweron_result.result) << std::endl;
        std::cout << "响应消息: " << poweron_result.message << std::endl;
        printSlotResults(poweron_result.slot_results);
        
        // 示例4：演示协议格式
        std::cout << "\n4. 协议格式演示..." << std::endl;
        std::cout << "根据power.md文档:" << std::endl;
        std::cout << "- m_strFlag: \"ETHSWB\\0\"" << std::endl;
        std::cout << "- m_strIp: 交换模块IP地址" << std::endl;
        std::cout << "- m_cmd: \"RESET\\0\", \"PWOFF\\0\", \"PWON\\0\"" << std::endl;
        std::cout << "- m_slot[16]: 槽位数组，0对应1槽，11对应12槽" << std::endl;
        std::cout << "  请求时: 1=要操作, 0=不操作" << std::endl;
        std::cout << "  响应时: 2=操作成功, -1=操作失败" << std::endl;
        std::cout << "- m_reqId: 请求序号" << std::endl;
        
        // 显示最后一次操作的详细信息
        std::cout << "\n最后操作详情: " << controller.getLastOperationDetails() << std::endl;
        
        std::cout << "\n=== 机箱板卡控制示例完成 ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "程序异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}