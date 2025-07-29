#include "resource/tcp_client.h"
#include "spdlog/spdlog.h"
#include <thread>
#include <mutex>
#include <future>
#include <vector>
#include <cstdio>

#include <asio.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>
#include <asio/read.hpp>

using namespace asio;
using namespace asio::ip;

struct TcpClient::Impl {
    io_context io_context;
    std::unique_ptr<tcp::socket> socket;
    int connect_timeout_seconds = 10;
    int read_write_timeout_seconds = 30;
    bool connected = false;
    std::mutex mutex;

    Impl() : socket(std::make_unique<tcp::socket>(io_context)) {}

    ~Impl() {
        disconnect();
    }

    void disconnect() {
        std::lock_guard<std::mutex> lock(mutex);
        if (socket && socket->is_open()) {
            error_code ec;
            socket->close(ec);
            connected = false;
        }
    }

    bool connect(const std::string& host, int port) {
        try {
            disconnect(); // 确保之前的连接已关闭
            
            // 重新创建socket
            socket = std::make_unique<tcp::socket>(io_context);
            
            // 解析主机名和端口
            tcp::resolver resolver(io_context);
            auto endpoints = resolver.resolve(host, std::to_string(port));

            // 同步连接
            socket->connect(*endpoints.begin());
            
            std::lock_guard<std::mutex> lock(mutex);
            connected = true;
            spdlog::info("Successfully connected to {}:{}", host, port);
            return true;

        } catch (const std::exception& e) {
            spdlog::error("TCP connection error: {}", e.what());
            connected = false;
            return false;
        }
    }

    TcpClient::BinaryData sendData(const TcpClient::BinaryData& data) {
        if (!connected || !socket || !socket->is_open()) {
            throw std::runtime_error("Not connected to server");
        }

        try {
            // 发送二进制数据
            std::size_t bytes_sent = asio::write(*socket, asio::buffer(data));
            spdlog::debug("Sent {} bytes", bytes_sent);

            // 接收响应 - 同步方式
            TcpClient::BinaryData response_buffer(4096); // 增大缓冲区
            std::size_t bytes_received = socket->read_some(asio::buffer(response_buffer));
            
            // 调整响应大小到实际接收的字节数
            response_buffer.resize(bytes_received);

            spdlog::debug("Received {} bytes of binary data", bytes_received);
            return response_buffer;

        } catch (const std::exception& e) {
            spdlog::error("TCP send/receive error: {}", e.what());
            disconnect();
            throw;
        }
    }
};

TcpClient::TcpClient() : pImpl_(std::make_unique<Impl>()) {}

TcpClient::~TcpClient() = default;

TcpClient::BinaryData TcpClient::sendAndReceive(const std::string& host, int port,
                                                const BinaryData& data,
                                                int timeout_seconds) {
    pImpl_->read_write_timeout_seconds = timeout_seconds;
    
    if (!pImpl_->connect(host, port)) {
        throw std::runtime_error("Failed to connect to " + host + ":" + std::to_string(port));
    }

    try {
        BinaryData response = pImpl_->sendData(data);
        pImpl_->disconnect(); // 发送完成后断开连接
        return response;
    } catch (const std::exception& e) {
        pImpl_->disconnect();
        throw;
    }
}

void TcpClient::sendAndReceiveAsync(const std::string& host, int port,
                                   const BinaryData& data,
                                   std::function<void(const std::string&, const BinaryData&)> callback,
                                   int timeout_seconds) {
    std::thread([this, host, port, data, callback, timeout_seconds]() {
        try {
            BinaryData response = sendAndReceive(host, port, data, timeout_seconds);
            callback("", response); // 第一个参数为空表示成功
        } catch (const std::exception& e) {
            callback(e.what(), BinaryData{}); // 第一个参数为错误信息，第二个参数为空
        }
    }).detach();
}

TcpClient::BinaryData TcpClient::sendAndReceiveString(const std::string& host, int port,
                                                     const std::string& data,
                                                     int timeout_seconds) {
    // 将字符串转换为二进制数据
    BinaryData binary_data(data.begin(), data.end());
    return sendAndReceive(host, port, binary_data, timeout_seconds);
}

TcpClient::BinaryData TcpClient::sendAndReceive(const std::string& host, int port,
                                               const void* data, size_t length,
                                               int timeout_seconds) {
    // 将C风格数据转换为二进制数据
    const uint8_t* byte_data = static_cast<const uint8_t*>(data);
    BinaryData binary_data(byte_data, byte_data + length);
    return sendAndReceive(host, port, binary_data, timeout_seconds);
}

bool TcpClient::isConnected() const {
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    return pImpl_->connected;
}

void TcpClient::setConnectTimeout(int seconds) {
    pImpl_->connect_timeout_seconds = seconds;
}

void TcpClient::setReadWriteTimeout(int seconds) {
    pImpl_->read_write_timeout_seconds = seconds;
}

std::string TcpClient::binaryToString(const BinaryData& data) {
    return std::string(data.begin(), data.end());
}

TcpClient::BinaryData TcpClient::stringToBinary(const std::string& str) {
    return BinaryData(str.begin(), str.end());
}

std::string TcpClient::binaryToHex(const BinaryData& data) {
    std::string hex_string;
    hex_string.reserve(data.size() * 2);
    
    for (uint8_t byte : data) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", byte);
        hex_string += hex;
    }
    
    return hex_string;
}