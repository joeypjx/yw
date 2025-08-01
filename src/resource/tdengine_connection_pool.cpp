#include "../../include/resource/tdengine_connection_pool.h"
#include "../../include/resource/log_manager.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <cstring>

// TDengine错误码兼容性定义
#ifndef TSDB_CODE_SUCCESS
#define TSDB_CODE_SUCCESS 0
#endif

//=============================================================================
// TDengineConnection Implementation
//=============================================================================

TDengineConnection::TDengineConnection(TAOS* taos, const std::chrono::steady_clock::time_point& created_time)
    : taos_(taos), created_time_(created_time), last_used_time_(created_time) {
}

TDengineConnection::~TDengineConnection() {
    if (taos_) {
        taos_close(taos_);
        taos_ = nullptr;
    }
}

TDengineConnection::TDengineConnection(TDengineConnection&& other) noexcept
    : taos_(other.taos_), created_time_(other.created_time_), last_used_time_(other.last_used_time_) {
    other.taos_ = nullptr;
}

TDengineConnection& TDengineConnection::operator=(TDengineConnection&& other) noexcept {
    if (this != &other) {
        if (taos_) {
            taos_close(taos_);
        }
        taos_ = other.taos_;
        created_time_ = other.created_time_;
        last_used_time_ = other.last_used_time_;
        other.taos_ = nullptr;
    }
    return *this;
}

bool TDengineConnection::isValid() const {
    if (!taos_) {
        return false;
    }
    
    // TDengine没有类似MySQL ping的直接方法，但我们可以执行一个简单的查询来检查
    // 这里我们简单地检查连接指针是否为空
    return true;
}

bool TDengineConnection::isExpired(int max_lifetime_seconds) const {
    auto now = std::chrono::steady_clock::now();
    auto lifetime = std::chrono::duration_cast<std::chrono::seconds>(now - created_time_).count();
    return lifetime >= max_lifetime_seconds;
}

bool TDengineConnection::isIdleTimeout(int idle_timeout_seconds) const {
    auto now = std::chrono::steady_clock::now();
    auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(now - last_used_time_).count();
    return idle_time >= idle_timeout_seconds;
}

void TDengineConnection::updateLastUsed() {
    last_used_time_ = std::chrono::steady_clock::now();
}

bool TDengineConnection::healthCheck(const std::string& query) {
    if (!taos_) {
        return false;
    }
    
    // 执行健康检查查询
    TAOS_RES* result = taos_query(taos_, query.c_str());
    if (taos_errno(result) != TSDB_CODE_SUCCESS) {
        taos_free_result(result);
        return false;
    }
    
    taos_free_result(result);
    updateLastUsed();
    return true;
}

//=============================================================================
// TDengineConnectionPool Implementation
//=============================================================================

TDengineConnectionPool::TDengineConnectionPool(const TDenginePoolConfig& config)
    : config_(config), initialized_(false), shutdown_(false),
      total_connections_(0), active_connections_(0),
      created_connections_(0), destroyed_connections_(0),
      pending_requests_(0), total_wait_time_(0.0), wait_count_(0),
      stop_health_check_(false) {
    
    // 参数验证和调整
    if (config_.min_connections < 1) {
        config_.min_connections = 1;
    }
    if (config_.max_connections < config_.min_connections) {
        config_.max_connections = config_.min_connections;
    }
    if (config_.initial_connections < config_.min_connections) {
        config_.initial_connections = config_.min_connections;
    }
    if (config_.initial_connections > config_.max_connections) {
        config_.initial_connections = config_.max_connections;
    }
}

TDengineConnectionPool::~TDengineConnectionPool() {
    shutdown();
}

bool TDengineConnectionPool::initialize() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (initialized_) {
        logDebug("连接池已经初始化");
        return true;
    }
    
    logInfo("正在初始化TDengine连接池...");
    
    // 初始化TDengine库
    if (taos_init() != 0) {
        logError("初始化TDengine库失败");
        return false;
    }
    
    // 设置本地化参数
    if (!config_.locale.empty()) {
        taos_options(TSDB_OPTION_LOCALE, config_.locale.c_str());
    }
    if (!config_.charset.empty()) {
        taos_options(TSDB_OPTION_CHARSET, config_.charset.c_str());
    }
    if (!config_.timezone.empty()) {
        taos_options(TSDB_OPTION_TIMEZONE, config_.timezone.c_str());
    }
    
    // 创建初始连接
    for (int i = 0; i < config_.initial_connections; ++i) {
        auto connection = createConnection();
        if (!connection) {
            logError("创建初始连接失败，连接池初始化失败");
            // 清理已创建的连接
            while (!available_connections_.empty()) {
                available_connections_.pop();
            }
            taos_cleanup();
            return false;
        }
        
        available_connections_.push(std::move(connection));
        total_connections_++;
    }
    
    initialized_ = true;
    
    // 启动健康检查线程
    stop_health_check_ = false;
    health_check_thread_ = std::thread(&TDengineConnectionPool::healthCheckLoop, this);
    
    logInfo("TDengine连接池初始化成功，创建了 " + std::to_string(config_.initial_connections) + " 个连接");
    return true;
}

void TDengineConnectionPool::shutdown() {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    if (shutdown_) {
        return;
    }
    
    logInfo("正在关闭TDengine连接池...");
    shutdown_ = true;
    
    // 停止健康检查线程
    stop_health_check_ = true;
    lock.unlock();
    
    if (health_check_thread_.joinable()) {
        health_check_thread_.join();
    }
    
    lock.lock();
    
    // 通知所有等待的线程
    pool_condition_.notify_all();
    
    // 等待所有活跃连接返回
    while (active_connections_ > 0) {
        logDebug("等待 " + std::to_string(active_connections_.load()) + " 个活跃连接返回...");
        pool_condition_.wait_for(lock, std::chrono::seconds(1));
    }
    
    // 清理所有连接
    while (!available_connections_.empty()) {
        available_connections_.pop();
    }
    
    total_connections_ = 0;
    initialized_ = false;
    
    // 清理TDengine库
    taos_cleanup();
    
    logInfo("TDengine连接池已关闭");
}

std::unique_ptr<TDengineConnection> TDengineConnectionPool::getConnection(int timeout_ms) {
    if (shutdown_) {
        logError("连接池已关闭，无法获取连接");
        return nullptr;
    }
    
    if (!initialized_) {
        logError("连接池未初始化，无法获取连接");
        return nullptr;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    int actual_timeout = (timeout_ms == 0) ? config_.acquire_timeout * 1000 : timeout_ms;
    
    std::unique_lock<std::mutex> lock(pool_mutex_);
    pending_requests_++;
    
    while (!shutdown_) {
        // 尝试从可用连接池获取连接
        if (!available_connections_.empty()) {
            auto connection = std::move(available_connections_.front());
            available_connections_.pop();
            
            // 检查连接是否有效
            if (connection->isValid() && 
                !connection->isExpired(config_.max_lifetime) &&
                !connection->isIdleTimeout(config_.idle_timeout)) {
                
                connection->updateLastUsed();
                active_connections_++;
                pending_requests_--;
                
                // 记录等待时间
                auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time).count();
                total_wait_time_.store(total_wait_time_.load() + wait_time);
                wait_count_++;
                
                logDebug("成功获取连接，等待时间: " + std::to_string(wait_time) + "ms");
                return connection;
            } else {
                // 连接无效，销毁并尝试创建新连接
                logDebug("发现无效连接，正在销毁");
                total_connections_--;
                destroyed_connections_++;
            }
        }
        
        // 如果没有可用连接且未达到最大连接数，创建新连接
        if (total_connections_ < config_.max_connections) {
            auto connection = createConnection();
            if (connection) {
                connection->updateLastUsed();
                active_connections_++;
                pending_requests_--;
                total_connections_++;
                
                // 记录等待时间
                auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time).count();
                total_wait_time_.store(total_wait_time_.load() + wait_time);
                wait_count_++;
                
                logDebug("创建新连接成功，等待时间: " + std::to_string(wait_time) + "ms");
                return connection;
            }
        }
        
        // 检查超时
        if (actual_timeout > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            if (elapsed >= actual_timeout) {
                pending_requests_--;
                logError("获取连接超时，等待时间: " + std::to_string(elapsed) + "ms");
                return nullptr;
            }
            
            // 等待连接释放或超时
            auto remaining = std::chrono::milliseconds(actual_timeout - elapsed);
            if (pool_condition_.wait_for(lock, remaining) == std::cv_status::timeout) {
                pending_requests_--;
                logError("获取连接超时");
                return nullptr;
            }
        } else if (actual_timeout == -1) {
            // 无限等待
            pool_condition_.wait(lock);
        } else {
            // 立即返回
            pending_requests_--;
            logError("无法立即获取连接");
            return nullptr;
        }
    }
    
    pending_requests_--;
    logError("连接池已关闭，无法获取连接");
    return nullptr;
}

void TDengineConnectionPool::releaseConnection(std::unique_ptr<TDengineConnection> connection) {
    if (!connection) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (shutdown_) {
        // 连接池已关闭，直接销毁连接
        total_connections_--;
        destroyed_connections_++;
        active_connections_--;
        return;
    }
    
    // 检查连接是否有效且未过期
    if (connection->isValid() && 
        !connection->isExpired(config_.max_lifetime) &&
        total_connections_ <= config_.max_connections) {
        
        // 连接有效，放回连接池
        available_connections_.push(std::move(connection));
        active_connections_--;
        logDebug("连接已释放回连接池");
    } else {
        // 连接无效或过期，销毁连接
        total_connections_--;
        destroyed_connections_++;
        active_connections_--;
        logDebug("销毁无效或过期的连接");
    }
    
    // 通知等待的线程
    pool_condition_.notify_one();
}

TDengineConnectionPool::PoolStats TDengineConnectionPool::getStats() const {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    PoolStats stats;
    stats.total_connections = total_connections_;
    stats.active_connections = active_connections_;
    stats.idle_connections = available_connections_.size();
    stats.pending_requests = pending_requests_;
    stats.created_connections = created_connections_;
    stats.destroyed_connections = destroyed_connections_;
    stats.average_wait_time = (wait_count_ > 0) ? (total_wait_time_ / wait_count_) : 0.0;
    
    return stats;
}

bool TDengineConnectionPool::isHealthy() const {
    if (!initialized_ || shutdown_) {
        return false;
    }
    
    auto stats = getStats();
    
    // 检查是否有足够的连接
    if (stats.total_connections < config_.min_connections) {
        return false;
    }
    
    // 检查是否有过多的等待请求
    if (stats.pending_requests > config_.max_connections) {
        return false;
    }
    
    return true;
}

void TDengineConnectionPool::updateConfig(const TDenginePoolConfig& config) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    // 如果数据库名称发生变化，清理现有连接以确保新连接使用正确的数据库
    bool database_changed = (config_.database != config.database);
    
    config_ = config;
    
    if (database_changed && initialized_) {
        // 清理所有可用连接，强制创建新连接
        while (!available_connections_.empty()) {
            available_connections_.pop();
            total_connections_--;
            destroyed_connections_++;
        }
        logInfo("数据库配置变更，已清理现有连接");
    }
    
    logInfo("TDengine连接池配置已更新");
}

void TDengineConnectionPool::setLogCallback(std::function<void(const std::string&, const std::string&)> callback) {
    log_callback_ = callback;
}

std::unique_ptr<TDengineConnection> TDengineConnectionPool::createConnection() {
    // 连接到TDengine
    TAOS* taos = taos_connect(config_.host.c_str(), 
                             config_.user.c_str(), 
                             config_.password.c_str(), 
                             config_.database.empty() ? nullptr : config_.database.c_str(), 
                             config_.port);
    
    if (!taos) {
        std::string error_msg = "连接TDengine失败: ";
        error_msg += taos_errstr(nullptr);
        logError(error_msg);
        return nullptr;
    }
    
    // 测试连接
    if (!testConnection(taos)) {
        logError("连接测试失败");
        taos_close(taos);
        return nullptr;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto connection = std::make_unique<TDengineConnection>(taos, now);
    
    created_connections_++;
    logDebug("成功创建新的TDengine连接");
    
    return connection;
}

bool TDengineConnectionPool::testConnection(TAOS* taos) {
    if (!taos) {
        return false;
    }
    
    // 测试简单查询
    TAOS_RES* result = taos_query(taos, config_.health_check_query.c_str());
    if (taos_errno(result) != TSDB_CODE_SUCCESS) {
        logError("连接查询测试失败: " + std::string(taos_errstr(result)));
        taos_free_result(result);
        return false;
    }
    
    taos_free_result(result);
    return true;
}

void TDengineConnectionPool::healthCheckLoop() {
    logInfo("健康检查线程已启动");
    
    while (!stop_health_check_) {
        std::this_thread::sleep_for(std::chrono::seconds(config_.health_check_interval));
        
        if (stop_health_check_) {
            break;
        }
        
        try {
            cleanupExpiredConnections();
            ensureMinConnections();
        } catch (const std::exception& e) {
            logError("健康检查过程中发生异常: " + std::string(e.what()));
        }
    }
    
    logInfo("健康检查线程已停止");
}

void TDengineConnectionPool::cleanupExpiredConnections() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (shutdown_) {
        return;
    }
    
    std::queue<std::unique_ptr<TDengineConnection>> valid_connections;
    int cleaned = 0;
    
    // 检查可用连接
    while (!available_connections_.empty()) {
        auto connection = std::move(available_connections_.front());
        available_connections_.pop();
        
        if (connection->isValid() && 
            !connection->isExpired(config_.max_lifetime) &&
            !connection->isIdleTimeout(config_.idle_timeout)) {
            // 执行健康检查
            if (connection->healthCheck(config_.health_check_query)) {
                valid_connections.push(std::move(connection));
            } else {
                cleaned++;
                total_connections_--;
                destroyed_connections_++;
            }
        } else {
            cleaned++;
            total_connections_--;
            destroyed_connections_++;
        }
    }
    
    // 将有效连接放回连接池
    available_connections_ = std::move(valid_connections);
    
    if (cleaned > 0) {
        logDebug("清理了 " + std::to_string(cleaned) + " 个过期或无效的连接");
    }
}

void TDengineConnectionPool::ensureMinConnections() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (shutdown_) {
        return;
    }
    
    int needed = config_.min_connections - total_connections_;
    if (needed <= 0) {
        return;
    }
    
    logDebug("需要创建 " + std::to_string(needed) + " 个连接以维持最小连接数");
    
    for (int i = 0; i < needed; ++i) {
        auto connection = createConnection();
        if (connection) {
            available_connections_.push(std::move(connection));
            total_connections_++;
            logDebug("为维持最小连接数创建了新连接");
        } else {
            logError("创建连接失败，无法维持最小连接数");
            break;
        }
    }
    
    // 通知等待的线程
    pool_condition_.notify_all();
}

void TDengineConnectionPool::logInfo(const std::string& message) const {
    if (log_callback_) {
        log_callback_("INFO", message);
    }
    // 同时使用项目的日志系统
    if (LogManager::getLogger()) {
        LogManager::getLogger()->info("[TDengine连接池] {}", message);
    }
}

void TDengineConnectionPool::logError(const std::string& message) const {
    if (log_callback_) {
        log_callback_("ERROR", message);
    }
    // 同时使用项目的日志系统
    if (LogManager::getLogger()) {
        LogManager::getLogger()->error("[TDengine连接池] {}", message);
    }
}

void TDengineConnectionPool::logDebug(const std::string& message) const {
    if (log_callback_) {
        log_callback_("DEBUG", message);
    }
    // 同时使用项目的日志系统
    if (LogManager::getLogger()) {
        LogManager::getLogger()->debug("[TDengine连接池] {}", message);
    }
}

//=============================================================================
// TDengineConnectionPoolManager Implementation
//=============================================================================

TDengineConnectionPoolManager& TDengineConnectionPoolManager::getInstance() {
    static TDengineConnectionPoolManager instance;
    return instance;
}

bool TDengineConnectionPoolManager::createPool(const std::string& name, const TDenginePoolConfig& config) {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    if (pools_.find(name) != pools_.end()) {
        return false; // 连接池已存在
    }
    
    auto pool = std::make_shared<TDengineConnectionPool>(config);
    if (!pool->initialize()) {
        return false;
    }
    
    pools_[name] = pool;
    return true;
}

std::shared_ptr<TDengineConnectionPool> TDengineConnectionPoolManager::getPool(const std::string& name) {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    auto it = pools_.find(name);
    if (it != pools_.end()) {
        return it->second;
    }
    
    return nullptr;
}

void TDengineConnectionPoolManager::destroyPool(const std::string& name) {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    auto it = pools_.find(name);
    if (it != pools_.end()) {
        it->second->shutdown();
        pools_.erase(it);
    }
}

void TDengineConnectionPoolManager::destroyAllPools() {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    for (auto& pair : pools_) {
        pair.second->shutdown();
    }
    pools_.clear();
}

std::vector<std::string> TDengineConnectionPoolManager::getAllPoolNames() const {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    std::vector<std::string> names;
    for (const auto& pair : pools_) {
        names.push_back(pair.first);
    }
    
    return names;
}

//=============================================================================
// TDengineConnectionGuard Implementation
//=============================================================================

TDengineConnectionGuard::TDengineConnectionGuard(std::shared_ptr<TDengineConnectionPool> pool, int timeout_ms)
    : pool_(pool) {
    if (pool_) {
        connection_ = pool_->getConnection(timeout_ms);
    }
}

TDengineConnectionGuard::~TDengineConnectionGuard() {
    if (connection_ && pool_) {
        pool_->releaseConnection(std::move(connection_));
    }
}