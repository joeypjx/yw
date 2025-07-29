#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "httplib.h"
#include "resource_storage.h"
#include "alarm_rule_storage.h"
#include "alarm_manager.h"
#include "node_storage.h"
#include "resource_manager.h"
#include "bmc_storage.h"
#include "chassis_controller.h"
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
     * @param alarm_manager 用于管理告警事件的 AlarmManager 实例的共享指针.
     * @param node_storage 用于存储节点数据的 NodeStorage 实例的共享指针.
     * @param host 监听的主机地址.
     * @param port 监听的端口.
     */
    HttpServer(std::shared_ptr<ResourceStorage> resource_storage,
               std::shared_ptr<AlarmRuleStorage> alarm_rule_storage,
               std::shared_ptr<AlarmManager> alarm_manager,
               std::shared_ptr<NodeStorage> node_storage,
               std::shared_ptr<ResourceManager> resource_manager,
               std::shared_ptr<BMCStorage> bmc_storage,
               std::shared_ptr<ChassisController> chassis_controller = nullptr,
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
     * @brief 处理 /alarm/rules 的POST请求 (创建告警规则).
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_alarm_rules_create(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理 /alarm/rules 的GET请求 (获取所有告警规则).
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_alarm_rules_list(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理 /alarm/rules/{id} 的GET请求 (获取单个告警规则).
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_alarm_rules_get(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理 /alarm/rules/{id}/update 的POST请求 (更新告警规则).
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_alarm_rules_update(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理 /alarm/rules/{id}/delete 的POST请求 (删除告警规则).
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_alarm_rules_delete(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理 /alarm/events 的GET请求 (获取所有告警事件).
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_alarm_events_list(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理 /alarm/events/count 的GET请求 (获取告警事件总数量).
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_alarm_events_count(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理 /heart 的POST请求 (节点心跳).
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_heart(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理 /nodes 的GET请求 (获取所有节点数据).
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_nodes_list(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理 /node/metrics 的GET请求 (获取节点指标数据).
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_node_metrics(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理 /node/historical-metrics 的GET请求 (获取节点历史指标数据).
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_node_historical_metrics(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理 /node/historical-bmc 的GET请求 (获取BMC历史数据).
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_node_historical_bmc(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理 /chassis/reset 的POST请求 (复位机箱板卡).
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_chassis_reset(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理 /chassis/power-off 的POST请求 (下电机箱板卡).
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_chassis_power_off(const httplib::Request& req, httplib::Response& res);

    /**
     * @brief 处理 /chassis/power-on 的POST请求 (上电机箱板卡).
     * @param req HTTP请求.
     * @param res HTTP响应.
     */
    void handle_chassis_power_on(const httplib::Request& req, httplib::Response& res);

    std::shared_ptr<ResourceStorage> m_resource_storage;
    std::shared_ptr<AlarmRuleStorage> m_alarm_rule_storage;
    std::shared_ptr<AlarmManager> m_alarm_manager;
    std::shared_ptr<NodeStorage> m_node_storage;
    std::shared_ptr<ResourceManager> m_resource_manager;
    std::shared_ptr<BMCStorage> m_bmc_storage;
    std::shared_ptr<ChassisController> m_chassis_controller;
    httplib::Server m_server;
    std::string m_host;
    int m_port;
    std::thread m_server_thread;
};

#endif // HTTP_SERVER_H
