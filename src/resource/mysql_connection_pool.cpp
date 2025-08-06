#include "mysql_connection_pool.h"
#include "log_manager.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <cstring>

// MySQL错误码兼容性定义
#ifndef CR_SERVER_GONE_ERROR
#define CR_SERVER_GONE_ERROR 2006
#endif

#ifndef CR_SERVER_LOST
#define CR_SERVER_LOST 2013
#endif

#ifndef CR_CONN_HOST_ERROR
#define CR_CONN_HOST_ERROR 2003
#endif

//=============================================================================
// MySQLConnection Implementation
//=============================================================================

MySQLConnection::MySQLConnection(MYSQL* mysql, const std::chrono::steady_clock::time_point& created_time)
    : mysql_(mysql), created_time_(created_time), last_used_time_(created_time) {
}

MySQLConnection::~MySQLConnection() {
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
    }
}

MySQLConnection::MySQLConnection(MySQLConnection&& other) noexcept
    : mysql_(other.mysql_), created_time_(other.created_time_), last_used_time_(other.last_used_time_) {
    other.mysql_ = nullptr;
}

MySQLConnection& MySQLConnection::operator=(MySQLConnection&& other) noexcept {
    if (this != &other) {
        if (mysql_) {
            mysql_close(mysql_);
        }
        mysql_ = other.mysql_;
        created_time_ = other.created_time_;
        last_used_time_ = other.last_used_time_;
        other.mysql_ = nullptr;
    }
    return *this;
}

bool MySQLConnection::isValid() const {
    if (!mysql_) {
        return false;
    }
    
    // 检查连接是否仍然活跃
    return mysql_ping(mysql_) == 0;
}

bool MySQLConnection::isExpired(int max_lifetime_seconds) const {
    auto now = std::chrono::steady_clock::now();
    auto lifetime = std::chrono::duration_cast<std::chrono::seconds>(now - created_time_).count();
    return lifetime >= max_lifetime_seconds;
}

bool MySQLConnection::isIdleTimeout(int idle_timeout_seconds) const {
    auto now = std::chrono::steady_clock::now();
    auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(now - last_used_time_).count();
    return idle_time >= idle_timeout_seconds;
}

void MySQLConnection::updateLastUsed() {
    last_used_time_ = std::chrono::steady_clock::now();
}

bool MySQLConnection::healthCheck(const std::string& query) {
    if (!mysql_) {
        return false;
    }
    
    // 先检查ping
    if (mysql_ping(mysql_) != 0) {
        return false;
    }
    
    // 执行健康检查查询
    if (mysql_query(mysql_, query.c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(mysql_);
    if (result) {
        mysql_free_result(result);
    }
    
    updateLastUsed();
    return true;
}

//=============================================================================
// MySQLConnectionPool Implementation
//=============================================================================

MySQLConnectionPool::MySQLConnectionPool(const MySQLPoolConfig& config)
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

MySQLConnectionPool::~MySQLConnectionPool() {
    shutdownForce();
}

bool MySQLConnectionPool::initialize() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (initialized_) {
        logDebug("连接池已经初始化");
        return true;
    }
    
    logInfo("正在初始化MySQL连接池...");
    
    // 初始化MySQL库
    if (mysql_library_init(0, nullptr, nullptr) != 0) {
        logError("初始化MySQL库失败");
        return false;
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
            mysql_library_end();
            return false;
        }
        
        available_connections_.push(std::move(connection));
        total_connections_++;
    }
    
    initialized_ = true;
    
    // 启动健康检查线程
    stop_health_check_ = false;
    health_check_thread_ = std::thread(&MySQLConnectionPool::healthCheckLoop, this);
    
    logInfo("MySQL连接池初始化成功，创建了 " + std::to_string(config_.initial_connections) + " 个连接");
    return true;
}

void MySQLConnectionPool::setShutdownTimeout(int timeout_ms) {
    shutdown_timeout_ms_ = timeout_ms;
    logInfo("设置关闭超时时间为 " + std::to_string(timeout_ms) + " 毫秒");
}

void MySQLConnectionPool::shutdownFast() {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    if (shutdown_) {
        return;
    }
    
    logInfo("正在快速关闭MySQL连接池...");
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
    
    // 不等待活跃连接，直接清理
    logInfo("跳过等待活跃连接，直接清理连接池");
    
    // 清理所有连接
    while (!available_connections_.empty()) {
        available_connections_.pop();
    }
    
    total_connections_ = 0;
    initialized_ = false;
    
    // 清理MySQL库
    mysql_library_end();
    
    logInfo("MySQL连接池已快速关闭");
}

void MySQLConnectionPool::shutdown() {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    if (shutdown_) {
        return;
    }
    
    logInfo("正在关闭MySQL连接池...");
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
    
    // 等待所有活跃连接返回，但设置超时
    auto start_wait = std::chrono::steady_clock::now();
    while (active_connections_ > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_wait).count();
        
        if (elapsed >= shutdown_timeout_ms_) {
            logWarning("关闭超时，仍有 " + std::to_string(active_connections_.load()) + " 个活跃连接未返回");
            break;
        }
        
        logDebug("等待 " + std::to_string(active_connections_.load()) + " 个活跃连接返回... (已等待 " + std::to_string(elapsed) + "ms)");
        pool_condition_.wait_for(lock, std::chrono::milliseconds(100));  // 减少等待间隔到100ms
    }
    
    // 清理所有连接
    while (!available_connections_.empty()) {
        available_connections_.pop();
    }
    
    total_connections_ = 0;
    initialized_ = false;
    
    // 清理MySQL库
    mysql_library_end();
    
    logInfo("MySQL连接池已关闭");
}

void MySQLConnectionPool::shutdownForce() {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    if (shutdown_) {
        return;
    }
    
    logInfo("正在强制关闭MySQL连接池...");
    shutdown_ = true;
    
    // 立即停止健康检查线程（不等待）
    stop_health_check_ = true;
    lock.unlock();
    
    // 强制分离健康检查线程
    if (health_check_thread_.joinable()) {
        health_check_thread_.detach();
    }
    
    lock.lock();
    
    // 通知所有等待的线程
    pool_condition_.notify_all();
    
    // 立即清理所有连接
    while (!available_connections_.empty()) {
        available_connections_.pop();
    }
    
    total_connections_ = 0;
    initialized_ = false;
    
    // 清理MySQL库
    mysql_library_end();
    
    logInfo("MySQL连接池已强制关闭");
}

std::unique_ptr<MySQLConnection> MySQLConnectionPool::getConnection(int timeout_ms) {
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

void MySQLConnectionPool::releaseConnection(std::unique_ptr<MySQLConnection> connection) {
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

MySQLConnectionPool::PoolStats MySQLConnectionPool::getStats() const {
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

bool MySQLConnectionPool::isHealthy() const {
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

void MySQLConnectionPool::setLogCallback(std::function<void(const std::string&, const std::string&)> callback) {
    log_callback_ = callback;
}

std::unique_ptr<MySQLConnection> MySQLConnectionPool::createConnection() {
    MYSQL* mysql = mysql_init(nullptr);
    if (!mysql) {
        logError("初始化MySQL连接失败");
        return nullptr;
    }
    
    // 设置连接选项
    bool reconnect = config_.auto_reconnect;
    mysql_options(mysql, MYSQL_OPT_RECONNECT, &reconnect);
    
    unsigned int timeout = config_.connection_timeout;
    mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, &timeout);
    mysql_options(mysql, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
    
    if (!config_.charset.empty()) {
        mysql_options(mysql, MYSQL_SET_CHARSET_NAME, config_.charset.c_str());
    }
    
    // 设置最大包大小
    mysql_options(mysql, MYSQL_OPT_MAX_ALLOWED_PACKET, &config_.max_allowed_packet);
    
    // 连接到数据库
    if (!mysql_real_connect(mysql, 
                           config_.host.c_str(),
                           config_.user.c_str(),
                           config_.password.c_str(),
                           config_.database.empty() ? nullptr : config_.database.c_str(),
                           config_.port,
                           nullptr,
                           CLIENT_MULTI_STATEMENTS)) {
        std::string error_msg = "连接MySQL失败: ";
        error_msg += mysql_error(mysql);
        logError(error_msg);
        mysql_close(mysql);
        return nullptr;
    }
    
    // 测试连接
    if (!testConnection(mysql)) {
        logError("连接测试失败");
        mysql_close(mysql);
        return nullptr;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto connection = std::make_unique<MySQLConnection>(mysql, now);
    
    created_connections_++;
    logDebug("成功创建新的MySQL连接");
    
    return connection;
}

bool MySQLConnectionPool::testConnection(MYSQL* mysql) {
    if (!mysql) {
        return false;
    }
    
    // 测试ping
    if (mysql_ping(mysql) != 0) {
        logError("连接ping测试失败: " + std::string(mysql_error(mysql)));
        return false;
    }
    
    // 测试简单查询
    if (mysql_query(mysql, config_.health_check_query.c_str()) != 0) {
        logError("连接查询测试失败: " + std::string(mysql_error(mysql)));
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(mysql);
    if (result) {
        mysql_free_result(result);
    }
    
    return true;
}

void MySQLConnectionPool::healthCheckLoop() {
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

void MySQLConnectionPool::cleanupExpiredConnections() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    if (shutdown_) {
        return;
    }
    
    std::queue<std::unique_ptr<MySQLConnection>> valid_connections;
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

void MySQLConnectionPool::ensureMinConnections() {
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

void MySQLConnectionPool::logInfo(const std::string& message) const {
    if (log_callback_) {
        log_callback_("INFO", message);
    }
    // 同时使用项目的日志系统
    if (LogManager::getLogger()) {
        LogManager::getLogger()->info("[MySQL连接池] {}", message);
    }
}

void MySQLConnectionPool::logError(const std::string& message) const {
    if (log_callback_) {
        log_callback_("ERROR", message);
    }
    // 同时使用项目的日志系统
    if (LogManager::getLogger()) {
        LogManager::getLogger()->error("[MySQL连接池] {}", message);
    }
}

void MySQLConnectionPool::logDebug(const std::string& message) const {
    if (log_callback_) {
        log_callback_("DEBUG", message);
    }
    // 同时使用项目的日志系统
    if (LogManager::getLogger()) {
        LogManager::getLogger()->debug("[MySQL连接池] {}", message);
    }
}

void MySQLConnectionPool::logWarning(const std::string& message) const {
    if (log_callback_) {
        log_callback_("WARNING", message);
    }
    // 同时使用项目的日志系统
    if (LogManager::getLogger()) {
        LogManager::getLogger()->warn("[MySQL连接池] {}", message);
    }
}

//=============================================================================
// MySQLConnectionPoolManager Implementation
//=============================================================================

MySQLConnectionPoolManager& MySQLConnectionPoolManager::getInstance() {
    static MySQLConnectionPoolManager instance;
    return instance;
}

bool MySQLConnectionPoolManager::createPool(const std::string& name, const MySQLPoolConfig& config) {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    if (pools_.find(name) != pools_.end()) {
        return false; // 连接池已存在
    }
    
    auto pool = std::make_shared<MySQLConnectionPool>(config);
    if (!pool->initialize()) {
        return false;
    }
    
    pools_[name] = pool;
    return true;
}

std::shared_ptr<MySQLConnectionPool> MySQLConnectionPoolManager::getPool(const std::string& name) {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    auto it = pools_.find(name);
    if (it != pools_.end()) {
        return it->second;
    }
    
    return nullptr;
}

void MySQLConnectionPoolManager::destroyPool(const std::string& name) {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    auto it = pools_.find(name);
    if (it != pools_.end()) {
        it->second->shutdown();
        pools_.erase(it);
    }
}

void MySQLConnectionPoolManager::destroyAllPools() {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    for (auto& pair : pools_) {
        pair.second->shutdown();
    }
    pools_.clear();
}

std::vector<std::string> MySQLConnectionPoolManager::getAllPoolNames() const {
    std::lock_guard<std::mutex> lock(pools_mutex_);
    
    std::vector<std::string> names;
    for (const auto& pair : pools_) {
        names.push_back(pair.first);
    }
    
    return names;
}

//=============================================================================
// MySQLConnectionGuard Implementation
//=============================================================================

MySQLConnectionGuard::MySQLConnectionGuard(std::shared_ptr<MySQLConnectionPool> pool, int timeout_ms)
    : pool_(pool) {
    if (pool_) {
        connection_ = pool_->getConnection(timeout_ms);
    }
}

MySQLConnectionGuard::~MySQLConnectionGuard() {
    if (connection_ && pool_) {
        pool_->releaseConnection(std::move(connection_));
    }
}