#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "httplib.h"
#include "resource_storage.h"
#include "alarm_rule_storage.h"
#include "json.hpp"
#include <string>
#include <thread>
#include <memory>

class HttpServer {
public:
    /**
     * @brief 构造函数.
     * @param resource_storage 用于存储资源数据的 ResourceStorage 实例的共享指针.
     * @param alarm_rule_storage 用于存储告警规则的 AlarmRuleStorage 实例的共享指针.
     * @param host 监听的主机地址.
     * @param port 监听的端口.
     */
    HttpServer(std::shared_ptr<ResourceStorage> resource_storage,
               std::shared_ptr<AlarmRuleStorage> alarm_rule_storage,
               const std::string& host = "0.0.0.0", int port = 8080);

    ~HttpServer();

    /**
     * @brief 在后台线程中启动HTTP服务器.
     * @return 成功启动返回true, 否则返回false.
     */
    bool start();

    /**
     * @brief 停止HTTP服务器.
     */
    void stop();

    /**
     * @brief 解析metricName并构建内部表达式格式.
     * @param metric_name 指标名称.
     * @param condition 条件对象.
     * @return 内部表达式JSON格式.
     */
    nlohmann::json parseMetricNameAndBuildExpression(const std::string& metric_name, const nlohmann::json& condition);

    /**
     * @brief 转换description模板格式从{}到{{}}.
     * @param template_str 模板字符串.
     * @return 转换后的模板字符串.
     */
    std::string convertDescriptionTemplate(const std::string& template_str);

private:
    /**
     * @brief 设置服务器路由.
     */
    void setup_routes();

    /**
     * @brief 处理 /resource 的POST请求.
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_resource(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理 /alarm/rules 的POST请求.
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_alarm_rules(const httplib::Request& req, httplib::Response& res);

    std::shared_ptr<ResourceStorage> m_resource_storage;
    std::shared_ptr<AlarmRuleStorage> m_alarm_rule_storage;
    httplib::Server m_server;
    std::string m_host;
    int m_port;
    std::thread m_server_thread;
};

#endif // HTTP_SERVER_H
