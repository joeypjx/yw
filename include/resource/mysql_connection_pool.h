#ifndef MYSQL_CONNECTION_POOL_H
#define MYSQL_CONNECTION_POOL_H

#include <mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <functional>

/**
 * @brief MySQL连接池配置结构体
 */
struct MySQLPoolConfig {
    std::string host = "localhost";           // 数据库主机
    int port = 3306;                         // 数据库端口
    std::string user = "test";               // 用户名
    std::string password = "HZ715Net";               // 密码
    std::string database = "alarm";               // 数据库名
    std::string charset = "utf8mb4";         // 字符集
    
    // 连接池配置
    int min_connections = 5;                 // 最小连接数
    int max_connections = 20;                // 最大连接数
    int initial_connections = 5;             // 初始连接数
    
    // 超时配置（秒）
    int connection_timeout = 30;             // 连接超时
    int idle_timeout = 600;                  // 空闲超时（10分钟）
    int max_lifetime = 3600;                 // 连接最大生存时间（1小时）
    int acquire_timeout = 10;                // 获取连接超时
    
    // 健康检查配置
    int health_check_interval = 60;          // 健康检查间隔（秒）
    std::string health_check_query = "SELECT 1"; // 健康检查SQL
    
    // 连接选项
    bool auto_reconnect = true;              // 自动重连
    bool use_ssl = false;                    // 使用SSL
    int max_allowed_packet = 16777216;       // 最大包大小（16MB）
};

/**
 * @brief MySQL连接包装类，提供RAII语义
 */
class MySQLConnection {
public:
    MySQLConnection(MYSQL* mysql, const std::chrono::steady_clock::time_point& created_time);
    ~MySQLConnection();
    
    // 禁止拷贝和赋值
    MySQLConnection(const MySQLConnection&) = delete;
    MySQLConnection& operator=(const MySQLConnection&) = delete;
    
    // 允许移动
    MySQLConnection(MySQLConnection&& other) noexcept;
    MySQLConnection& operator=(MySQLConnection&& other) noexcept;
    
    // 获取原始MySQL连接
    MYSQL* get() const { return mysql_; }
    
    // 检查连接是否有效
    bool isValid() const;
    
    // 检查连接是否过期
    bool isExpired(int max_lifetime_seconds) const;
    
    // 检查连接是否空闲超时
    bool isIdleTimeout(int idle_timeout_seconds) const;
    
    // 更新最后使用时间
    void updateLastUsed();
    
    // 执行健康检查
    bool healthCheck(const std::string& query = "SELECT 1");
    
    // 获取连接创建时间
    std::chrono::steady_clock::time_point getCreatedTime() const { return created_time_; }
    
    // 获取最后使用时间
    std::chrono::steady_clock::time_point getLastUsedTime() const { return last_used_time_; }

private:
    MYSQL* mysql_;
    std::chrono::steady_clock::time_point created_time_;
    std::chrono::steady_clock::time_point last_used_time_;
};

/**
 * @brief MySQL连接池类
 */
class MySQLConnectionPool {
public:
    /**
     * @brief 构造函数
     * @param config 连接池配置
     */
    explicit MySQLConnectionPool(const MySQLPoolConfig& config);
    
    /**
     * @brief 析构函数
     */
    ~MySQLConnectionPool();
    
    // 禁止拷贝和赋值
    MySQLConnectionPool(const MySQLConnectionPool&) = delete;
    MySQLConnectionPool& operator=(const MySQLConnectionPool&) = delete;
    
    /**
     * @brief 初始化连接池
     * @return 成功返回true，失败返回false
     */
    bool initialize();
    
    /**
     * @brief 关闭连接池
     */
    void shutdown();
    
    /**
     * @brief 获取数据库连接
     * @param timeout_ms 超时时间（毫秒），0表示使用默认超时，-1表示无限等待
     * @return 连接智能指针，获取失败返回nullptr
     */
    std::unique_ptr<MySQLConnection> getConnection(int timeout_ms = 0);
    
    /**
     * @brief 释放数据库连接（由智能指针自动调用）
     * @param connection 要释放的连接
     */
    void releaseConnection(std::unique_ptr<MySQLConnection> connection);
    
    /**
     * @brief 获取连接池统计信息
     */
    struct PoolStats {
        int total_connections;      // 总连接数
        int active_connections;     // 活跃连接数
        int idle_connections;       // 空闲连接数
        int pending_requests;       // 等待连接的请求数
        int created_connections;    // 已创建的连接总数
        int destroyed_connections;  // 已销毁的连接总数
        double average_wait_time;   // 平均等待时间（毫秒）
    };
    
    PoolStats getStats() const;
    
    /**
     * @brief 检查连接池是否正常运行
     * @return 正常返回true，异常返回false
     */
    bool isHealthy() const;
    
    /**
     * @brief 设置日志回调函数
     * @param callback 日志回调函数
     */
    void setLogCallback(std::function<void(const std::string&, const std::string&)> callback);

    // 快速关闭连接池（不等待活跃连接）
    void shutdownFast();
    
    // 强制关闭连接池（立即关闭，可能有风险）
    void shutdownForce();
    
    // 设置关闭超时时间（毫秒）
    void setShutdownTimeout(int timeout_ms);
    
    // 获取关闭超时时间
    int getShutdownTimeout() const { return shutdown_timeout_ms_; }

    // 获取配置
    const MySQLPoolConfig& getConfig() const { return config_; }

private:
    MySQLPoolConfig config_;
    
    // 连接池状态
    std::atomic<bool> initialized_;
    std::atomic<bool> shutdown_;
    
    // 连接管理
    std::queue<std::unique_ptr<MySQLConnection>> available_connections_;
    mutable std::mutex pool_mutex_;
    std::condition_variable pool_condition_;
    
    // 统计信息
    mutable std::atomic<int> total_connections_;
    mutable std::atomic<int> active_connections_;
    mutable std::atomic<int> created_connections_;
    mutable std::atomic<int> destroyed_connections_;
    mutable std::atomic<int> pending_requests_;
    mutable std::atomic<double> total_wait_time_;
    mutable std::atomic<int> wait_count_;
    
    // 健康检查线程
    std::thread health_check_thread_;
    std::atomic<bool> stop_health_check_;
    
    // 日志回调
    std::function<void(const std::string&, const std::string&)> log_callback_;
    
    // 关闭超时时间（毫秒）
    int shutdown_timeout_ms_ = 5000;  // 默认5秒
    
    // 私有方法
    std::unique_ptr<MySQLConnection> createConnection();
    bool testConnection(MYSQL* mysql);
    void healthCheckLoop();
    void cleanupExpiredConnections();
    void ensureMinConnections();
    void logInfo(const std::string& message) const;
    void logError(const std::string& message) const;
    void logDebug(const std::string& message) const;
    void logWarning(const std::string& message) const;
};

/**
 * @brief 连接池管理器单例类
 */
class MySQLConnectionPoolManager {
public:
    /**
     * @brief 获取单例实例
     */
    static MySQLConnectionPoolManager& getInstance();
    
    /**
     * @brief 创建连接池
     * @param name 连接池名称
     * @param config 连接池配置
     * @return 成功返回true，失败返回false
     */
    bool createPool(const std::string& name, const MySQLPoolConfig& config);
    
    /**
     * @brief 获取连接池
     * @param name 连接池名称
     * @return 连接池指针，不存在返回nullptr
     */
    std::shared_ptr<MySQLConnectionPool> getPool(const std::string& name);
    
    /**
     * @brief 销毁连接池
     * @param name 连接池名称
     */
    void destroyPool(const std::string& name);
    
    /**
     * @brief 销毁所有连接池
     */
    void destroyAllPools();
    
    /**
     * @brief 获取所有连接池名称
     */
    std::vector<std::string> getAllPoolNames() const;

private:
    MySQLConnectionPoolManager() = default;
    ~MySQLConnectionPoolManager() = default;
    
    // 禁止拷贝和赋值
    MySQLConnectionPoolManager(const MySQLConnectionPoolManager&) = delete;
    MySQLConnectionPoolManager& operator=(const MySQLConnectionPoolManager&) = delete;
    
    std::map<std::string, std::shared_ptr<MySQLConnectionPool>> pools_;
    mutable std::mutex pools_mutex_;
};

/**
 * @brief RAII连接获取器，自动管理连接的获取和释放
 */
class MySQLConnectionGuard {
public:
    /**
     * @brief 构造函数，自动获取连接
     * @param pool 连接池
     * @param timeout_ms 超时时间（毫秒）
     */
    MySQLConnectionGuard(std::shared_ptr<MySQLConnectionPool> pool, int timeout_ms = 0);
    
    /**
     * @brief 析构函数，自动释放连接
     */
    ~MySQLConnectionGuard();
    
    // 禁止拷贝和赋值
    MySQLConnectionGuard(const MySQLConnectionGuard&) = delete;
    MySQLConnectionGuard& operator=(const MySQLConnectionGuard&) = delete;
    
    /**
     * @brief 检查连接是否有效
     */
    bool isValid() const { return connection_ != nullptr; }
    
    /**
     * @brief 获取连接
     */
    MySQLConnection* get() const { return connection_.get(); }
    
    /**
     * @brief 箭头操作符
     */
    MySQLConnection* operator->() const { return connection_.get(); }
    
    /**
     * @brief 解引用操作符
     */
    MySQLConnection& operator*() const { return *connection_; }

private:
    std::shared_ptr<MySQLConnectionPool> pool_;
    std::unique_ptr<MySQLConnection> connection_;
};

#endif // MYSQL_CONNECTION_POOL_H