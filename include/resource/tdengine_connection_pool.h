#pragma once

#include <string>
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <thread>
#include <map>
#include <functional>
#include <taos.h>

//=============================================================================
// TDengine连接池配置结构
//=============================================================================

struct TDenginePoolConfig {
    // 连接参数
    std::string host = "localhost";
    int port = 6030;
    std::string user = "test";
    std::string password = "HZ715Net";
    std::string database = "resource";
    
    // 连接池大小配置
    int min_connections = 3;        // 最小连接数
    int max_connections = 10;       // 最大连接数
    int initial_connections = 5;    // 初始连接数
    
    // 超时配置（秒）
    int connection_timeout = 30;    // 连接超时
    int idle_timeout = 600;         // 空闲超时（10分钟）
    int max_lifetime = 3600;        // 连接最大生存时间（1小时）
    int acquire_timeout = 10;       // 获取连接超时
    
    // 健康检查配置
    int health_check_interval = 60; // 健康检查间隔（秒）
    std::string health_check_query = "SELECT SERVER_VERSION()"; // 健康检查SQL
    
    // TDengine特定配置
    std::string locale = "C";
    std::string charset = "UTF-8";
    std::string timezone = "";
    
    // 其他配置
    bool auto_reconnect = true;     // 自动重连
    int max_sql_length = 1048576;   // 最大SQL长度（1MB）
};

//=============================================================================
// TDengine连接RAII包装器
//=============================================================================

class TDengineConnection {
public:
    TDengineConnection(TAOS* taos, const std::chrono::steady_clock::time_point& created_time);
    ~TDengineConnection();
    
    // 移动构造和赋值
    TDengineConnection(TDengineConnection&& other) noexcept;
    TDengineConnection& operator=(TDengineConnection&& other) noexcept;
    
    // 禁用拷贝构造和赋值
    TDengineConnection(const TDengineConnection&) = delete;
    TDengineConnection& operator=(const TDengineConnection&) = delete;
    
    // 获取原始TAOS连接
    TAOS* get() const { return taos_; }
    
    // 连接有效性检查
    bool isValid() const;
    bool isExpired(int max_lifetime_seconds) const;
    bool isIdleTimeout(int idle_timeout_seconds) const;
    
    // 更新最后使用时间
    void updateLastUsed();
    
    // 健康检查
    bool healthCheck(const std::string& query = "SELECT SERVER_VERSION()");
    
    // 获取创建时间和最后使用时间
    std::chrono::steady_clock::time_point getCreatedTime() const { return created_time_; }
    std::chrono::steady_clock::time_point getLastUsedTime() const { return last_used_time_; }

private:
    TAOS* taos_;
    std::chrono::steady_clock::time_point created_time_;
    std::chrono::steady_clock::time_point last_used_time_;
};

//=============================================================================
// TDengine连接池类
//=============================================================================

class TDengineConnectionPool {
public:
    // 连接池统计信息
    struct PoolStats {
        int total_connections = 0;
        int active_connections = 0;
        int idle_connections = 0;
        int pending_requests = 0;
        int created_connections = 0;
        int destroyed_connections = 0;
        double average_wait_time = 0.0; // 毫秒
    };

    explicit TDengineConnectionPool(const TDenginePoolConfig& config);
    ~TDengineConnectionPool();
    
    // 禁用拷贝构造和赋值
    TDengineConnectionPool(const TDengineConnectionPool&) = delete;
    TDengineConnectionPool& operator=(const TDengineConnectionPool&) = delete;
    
    // 初始化和关闭
    bool initialize();
    void shutdown();
    
    // 获取和释放连接
    std::unique_ptr<TDengineConnection> getConnection(int timeout_ms = 0);
    void releaseConnection(std::unique_ptr<TDengineConnection> connection);
    
    // 统计信息
    PoolStats getStats() const;
    bool isHealthy() const;
    
    // 配置更新
    void updateConfig(const TDenginePoolConfig& config);
    TDenginePoolConfig getConfig() const { return config_; }
    
    // 日志回调设置
    void setLogCallback(std::function<void(const std::string&, const std::string&)> callback);

    // 快速关闭连接池（不等待活跃连接）
    void shutdownFast();
    
    // 强制关闭连接池（立即关闭，可能有风险）
    void shutdownForce();
    
    // 设置关闭超时时间（毫秒）
    void setShutdownTimeout(int timeout_ms);
    
    // 获取关闭超时时间
    int getShutdownTimeout() const { return shutdown_timeout_ms_; }

private:
    TDenginePoolConfig config_;
    mutable std::mutex pool_mutex_;
    std::condition_variable pool_condition_;
    
    std::queue<std::unique_ptr<TDengineConnection>> available_connections_;
    
    // 连接池状态
    std::atomic<bool> initialized_;
    std::atomic<bool> shutdown_;
    
    // 统计信息
    std::atomic<int> total_connections_;
    std::atomic<int> active_connections_;
    std::atomic<int> created_connections_;
    std::atomic<int> destroyed_connections_;
    std::atomic<int> pending_requests_;
    std::atomic<double> total_wait_time_;
    std::atomic<int> wait_count_;
    
    // 健康检查线程
    std::thread health_check_thread_;
    std::atomic<bool> stop_health_check_;
    
    // 日志回调
    std::function<void(const std::string&, const std::string&)> log_callback_;
    
    // 关闭超时时间（毫秒）
    int shutdown_timeout_ms_ = 5000;  // 默认5秒
    
    // 私有方法
    std::unique_ptr<TDengineConnection> createConnection();
    bool testConnection(TAOS* taos);
    void healthCheckLoop();
    void cleanupExpiredConnections();
    void ensureMinConnections();
    
    // 日志方法
    void logInfo(const std::string& message) const;
    void logError(const std::string& message) const;
    void logDebug(const std::string& message) const;
    void logWarning(const std::string& message) const;
};

//=============================================================================
// TDengine连接池管理器（单例模式）
//=============================================================================

class TDengineConnectionPoolManager {
public:
    static TDengineConnectionPoolManager& getInstance();
    
    // 创建和管理连接池
    bool createPool(const std::string& name, const TDenginePoolConfig& config);
    std::shared_ptr<TDengineConnectionPool> getPool(const std::string& name);
    void destroyPool(const std::string& name);
    void destroyAllPools();
    
    // 获取所有连接池名称
    std::vector<std::string> getAllPoolNames() const;

private:
    TDengineConnectionPoolManager() = default;
    ~TDengineConnectionPoolManager() { destroyAllPools(); }
    
    mutable std::mutex pools_mutex_;
    std::map<std::string, std::shared_ptr<TDengineConnectionPool>> pools_;
};

//=============================================================================
// TDengine连接RAII守卫类
//=============================================================================

class TDengineConnectionGuard {
public:
    explicit TDengineConnectionGuard(std::shared_ptr<TDengineConnectionPool> pool, int timeout_ms = 0);
    ~TDengineConnectionGuard();
    
    // 禁用拷贝构造和赋值
    TDengineConnectionGuard(const TDengineConnectionGuard&) = delete;
    TDengineConnectionGuard& operator=(const TDengineConnectionGuard&) = delete;
    
    // 移动构造和赋值
    TDengineConnectionGuard(TDengineConnectionGuard&& other) noexcept
        : pool_(std::move(other.pool_)), connection_(std::move(other.connection_)) {
        other.pool_ = nullptr;
        other.connection_ = nullptr;
    }
    
    TDengineConnectionGuard& operator=(TDengineConnectionGuard&& other) noexcept {
        if (this != &other) {
            if (connection_ && pool_) {
                pool_->releaseConnection(std::move(connection_));
            }
            pool_ = std::move(other.pool_);
            connection_ = std::move(other.connection_);
            other.pool_ = nullptr;
            other.connection_ = nullptr;
        }
        return *this;
    }
    
    // 检查连接是否有效
    bool isValid() const { return connection_ != nullptr; }
    
    // 获取连接
    TDengineConnection* get() const { return connection_.get(); }
    TDengineConnection* operator->() const { return connection_.get(); }
    TDengineConnection& operator*() const { return *connection_; }

private:
    std::shared_ptr<TDengineConnectionPool> pool_;
    std::unique_ptr<TDengineConnection> connection_;
};

//=============================================================================
// TDengine结果集RAII包装器
//=============================================================================

class TDengineResultRAII {
public:
    explicit TDengineResultRAII(TAOS_RES* res) : result_(res) {}
    ~TDengineResultRAII() { 
        if (result_) {
            taos_free_result(result_); 
        }
    }
    
    // 禁用拷贝构造和赋值
    TDengineResultRAII(const TDengineResultRAII&) = delete;
    TDengineResultRAII& operator=(const TDengineResultRAII&) = delete;
    
    // 移动构造和赋值
    TDengineResultRAII(TDengineResultRAII&& other) noexcept : result_(other.result_) {
        other.result_ = nullptr;
    }
    
    TDengineResultRAII& operator=(TDengineResultRAII&& other) noexcept {
        if (this != &other) {
            if (result_) {
                taos_free_result(result_);
            }
            result_ = other.result_;
            other.result_ = nullptr;
        }
        return *this;
    }
    
    TAOS_RES* get() const { return result_; }
    TAOS_RES* release() { 
        TAOS_RES* r = result_; 
        result_ = nullptr; 
        return r; 
    }

private:
    TAOS_RES* result_;
};