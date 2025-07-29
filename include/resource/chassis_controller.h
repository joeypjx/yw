#ifndef CHASSIS_CONTROLLER_H
#define CHASSIS_CONTROLLER_H

#include "tcp_client.h"
#include "operation_model.h"
#include <string>
#include <memory>

class ChassisController {
public:
    enum class OperationResult {
        SUCCESS,
        PARTIAL_SUCCESS,
        NETWORK_ERROR,
        TIMEOUT_ERROR,
        INVALID_RESPONSE,
        UNKNOWN_ERROR
    };

    enum class SlotStatus {
        NO_OPERATION = 0,    // 不操作
        REQUEST_OPERATION = 1, // 请求操作
        SUCCESS = 2,         // 操作成功
        FAILED = -1          // 操作失败
    };

    struct SlotResult {
        int slot_number;     // 槽位号 (1-12)
        SlotStatus status;   // 槽位状态
    };

    struct OperationResponse {
        OperationResult result;
        std::string message;
        std::vector<SlotResult> slot_results; // 各槽位的操作结果
        TcpClient::BinaryData raw_response;
    };

    ChassisController();
    ~ChassisController();

    // 复位单个槽位的机箱板卡
    OperationResponse resetChassisBoard(const std::string& target_ip, 
                                       int slot_number, 
                                       int req_id = 0);

    // 复位多个槽位的机箱板卡
    OperationResponse resetChassisBoards(const std::string& target_ip, 
                                        const std::vector<int>& slot_numbers, 
                                        int req_id = 0);

    // 下电单个槽位的机箱板卡
    OperationResponse powerOffChassisBoard(const std::string& target_ip, 
                                          int slot_number, 
                                          int req_id = 0);

    // 下电多个槽位的机箱板卡
    OperationResponse powerOffChassisBoards(const std::string& target_ip, 
                                           const std::vector<int>& slot_numbers, 
                                           int req_id = 0);

    // 上电单个槽位的机箱板卡
    OperationResponse powerOnChassisBoard(const std::string& target_ip, 
                                         int slot_number, 
                                         int req_id = 0);

    // 上电多个槽位的机箱板卡
    OperationResponse powerOnChassisBoards(const std::string& target_ip, 
                                          const std::vector<int>& slot_numbers, 
                                          int req_id = 0);

    // 设置服务器地址和端口
    void setServerAddress(const std::string& host, int port);

    // 设置操作超时时间
    void setTimeout(int seconds);

    // 设置操作标识符
    void setOperationFlag(const std::string& flag);

    // 获取最后一次操作的详细信息
    std::string getLastOperationDetails() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;

    // 内部方法：执行操作
    OperationResponse executeOperation(const std::string& cmd,
                                     const std::string& target_ip,
                                     const std::vector<int>& slot_numbers,
                                     int req_id);

    // 内部方法：构建操作模型
    OperationModel buildOperationModel(const std::string& cmd,
                                      const std::string& target_ip,
                                      const std::vector<int>& slot_numbers,
                                      int req_id) const;

    // 内部方法：解析响应
    OperationResult parseResponse(const TcpClient::BinaryData& response, 
                                 std::vector<SlotResult>& slot_results,
                                 std::string& message) const;
};

#endif // CHASSIS_CONTROLLER_H