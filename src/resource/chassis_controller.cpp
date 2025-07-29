#include "resource/chassis_controller.h"
#include "spdlog/spdlog.h"
#include <cstring>
#include <sstream>

struct ChassisController::Impl {
    std::unique_ptr<TcpClient> tcp_client;
    std::string server_host = "127.0.0.1";
    int server_port = 33000;  // 根据文档，默认端口为33000
    int timeout_seconds = 30;
    std::string operation_flag = "ETHSWB";  // 根据文档，使用"ETHSWB"
    std::string last_operation_details;

    Impl() : tcp_client(std::make_unique<TcpClient>()) {}
};

ChassisController::ChassisController() : pImpl_(std::make_unique<Impl>()) {}

ChassisController::~ChassisController() = default;

ChassisController::OperationResponse ChassisController::resetChassisBoard(
    const std::string& target_ip, 
    int slot_number, 
    int req_id) {
    return executeOperation("RESET", target_ip, {slot_number}, req_id);
}

ChassisController::OperationResponse ChassisController::resetChassisBoards(
    const std::string& target_ip, 
    const std::vector<int>& slot_numbers, 
    int req_id) {
    return executeOperation("RESET", target_ip, slot_numbers, req_id);
}

ChassisController::OperationResponse ChassisController::powerOffChassisBoard(
    const std::string& target_ip, 
    int slot_number, 
    int req_id) {
    return executeOperation("PWOFF", target_ip, {slot_number}, req_id);
}

ChassisController::OperationResponse ChassisController::powerOffChassisBoards(
    const std::string& target_ip, 
    const std::vector<int>& slot_numbers, 
    int req_id) {
    return executeOperation("PWOFF", target_ip, slot_numbers, req_id);
}

ChassisController::OperationResponse ChassisController::powerOnChassisBoard(
    const std::string& target_ip, 
    int slot_number, 
    int req_id) {
    return executeOperation("PWON", target_ip, {slot_number}, req_id);
}

ChassisController::OperationResponse ChassisController::powerOnChassisBoards(
    const std::string& target_ip, 
    const std::vector<int>& slot_numbers, 
    int req_id) {
    return executeOperation("PWON", target_ip, slot_numbers, req_id);
}

void ChassisController::setServerAddress(const std::string& host, int port) {
    pImpl_->server_host = host;
    pImpl_->server_port = port;
    spdlog::info("Chassis controller server set to {}:{}", host, port);
}

void ChassisController::setTimeout(int seconds) {
    pImpl_->timeout_seconds = seconds;
    pImpl_->tcp_client->setReadWriteTimeout(seconds);
    pImpl_->tcp_client->setConnectTimeout(seconds);
}

void ChassisController::setOperationFlag(const std::string& flag) {
    pImpl_->operation_flag = flag;
}

std::string ChassisController::getLastOperationDetails() const {
    return pImpl_->last_operation_details;
}

ChassisController::OperationResponse ChassisController::executeOperation(
    const std::string& cmd,
    const std::string& target_ip,
    const std::vector<int>& slot_numbers,
    int req_id) {
    
    OperationResponse response;
    
    try {
        // 构建操作模型
        OperationModel op_model = buildOperationModel(cmd, target_ip, slot_numbers, req_id);
        
        // 将操作模型转换为二进制数据
        TcpClient::BinaryData binary_data(
            reinterpret_cast<const uint8_t*>(&op_model),
            reinterpret_cast<const uint8_t*>(&op_model) + sizeof(OperationModel)
        );
        
        // 记录操作详情
        std::ostringstream details;
        details << "Operation: " << cmd 
                << ", Target IP: " << target_ip 
                << ", Slots: ";
        for (size_t i = 0; i < slot_numbers.size(); ++i) {
            if (i > 0) details << ",";
            details << slot_numbers[i];
        }
        details << ", Request ID: " << req_id
                << ", Server: " << pImpl_->server_host << ":" << pImpl_->server_port;
        pImpl_->last_operation_details = details.str();
        
        spdlog::info("Executing chassis operation: {}", pImpl_->last_operation_details);
        spdlog::debug("Sending binary data: {}", TcpClient::binaryToHex(binary_data));
        
        // 发送请求并接收响应
        TcpClient::BinaryData tcp_response = pImpl_->tcp_client->sendAndReceive(
            pImpl_->server_host, 
            pImpl_->server_port, 
            binary_data, 
            pImpl_->timeout_seconds
        );
        
        response.raw_response = tcp_response;
        spdlog::debug("Received response: {}", TcpClient::binaryToHex(tcp_response));
        
        // 解析响应
        std::string message;
        response.result = parseResponse(tcp_response, response.slot_results, message);
        response.message = message;
        
        if (response.result == OperationResult::SUCCESS || response.result == OperationResult::PARTIAL_SUCCESS) {
            spdlog::info("Chassis operation completed: {}", message);
        } else {
            spdlog::error("Chassis operation failed: {}", message);
        }
        
    } catch (const std::runtime_error& e) {
        response.result = OperationResult::NETWORK_ERROR;
        response.message = std::string("Network error: ") + e.what();
        spdlog::error("Chassis operation network error: {}", response.message);
        
    } catch (const std::exception& e) {
        response.result = OperationResult::UNKNOWN_ERROR;
        response.message = std::string("Unknown error: ") + e.what();
        spdlog::error("Chassis operation unknown error: {}", response.message);
    }
    
    return response;
}

OperationModel ChassisController::buildOperationModel(
    const std::string& cmd,
    const std::string& target_ip,
    const std::vector<int>& slot_numbers,
    int req_id) const {
    
    OperationModel model;
    
    // 清零结构体
    memset(&model, 0, sizeof(OperationModel));
    
    // 填充字段，确保不超出数组边界
    strncpy(model.m_strFlag, pImpl_->operation_flag.c_str(), sizeof(model.m_strFlag) - 1);
    strncpy(model.m_strIp, target_ip.c_str(), sizeof(model.m_strIp) - 1);
    strncpy(model.m_cmd, cmd.c_str(), sizeof(model.m_cmd) - 1);
    model.m_reqId = req_id;
    
    // 设置槽位数组：m_slot[x] x=槽位号，0对应1槽，11对应第12槽
    // 根据文档：m_slot[x]=1表示要操作，m_slot[x]=0表示不操作
    for (int slot_num : slot_numbers) {
        if (slot_num >= 1 && slot_num <= 12) {
            model.m_slot[slot_num - 1] = static_cast<char>(SlotStatus::REQUEST_OPERATION); // 1表示要操作
        } else {
            spdlog::warn("Invalid slot number: {}. Valid range is 1-12", slot_num);
        }
    }
    
    return model;
}

ChassisController::OperationResult ChassisController::parseResponse(
    const TcpClient::BinaryData& response, 
    std::vector<SlotResult>& slot_results,
    std::string& message) const {
    
    slot_results.clear();
    
    if (response.empty()) {
        message = "Empty response received";
        return OperationResult::INVALID_RESPONSE;
    }
    
    // 如果响应数据足够大，尝试解析为OperationModel结构
    if (response.size() >= sizeof(OperationModel)) {
        const OperationModel* response_model = 
            reinterpret_cast<const OperationModel*>(response.data());
        
        std::ostringstream msg;
        msg << "Response - Flag: " << std::string(response_model->m_strFlag, sizeof(response_model->m_strFlag))
            << ", IP: " << std::string(response_model->m_strIp, sizeof(response_model->m_strIp))
            << ", CMD: " << std::string(response_model->m_cmd, sizeof(response_model->m_cmd))
            << ", ReqID: " << response_model->m_reqId;
        
        // 解析槽位状态
        int success_count = 0;
        int failed_count = 0;
        int total_requested = 0;
        
        for (int i = 0; i < 16; ++i) {
            char slot_status = response_model->m_slot[i];
            if (slot_status != 0) { // 有操作的槽位
                SlotResult slot_result;
                slot_result.slot_number = i + 1; // 槽位号从1开始
                
                if (slot_status == static_cast<char>(SlotStatus::SUCCESS)) {
                    slot_result.status = SlotStatus::SUCCESS;
                    success_count++;
                } else if (slot_status == static_cast<char>(SlotStatus::FAILED)) {
                    slot_result.status = SlotStatus::FAILED;
                    failed_count++;
                } else if (slot_status == static_cast<char>(SlotStatus::REQUEST_OPERATION)) {
                    slot_result.status = SlotStatus::REQUEST_OPERATION;
                } else {
                    slot_result.status = SlotStatus::NO_OPERATION;
                }
                
                slot_results.push_back(slot_result);
                total_requested++;
            }
        }
        
        msg << ", Processed slots: " << total_requested 
            << ", Success: " << success_count 
            << ", Failed: " << failed_count;
        
        message = msg.str();
        
        // 根据成功/失败数量确定结果
        if (failed_count == 0 && success_count > 0) {
            return OperationResult::SUCCESS;
        } else if (success_count > 0 && failed_count > 0) {
            return OperationResult::PARTIAL_SUCCESS;
        } else if (failed_count > 0) {
            return OperationResult::INVALID_RESPONSE;
        } else {
            return OperationResult::SUCCESS; // 没有明确失败的情况认为成功
        }
    }
    
    // 如果不能解析为结构体，尝试作为字符串处理
    message = TcpClient::binaryToString(response);
    
    // 检查常见的成功响应
    if (message.find("OK") != std::string::npos || 
        message.find("SUCCESS") != std::string::npos ||
        message.find("COMPLETED") != std::string::npos) {
        return OperationResult::SUCCESS;
    }
    
    // 检查常见的错误响应
    if (message.find("ERROR") != std::string::npos ||
        message.find("FAILED") != std::string::npos ||
        message.find("TIMEOUT") != std::string::npos) {
        return OperationResult::INVALID_RESPONSE;
    }
    
    // 默认认为是成功的
    return OperationResult::SUCCESS;
}