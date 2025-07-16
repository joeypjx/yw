#ifndef MULTICAST_SENDER_H
#define MULTICAST_SENDER_H

#include <string>
#include <thread>
#include <atomic>
#include <vector>

class MulticastSender {
public:
    /**
     * @brief 构造函数.
     * @param multicast_ip 组播IP地址.
     * @param multicast_port 组播端口.
     * @param manager_port 对外服务的HTTP端口.
     * @param config_path 用于获取本机IP的配置文件路径.
     */
    MulticastSender(const std::string& multicast_ip, int multicast_port,
                    int manager_port = 8080, const std::string& config_path = "");

    ~MulticastSender();

    /**
     * @brief 启动发送线程.
     */
    void start();

    /**
     * @brief 停止发送线程.
     */
    void stop();

private:
    /**
     * @brief 每2秒发送一次心跳消息的线程循环.
     */
    void heartbeatLoop();

    /**
     * @brief 每5秒发送一次资源消息的线程循环.
     */
    void resourceLoop();

    /**
     * @brief 发送组播消息的通用函数.
     * @param message 要发送的消息.
     * @return 成功返回true，否则返回false.
     */
    bool sendMulticastMessage(const std::string& message);

    std::string m_multicast_ip;
    int m_multicast_port;
    std::string m_manager_ip;
    int m_manager_port;

    std::atomic<bool> m_running;
    std::vector<std::thread> m_threads;
};

#endif // MULTICAST_SENDER_H
