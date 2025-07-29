#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <string>
#include <memory>
#include <chrono>
#include <functional>
#include <vector>
#include <cstdint>

class TcpClient {
public:
    using BinaryData = std::vector<uint8_t>;
    
    TcpClient();
    ~TcpClient();

    // 同步发送二进制数据并接收响应
    BinaryData sendAndReceive(const std::string& host, int port, 
                             const BinaryData& data, 
                             int timeout_seconds = 30);

    // 异步发送二进制数据并接收响应
    void sendAndReceiveAsync(const std::string& host, int port,
                           const BinaryData& data,
                           std::function<void(const std::string&, const BinaryData&)> callback,
                           int timeout_seconds = 30);

    // 便利方法：发送字符串数据（内部转换为二进制）
    BinaryData sendAndReceiveString(const std::string& host, int port,
                                   const std::string& data,
                                   int timeout_seconds = 30);

    // 便利方法：发送C风格数据
    BinaryData sendAndReceive(const std::string& host, int port,
                             const void* data, size_t length,
                             int timeout_seconds = 30);

    // 检查连接状态
    bool isConnected() const;

    // 设置连接超时
    void setConnectTimeout(int seconds);

    // 设置读写超时
    void setReadWriteTimeout(int seconds);

    // 静态辅助方法：将二进制数据转换为字符串
    static std::string binaryToString(const BinaryData& data);
    
    // 静态辅助方法：将字符串转换为二进制数据
    static BinaryData stringToBinary(const std::string& str);
    
    // 静态辅助方法：将二进制数据转换为十六进制字符串（用于调试）
    static std::string binaryToHex(const BinaryData& data);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

#endif // TCP_CLIENT_H