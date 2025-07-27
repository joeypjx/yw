#ifndef IP_UTILS_H
#define IP_UTILS_H

#include <string>

class IPAddressUtil {
public:
    /**
     * @brief 获取主机的IP地址.
     *
     * 该函数遵循以下顺序来确定IP地址:
     * 1. 如果提供了有效的配置文件路径，则尝试从该文件中获取IP.
     *    - 配置文件应为JSON格式.
     *    - 支持 "ip_address" 键，直接指定IP.
     *    - 支持 "interface_name" 键，指定网络接口名称.
     * 2. 如果配置文件无法获取IP，则会自动选择一个“智能”的默认IP.
     * 3. 如果所有方法都失败，则返回 "127.0.0.1".
     *
     * @param configPath JSON配置文件的可选路径.
     * @return 返回找到的IP地址字符串.
     */
    static std::string getIPAddress(const std::string& configPath = "");

private:
    /**
     * @brief 从JSON配置文件中解析IP地址.
     * @param configPath 配置文件的路径.
     * @return 如果成功则返回IP地址，否则返回空字符串.
     */
    static std::string getIPFromConfig(const std::string& configPath);

    /**
     * @brief 获取一个智能的默认IP���址.
     *
     * 扫描所有网络接口，并根据预定义的优先级和规则选择最佳IP.
     * - 排除回环和关闭的接口.
     * - 优先选择常见物理网卡名称.
     *
     * @return 返回找到的最佳IP地址，如果找不到则返回空字符串.
     */
    static std::string getSmartDefaultIP();
};

#endif // IP_UTILS_H
