#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include "spdlog/spdlog.h"
#include <memory>
#include <string>

class LogManager {
public:
    /**
     * @brief 根据配置文件初始化日志系统.
     * 应该在程序启动时调用一次.
     * @param configPath JSON配置文件的路径.
     */
    static void init(const std::string& configPath = "log_config.json");

    /**
     * @brief 获取全局日志记录器的共享指针.
     * @return spdlog::logger的实例.
     */
    static std::shared_ptr<spdlog::logger>& getLogger();
};

#endif // LOG_MANAGER_H
