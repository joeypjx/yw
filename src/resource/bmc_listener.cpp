#include "../../include/resource/bmc_listener.h"
#include "../../include/resource/log_manager.h"
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <unistd.h>
#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <thread>
#include <atomic>
#include <functional>
#include <cctype>

using namespace std;

class BMCListener {
private:
    string group_ip_;
    uint16_t port_;
    int sockfd_;
    atomic<bool> running_;
    thread listener_thread_;
    function<void(const UdpInfo&)> data_callback_;
    
public:
    BMCListener(const string& group_ip, uint16_t port) 
        : group_ip_(group_ip), port_(port), sockfd_(-1), running_(false) {}
    
    ~BMCListener() {
        stop();
    }
    
    bool initialize() {
        struct sockaddr_in addr;
        struct ip_mreq mreq;
        int opt = 1;
        
        sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd_ < 0) {
            LogManager::getLogger()->error("BMC socket creation failed: {}", strerror(errno));
            return false;
        }
        
        if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            LogManager::getLogger()->error("BMC setsockopt SO_REUSEADDR failed: {}", strerror(errno));
            close(sockfd_);
            return false;
        }
        
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);
        
        if (::bind(sockfd_, reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            LogManager::getLogger()->error("BMC bind failed: {}", strerror(errno));
            close(sockfd_);
            return false;
        }
        
        mreq.imr_multiaddr.s_addr = inet_addr(group_ip_.c_str());
        mreq.imr_interface.s_addr = INADDR_ANY;
        
        if (setsockopt(sockfd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            LogManager::getLogger()->error("BMC setsockopt IP_ADD_MEMBERSHIP failed: {}", strerror(errno));
            close(sockfd_);
            return false;
        }
        
        LogManager::getLogger()->info("✅ BMC监听器初始化成功 {}:{}", group_ip_, port_);
        return true;
    }
    
    void start() {
        if (running_) return;
        
        running_ = true;
        listener_thread_ = thread(&BMCListener::listen, this);
        LogManager::getLogger()->info("🔊 BMC监听线程启动");
    }
    
    void stop() {
        if (!running_) return;
        
        running_ = false;
        if (listener_thread_.joinable()) {
            listener_thread_.join();
        }
        
        if (sockfd_ >= 0) {
            close(sockfd_);
            sockfd_ = -1;
        }
        LogManager::getLogger()->info("🔇 BMC监听器已停止");
    }
    
    void setDataCallback(const function<void(const UdpInfo&)>& callback) {
        data_callback_ = callback;
    }
    
private:
    void listen() {
        UdpInfo data;
        
        while (running_) {
            int result = receive(&data, 1000);
            if (result > 0) {
                LogManager::getLogger()->debug("收到BMC数据 ({} bytes)", result);
                
                if (data_callback_) {
                    data_callback_(data);
                }
            } else if (result < 0) {
                LogManager::getLogger()->error("BMC数据接收错误");
                break;
            }
        }
    }
    
    int receive(UdpInfo* data, int timeout_ms) {
        fd_set readfds;
        struct timeval timeout;
        ssize_t recv_len;
        
        if (sockfd_ < 0 || data == nullptr) {
            return -1;
        }
        
        FD_ZERO(&readfds);
        FD_SET(sockfd_, &readfds);
        
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        
        int ready = select(sockfd_ + 1, &readfds, NULL, NULL, &timeout);
        if (ready < 0) {
            LogManager::getLogger()->error("BMC select failed: {}", strerror(errno));
            return -1;
        } else if (ready == 0) {
            return 0;
        }
        
        recv_len = recv(sockfd_, data, sizeof(UdpInfo), 0);
        if (recv_len < 0) {
            LogManager::getLogger()->error("BMC recv failed: {}", strerror(errno));
            return -1;
        }
        
        if (recv_len != sizeof(UdpInfo)) {
            LogManager::getLogger()->warn("BMC收到 {} 字节, 期望 {} 字节", recv_len, sizeof(UdpInfo));
            return -1;
        }
        
        if (data->head != 0xA55A || data->tail != 0xA55A) {
            LogManager::getLogger()->warn("BMC数据包头尾无效");
            return -1;
        }
        
        return recv_len;
    }
    
    string dataToJson(const UdpInfo* data) {
        if (data == nullptr) {
            return "";
        }
        
        ostringstream json_stream;
        
        json_stream << "{\n"
                    << "  \"header\": {\n"
                    << "    \"head\": \"0x" << hex << uppercase << data->head << "\",\n"
                    << "    \"message_length\": " << dec << data->msglenth << ",\n"
                    << "    \"sequence_number\": " << dec << data->seqnum << ",\n"
                    << "    \"message_type\": \"0x" << hex << uppercase << data->msgtype << "\",\n"
                    << "    \"timestamp\": " << dec << data->timestamp << ",\n"
                    << "    \"box_name\": " << dec << static_cast<int>(data->boxname) << ",\n"
                    << "    \"box_id\": " << dec << static_cast<int>(data->boxid) << ",\n"
                    << "    \"tail\": \"0x" << hex << uppercase << data->tail << "\"\n"
                    << "  },\n"
                    << "  \"fans\": [\n";
        
        for (int i = 0; i < 2; i++) {
            json_stream << "    {\n"
                        << "      \"sequence\": " << dec << static_cast<int>(data->fan[i].fanseq) << ",\n"
                        << "      \"mode\": {\n"
                        << "        \"alarm_type\": " << dec << ((data->fan[i].fanmode >> 4) & 0x0F) << ",\n"
                        << "        \"work_mode\": " << dec << (data->fan[i].fanmode & 0x0F) << "\n"
                        << "      },\n"
                        << "      \"speed\": " << dec << data->fan[i].fanspeed << "\n"
                        << "    }" << (i < 1 ? "," : "") << "\n";
        }
        
        json_stream << "  ],\n  \"boards\": [\n";
        
        for (int i = 0; i < 14; i++) {
            json_stream << "    {\n"
                        << "      \"ipmb_address\": " << dec << static_cast<int>(data->board[i].ipmbaddr) << ",\n"
                        << "      \"module_type\": " << dec << data->board[i].moduletype << ",\n"
                        << "      \"bmc_company\": " << dec << data->board[i].bmccompany << ",\n"
                        << "      \"bmc_version\": \"" << cleanString(string(reinterpret_cast<const char*>(data->board[i].bmcversion), 8)) << "\",\n"
                        << "      \"sensor_count\": " << dec << static_cast<int>(data->board[i].sensornum) << ",\n"
                        << "      \"sensors\": [\n";
            
            int sensor_count = data->board[i].sensornum < 5 ? data->board[i].sensornum : 5;
            for (int j = 0; j < sensor_count; j++) {
                uint16_t sensor_value = (data->board[i].sensor[j].sensorvalue_H << 8) | 
                                        data->board[i].sensor[j].sensorvalue_L;
                json_stream << "        {\n"
                            << "          \"sequence\": " << dec << static_cast<int>(data->board[i].sensor[j].sensorseq) << ",\n"
                            << "          \"type\": " << dec << static_cast<int>(data->board[i].sensor[j].sensortype) << ",\n"
                            << "          \"name\": \"" << cleanString(string(reinterpret_cast<const char*>(data->board[i].sensor[j].sensorname), 6)) << "\",\n"
                            << "          \"value\": " << dec << sensor_value << ",\n"
                            << "          \"alarm_type\": " << dec << static_cast<int>(data->board[i].sensor[j].sensoralmtype) << "\n"
                            << "        }" << (j < sensor_count - 1 ? "," : "") << "\n";
            }
            
            json_stream << "      ]\n    }" << (i < 13 ? "," : "") << "\n";
        }
        
        json_stream << "  ]\n}";
        
        return json_stream.str();
    }
    
private:
    string cleanString(const string& str) {
        string result;
        result.reserve(str.length());
        
        for (char c : str) {
            if (c == '\0') break;  // 遇到null字符就停止
            if (isalnum(c) || c == '_' || c == '.' || c == '-') {
                result += c;
            } else {
                result += '_';
            }
        }
        
        return result.empty() ? "unknown" : result;
    }
};

// 全局BMC监听器实例
static shared_ptr<BMCListener> g_bmc_listener;

// 兼容的C风格接口
int bmc_listener_init(const char* group_ip, uint16_t port) {
    g_bmc_listener = make_shared<BMCListener>(group_ip, port);
    return g_bmc_listener->initialize() ? 0 : -1;
}

void bmc_listener_start() {
    if (g_bmc_listener) {
        g_bmc_listener->start();
    }
}

void bmc_listener_stop() {
    if (g_bmc_listener) {
        g_bmc_listener->stop();
    }
}

void bmc_listener_set_callback(const function<void(const UdpInfo&)>& callback) {
    if (g_bmc_listener) {
        g_bmc_listener->setDataCallback(callback);
    }
}

void bmc_listener_cleanup() {
    g_bmc_listener.reset();
}