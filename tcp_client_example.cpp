#include "resource/tcp_client.h"
#include "spdlog/spdlog.h"
#include <iostream>

int main() {
    try {
        TcpClient client;
        
        // 示例1：发送二进制数据
        TcpClient::BinaryData binary_data = {0x01, 0x02, 0x03, 0x04, 0xFF, 0xAB, 0xCD, 0xEF};
        
        std::cout << "Sending binary data: " << TcpClient::binaryToHex(binary_data) << std::endl;
        
        // 注意：这里使用一个不存在的服务器作为示例，实际使用时需要替换为真实的服务器
        // TcpClient::BinaryData response = client.sendAndReceive("127.0.0.1", 8080, binary_data);
        // std::cout << "Received response: " << TcpClient::binaryToHex(response) << std::endl;
        
        // 示例2：发送字符串数据（转换为二进制）
        std::string message = "Hello, Binary World!";
        TcpClient::BinaryData string_as_binary = TcpClient::stringToBinary(message);
        
        std::cout << "String as binary: " << TcpClient::binaryToHex(string_as_binary) << std::endl;
        std::cout << "Binary as string: " << TcpClient::binaryToString(string_as_binary) << std::endl;
        
        // 示例3：发送C风格数据
        char c_data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
        // TcpClient::BinaryData c_response = client.sendAndReceive("127.0.0.1", 8080, c_data, sizeof(c_data));
        
        std::cout << "TCP Client with binary data support is ready!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}