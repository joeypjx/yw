#include "../../include/resource/bmc_storage.h"
#include "../../include/resource/log_manager.h"
#include "../../include/resource/utils.h"
#include "../../include/json.hpp"
#include <taos.h>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstring>
#include <numeric>
#include <algorithm>
#include <cctype>

using namespace std;
using json = nlohmann::json;

// 连接池注入构造函数 - 推荐使用
BMCStorage::BMCStorage(std::shared_ptr<TDengineConnectionPool> connection_pool)
    : m_connection_pool(connection_pool), m_initialized(false), m_owns_connection_pool(false) {
    if (!m_connection_pool) {
        logError("Injected connection pool is null");
    }
}

// 新的连接池构造函数
BMCStorage::BMCStorage(const TDenginePoolConfig& pool_config)
    : m_pool_config(pool_config), m_initialized(false), m_owns_connection_pool(true) {
    m_connection_pool = std::make_shared<TDengineConnectionPool>(m_pool_config);
}

// 兼容性构造函数 - 将旧参数转换为连接池配置
BMCStorage::BMCStorage(const string& host, const string& user, 
                       const string& password, const string& database)
    : m_initialized(false), m_owns_connection_pool(true) {
    m_pool_config = createDefaultPoolConfig();
    m_pool_config.host = host;
    m_pool_config.user = user;
    m_pool_config.password = password;
    m_pool_config.database = database;
    
    m_connection_pool = std::make_shared<TDengineConnectionPool>(m_pool_config);
}

BMCStorage::~BMCStorage() {
    shutdown();
}

bool BMCStorage::initialize() {
    if (m_initialized) {
        logInfo("BMCStorage already initialized");
        return true;
    }
    
    if (!m_connection_pool) {
        logError("Connection pool not created");
        return false;
    }
    
    // 只有当我们拥有连接池时才需要初始化它
    if (m_owns_connection_pool) {
        if (!m_connection_pool->initialize()) {
            logError("Failed to initialize connection pool");
            return false;
        }
    }
    
    // 先标记为已初始化，这样executeQuery就可以正常工作了
    m_initialized = true;
    
    // 如果配置中指定了数据库，切换到该数据库
    if (!m_pool_config.database.empty()) {
        string use_db_sql = "USE " + m_pool_config.database;
        if (!executeQuery(use_db_sql)) {
            last_error_ = "切换到数据库 " + m_pool_config.database + " 失败";
            logError("切换数据库失败: " + last_error_);
            m_initialized = false; // 失败时重置状态
            return false;
        }
    }
    logInfo("BMC存储初始化成功: " + m_pool_config.host);
    return true;
}

void BMCStorage::shutdown() {
    if (!m_initialized) {
        return;
    }
    
    // 只有当我们拥有连接池时才关闭它
    if (m_connection_pool && m_owns_connection_pool) {
        m_connection_pool->shutdown();
    }
    
    m_initialized = false;
    logInfo("BMC存储已关闭");
}

bool BMCStorage::createBMCTables() {
    LogManager::getLogger()->info("📊 创建BMC相关超级表...");
    
    // 先尝试删除可能存在的旧超级表（如果结构不匹配）
    dropOldBMCTables();
    
    // 使用单个连接批量创建BMC超级表，减少连接开销
    TDengineConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) {
        last_error_ = "无法获取数据库连接";
        LogManager::getLogger()->error("Failed to get database connection from pool");
        return false;
    }
    
    TAOS* taos = guard->get();
    
    // 定义所有BMC CREATE STABLE语句
    std::vector<std::pair<std::string, std::string>> bmc_tables = {
        {"bmc_fan_super", R"(
            CREATE TABLE IF NOT EXISTS bmc_fan_super (
                ts TIMESTAMP,
                alarm_type SMALLINT,
                work_mode SMALLINT,
                speed INT
            ) TAGS (
                box_id SMALLINT,
                fan_seq SMALLINT
            )
        )"},
        
        {"bmc_sensor_super", R"(
            CREATE TABLE IF NOT EXISTS bmc_sensor_super (
                ts TIMESTAMP,
                sensor_value INT,
                alarm_type SMALLINT
            ) TAGS (
                box_id SMALLINT,
                slot_id SMALLINT,
                sensor_seq SMALLINT,
                sensor_name NCHAR(16),
                sensor_type SMALLINT,
                host_ip NCHAR(16)
            )
        )"}
    };
    
    // 批量执行CREATE STABLE语句，使用同一个连接
    std::vector<std::string> failed_tables;
    for (const auto& table : bmc_tables) {
        LogManager::getLogger()->debug("创建BMC超级表: " + table.first);
        
        TAOS_RES* result = taos_query(taos, table.second.c_str());
        if (taos_errno(result) != 0) {
            std::string error_msg = "创建BMC超级表失败 " + table.first + ": " + std::string(taos_errstr(result));
            LogManager::getLogger()->error(error_msg);
            failed_tables.push_back(table.first);
        } else {
            LogManager::getLogger()->debug("✅ " + table.first + " 创建成功");
        }
        taos_free_result(result);
    }
    
    if (!failed_tables.empty()) {
        std::string failed_list;
        for (size_t i = 0; i < failed_tables.size(); ++i) {
            if (i > 0) failed_list += ", ";
            failed_list += failed_tables[i];
        }
        last_error_ = "创建BMC超级表失败: " + failed_list;
        LogManager::getLogger()->error(last_error_);
        return false;
    }
    
    LogManager::getLogger()->info("✅ BMC超级表创建成功");
    return true;
}

// 保留这些方法以维持接口兼容性，但它们现在只是简单的包装器
bool BMCStorage::createFanSuperTable() {
    // 这个方法现在被createBMCTables()统一处理，保留是为了兼容性
    LogManager::getLogger()->debug("✅ 风扇超级表创建成功 (通过批量创建)");
    return true;
}

bool BMCStorage::createSensorSuperTable() {
    // 这个方法现在被createBMCTables()统一处理，保留是为了兼容性
    LogManager::getLogger()->debug("✅ 传感器超级表创建成功 (通过批量创建)");
    return true;
}

void BMCStorage::dropOldBMCTables() {
    // 静默删除旧的超级表，如果不存在也不会报错
    executeQuery("DROP TABLE IF EXISTS bmc_fan_super");
    executeQuery("DROP TABLE IF EXISTS bmc_sensor_super");
    LogManager::getLogger()->debug("🗑️ 清理旧BMC超级表");
}



bool BMCStorage::storeBMCData(const UdpInfo& udp_info) {
    // 使用批量插入优化
    return storeBMCDataBatch(udp_info);
}

bool BMCStorage::storeBMCDataBatch(const UdpInfo& udp_info) {
    try {
        // 使用单个连接和批量INSERT语句
        TDengineConnectionGuard guard(m_connection_pool);
        if (!guard.isValid()) {
            last_error_ = "无法获取数据库连接";
            LogManager::getLogger()->error("Failed to get database connection from pool");
            return false;
        }

        TAOS* taos = guard->get();
        
        // 获取当前时间戳
        auto now = chrono::system_clock::now();
        auto timestamp = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count();

        // 1. 批量创建子表语句
        std::vector<std::string> createTableStatements;
        std::ostringstream batchInsertSql;
        batchInsertSql << "INSERT INTO ";

        // 2. 处理风扇数据
        for (int i = 0; i < 2; i++) {
            const auto& fan = udp_info.fan[i];
            string table_name = "bmc_fan_" + to_string(udp_info.boxid) + "_" + to_string(fan.fanseq);
            
            // 创建子表语句
            ostringstream create_sql;
            create_sql << "CREATE TABLE IF NOT EXISTS " << table_name 
                      << " USING bmc_fan_super TAGS ("
                      << static_cast<int>(udp_info.boxid) << ", "
                      << static_cast<int>(fan.fanseq) << ")";
            createTableStatements.push_back(create_sql.str());
            
            // 添加到批量插入语句
            batchInsertSql << table_name << " VALUES ("
                          << timestamp << ", "
                          << ((fan.fanmode >> 4) & 0x0F) << ", "  // alarm_type
                          << (fan.fanmode & 0x0F) << ", "         // work_mode  
                          << fan.fanspeed << ") ";
        }

        // 3. 处理传感器数据
        for (int i = 0; i < 14; i++) {
            const auto& board = udp_info.board[i];
            uint8_t slot_id = Utils::ipmbaddrToSlotId(board.ipmbaddr);
            if (slot_id == 0) {
                continue;
            }
            
            // 计算host_ip
            std::string host_ip = Utils::calculateHostIP(static_cast<int>(udp_info.boxid), static_cast<int>(slot_id));
            
            int sensor_count = board.sensornum < 5 ? board.sensornum : 5;
            for (int j = 0; j < sensor_count; j++) {
                const auto& sensor = board.sensor[j];
                
                string table_name = "bmc_sensor_" + to_string(udp_info.boxid) + "_" + 
                                   to_string(slot_id) + "_" + to_string(sensor.sensorseq);
                
                // 传感器名称清理
                string sensor_name = cleanString(string(reinterpret_cast<const char*>(sensor.sensorname), 6));
                
                // 创建子表语句
                ostringstream create_sql;
                create_sql << "CREATE TABLE IF NOT EXISTS " << table_name 
                          << " USING bmc_sensor_super TAGS ("
                          << static_cast<int>(udp_info.boxid) << ", "
                          << static_cast<int>(slot_id) << ", "
                          << static_cast<int>(sensor.sensorseq) << ", "
                          << "'" << sensor_name << "', "
                          << static_cast<int>(sensor.sensortype) << ", "
                          << "'" << host_ip << "')";
                createTableStatements.push_back(create_sql.str());
                
                // 合并传感器值
                uint16_t sensor_value = (sensor.sensorvalue_H << 8) | sensor.sensorvalue_L;
                
                // 添加到批量插入语句
                batchInsertSql << table_name << " VALUES ("
                              << timestamp << ", "
                              << sensor_value << ", "
                              << static_cast<int>(sensor.sensoralmtype) << ") ";
            }
        }

        // 4. 执行所有CREATE TABLE语句
        for (const auto& createSql : createTableStatements) {
            TAOS_RES* result = taos_query(taos, createSql.c_str());
            if (taos_errno(result) != 0) {
                LogManager::getLogger()->warn("创建BMC子表失败: {}", taos_errstr(result));
                // 继续执行，不中断整个过程
            }
            taos_free_result(result);
        }

        // 5. 执行批量插入
        string finalSql = batchInsertSql.str();
        LogManager::getLogger()->debug("执行BMC批量插入: {}", finalSql);
        
        TAOS_RES* result = taos_query(taos, finalSql.c_str());
        if (taos_errno(result) != 0) {
            last_error_ = "BMC批量插入失败: " + string(taos_errstr(result));
            LogManager::getLogger()->error("BMC批量插入失败: {}", taos_errstr(result));
            taos_free_result(result);
            return false;
        }
        
        taos_free_result(result);
        LogManager::getLogger()->debug("✅ BMC批量数据存储成功: box_id={}", udp_info.boxid);
        return true;
        
    } catch (const exception& e) {
        last_error_ = "BMC批量存储数据异常: " + string(e.what());
        LogManager::getLogger()->error("BMC批量存储数据异常: {}", e.what());
        return false;
    }
}
    
bool BMCStorage::storeBMCDataFromJson(const string& json_data) {
    try {
        json j = json::parse(json_data);
        
        // 解析header信息
        if (!j.contains("header") || !j.contains("fans") || !j.contains("boards")) {
            last_error_ = "JSON数据格式不正确，缺少必要字段";
            return false;
        }
        
        auto header = j["header"];
        uint8_t box_id = header["box_id"];
        uint32_t timestamp = header["timestamp"];
        
        // 获取当前时间戳
        auto now = chrono::system_clock::now();
        auto ts = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count();
        
        // 存储风扇数据
        auto fans = j["fans"];
        for (const auto& fan : fans) {
            uint8_t fan_seq = fan["sequence"];
            uint8_t alarm_type = fan["mode"]["alarm_type"];
            uint8_t work_mode = fan["mode"]["work_mode"];
            uint32_t speed = fan["speed"];
            
            string table_name = "bmc_fan_" + to_string(box_id) + "_" + to_string(fan_seq);
            
            // 创建子表
            ostringstream create_sql;
            create_sql << "CREATE TABLE IF NOT EXISTS " << table_name 
                      << " USING bmc_fan_super TAGS ("
                      << static_cast<int>(box_id) << ", "
                      << static_cast<int>(fan_seq) << ")";
            
            if (executeQuery(create_sql.str())) {
                // 插入数据
                ostringstream insert_sql;
                insert_sql << "INSERT INTO " << table_name << " VALUES ("
                          << ts << ", "
                          << static_cast<int>(alarm_type) << ", "
                          << static_cast<int>(work_mode) << ", "
                          << speed << ")";
                
                executeQuery(insert_sql.str());
            }
        }
        
        // 存储传感器数据
        auto boards = j["boards"];
        for (const auto& board : boards) {
            uint8_t slot_id =  Utils::ipmbaddrToSlotId(board["ipmb_address"]);
            if (slot_id == 0) {
                continue;
            }
            auto sensors = board["sensors"];
            
            // 计算host_ip
            std::string host_ip = Utils::calculateHostIP(static_cast<int>(box_id), static_cast<int>(slot_id));
            
            for (const auto& sensor : sensors) {
                uint8_t sensor_seq = sensor["sequence"];
                uint8_t sensor_type = sensor["type"];
                string sensor_name = cleanString(sensor["name"]);
                uint16_t sensor_value = sensor["value"];
                uint8_t alarm_type = sensor["alarm_type"];
                
                string table_name = "bmc_sensor_" + to_string(box_id) + "_" + 
                                   to_string(slot_id) + "_" + to_string(sensor_seq);
                
                // 创建子表
                ostringstream create_sql;
                create_sql << "CREATE TABLE IF NOT EXISTS " << table_name 
                          << " USING bmc_sensor_super TAGS ("
                          << static_cast<int>(box_id) << ", "
                          << static_cast<int>(slot_id) << ", "
                          << static_cast<int>(sensor_seq) << ", "
                          << "'" << sensor_name << "', "
                          << static_cast<int>(sensor_type) << ", "
                          << "'" << host_ip << "')";
                
                if (executeQuery(create_sql.str())) {
                    // 插入数据
                    ostringstream insert_sql;
                    insert_sql << "INSERT INTO " << table_name << " VALUES ("
                              << ts << ", "
                              << sensor_value << ", "
                              << static_cast<int>(alarm_type) << ")";
                    
                    executeQuery(insert_sql.str());
                }
            }
        }
        
        LogManager::getLogger()->debug("✅ 从JSON存储BMC数据成功: box_id={}", box_id);
        return true;
        
    } catch (const exception& e) {
        last_error_ = "解析JSON数据异常: " + string(e.what());
        LogManager::getLogger()->error("解析JSON数据异常: {}", e.what());
        return false;
    }
}

string BMCStorage::getLastError() const {
    return last_error_;
}

bool BMCStorage::executeQuery(const string& sql) {
    if (!m_initialized) {
        last_error_ = "BMCStorage not initialized";
        logError("BMCStorage not initialized");
        return false;
    }
    
    TDengineConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) {
        last_error_ = "Failed to get database connection from pool";
        logError("Failed to get database connection from pool");
        return false;
    }
    
    logDebug("Executing SQL: " + sql);
    
    TAOS* taos = guard->get();
    TAOS_RES* result = taos_query(taos, sql.c_str());
    int code = taos_errno(result);
    
    if (code != 0) {
        last_error_ = "SQL执行失败: " + string(taos_errstr(result)) + " SQL: " + sql;
        logError("SQL执行失败: " + string(taos_errstr(result)) + " - SQL: " + sql);
        taos_free_result(result);
        return false;
    }
    
    taos_free_result(result);
    return true;
}

string BMCStorage::cleanString(const string& str) {
    string result;
    result.reserve(str.length());
    
    for (char c : str) {
        if (c == '\0') break;  // 遇到null字符就停止
        if (isalnum(c) || c == '_') {
            result += c;
        } else {
            result += '_';
        }
    }
    
    return result.empty() ? "unknown" : result;
}

vector<BMCQueryResult> BMCStorage::executeBMCQuerySQL(const string& sql) {
    vector<BMCQueryResult> results;
    
    if (!m_initialized) {
        last_error_ = "BMCStorage not initialized";
        logError("BMCStorage not initialized");
        return results;
    }
    
    logDebug("BMCStorage: 执行查询: " + sql);
    
    TDengineConnectionGuard guard(m_connection_pool);
    if (!guard.isValid()) {
        last_error_ = "Failed to get database connection from pool";
        logError("Failed to get database connection from pool");
        return results;
    }
    
    TAOS* taos = guard->get();
    TAOS_RES* res = taos_query(taos, sql.c_str());
    int code = taos_errno(res);
    
    if (code != 0) {
        last_error_ = "SQL执行失败: " + string(taos_errstr(res)) + " SQL: " + sql;
        logError("BMCStorage: SQL执行失败: " + string(taos_errstr(res)) + " - SQL: " + sql);
        taos_free_result(res);
        return results;
    }
    
    // 获取字段信息
    int field_count = taos_field_count(res);
    TAOS_FIELD* fields = taos_fetch_fields(res);
    
    if (field_count == 0) {
        taos_free_result(res);
        return results;
    }
    
    // 处理查询结果
    TAOS_ROW row;
    while ((row = taos_fetch_row(res))) {
        int* lengths = taos_fetch_lengths(res);
        
        BMCQueryResult result;
        result.timestamp = chrono::system_clock::now();
        
        // 遍历所有字段
        for (int i = 0; i < field_count; i++) {
            if (row[i] == nullptr) continue;
            
            string field_name = fields[i].name;
            
            if (field_name == "ts") {
                // 时间戳字段
                if (fields[i].type == TSDB_DATA_TYPE_TIMESTAMP) {
                    int64_t timestamp = *(int64_t*)row[i];
                    result.timestamp = chrono::system_clock::from_time_t(timestamp / 1000) + 
                                     chrono::milliseconds(timestamp % 1000);
                }
            } else if (field_name == "box_id" || field_name == "slot_id" || 
                      field_name == "fan_seq" || field_name == "sensor_seq" ||
                      field_name == "sensor_name" || field_name == "sensor_type") {
                // 标签字段
                if (fields[i].type == TSDB_DATA_TYPE_NCHAR || fields[i].type == TSDB_DATA_TYPE_BINARY) {
                    result.labels[field_name] = string((char*)row[i], lengths[i]);
                } else if (fields[i].type == TSDB_DATA_TYPE_TINYINT) {
                    result.labels[field_name] = to_string(*(int8_t*)row[i]);
                } else if (fields[i].type == TSDB_DATA_TYPE_SMALLINT) {
                    result.labels[field_name] = to_string(*(int16_t*)row[i]);
                } else if (fields[i].type == TSDB_DATA_TYPE_INT) {
                    result.labels[field_name] = to_string(*(int*)row[i]);
                }
            } else {
                // 数值字段 - 存储到 metrics map 中
                double value = 0.0;
                if (fields[i].type == TSDB_DATA_TYPE_FLOAT) {
                    value = *(float*)row[i];
                } else if (fields[i].type == TSDB_DATA_TYPE_DOUBLE) {
                    value = *(double*)row[i];
                } else if (fields[i].type == TSDB_DATA_TYPE_INT) {
                    value = *(int*)row[i];
                } else if (fields[i].type == TSDB_DATA_TYPE_BIGINT) {
                    value = *(int64_t*)row[i];
                } else if (fields[i].type == TSDB_DATA_TYPE_SMALLINT) {
                    value = *(int16_t*)row[i];
                } else if (fields[i].type == TSDB_DATA_TYPE_TINYINT) {
                    value = *(int8_t*)row[i];
                }
                result.metrics[field_name] = value;
            }
        }
        
        results.push_back(result);
    }
    
    taos_free_result(res);
    
    LogManager::getLogger()->debug("BMCStorage: 查询返回 {} 行数据", results.size());
    return results;
}

chrono::seconds BMCStorage::parseTimeRange(const string& time_range) {
    if (time_range.empty()) {
        return chrono::seconds(3600); // 默认1小时
    }
    
    string number_str;
    char unit = 's';
    
    // 分离数字和单位
    for (char c : time_range) {
        if (isdigit(c)) {
            number_str += c;
        } else {
            unit = c;
            break;
        }
    }
    
    if (number_str.empty()) {
        return chrono::seconds(3600); // 默认1小时
    }
    
    int number = stoi(number_str);
    
    switch (unit) {
        case 's': return chrono::seconds(number);
        case 'm': return chrono::seconds(number * 60);
        case 'h': return chrono::seconds(number * 3600);
        case 'd': return chrono::seconds(number * 86400);
        default: return chrono::seconds(number); // 默认按秒处理
    }
}

BMCRangeData BMCStorage::getBMCRangeData(uint8_t box_id, 
                                         const string& time_range,
                                         const vector<string>& metrics) {
    BMCRangeData rangeData;
    rangeData.box_id = box_id;
    rangeData.time_range = time_range;
    rangeData.metrics_types = metrics;
    
    if (!m_initialized) {
        logError("BMCStorage not initialized");
        return rangeData;
    }
    
    // 记录查询时间范围
    rangeData.end_time = chrono::system_clock::now();
    auto duration = parseTimeRange(time_range);
    rangeData.start_time = rangeData.end_time - duration;
    
    try {
        for (const string& metric : metrics) {
            BMCRangeData::TimeSeriesData timeSeriesData;
            timeSeriesData.metric_type = metric;
            
            if (metric == "fan") {
                // 查询风扇数据
                string sql = "SELECT * FROM bmc_fan_super WHERE box_id = " + to_string(box_id) +
                            " AND ts > NOW() - " + time_range + 
                            " ORDER BY ts ASC";
                timeSeriesData.data_points = executeBMCQuerySQL(sql);
                
            } else if (metric == "sensor") {
                // 查询传感器数据
                string sql = "SELECT * FROM bmc_sensor_super WHERE box_id = " + to_string(box_id) +
                            " AND ts > NOW() - " + time_range + 
                            " ORDER BY ts ASC";
                timeSeriesData.data_points = executeBMCQuerySQL(sql);
            }
            
            if (!timeSeriesData.data_points.empty()) {
                rangeData.time_series.push_back(timeSeriesData);
            }
        }
        
        LogManager::getLogger()->debug("BMCStorage: 获取box_id={} {}时间段内数据: {} 种指标类型, 总共 {} 个数据点", 
                                     box_id, time_range, rangeData.time_series.size(),
                                     accumulate(rangeData.time_series.begin(), rangeData.time_series.end(), 0,
                                               [](int sum, const auto& ts) { return sum + ts.data_points.size(); }));
        
    } catch (const exception& e) {
        LogManager::getLogger()->error("BMCStorage: 获取范围数据失败 box_id={}: {}", box_id, e.what());
        last_error_ = "获取BMC范围数据异常: " + string(e.what());
    }
    
    return rangeData;
}

// BMCRangeData的to_json实现
nlohmann::json BMCRangeData::to_json() const {
    nlohmann::json j;
    
    j["box_id"] = box_id;
    j["time_range"] = time_range;
    j["start_time"] = chrono::duration_cast<chrono::milliseconds>(start_time.time_since_epoch()).count();
    j["end_time"] = chrono::duration_cast<chrono::milliseconds>(end_time.time_since_epoch()).count();
    
    // 构建metrics对象
    nlohmann::json metrics;
    
    for (const auto& ts : time_series) {
        if (ts.metric_type == "fan") {
            // 风扇数据按fan_seq分组
            nlohmann::json fan_groups;
            map<string, nlohmann::json> fan_data;
            
            for (const auto& point : ts.data_points) {
                string fan_key = point.labels.count("fan_seq") ? 
                               ("fan_" + point.labels.at("fan_seq")) : "fan_0";
                
                nlohmann::json fan_point;
                fan_point["timestamp"] = chrono::duration_cast<chrono::milliseconds>(
                    point.timestamp.time_since_epoch()).count();
                
                // 添加所有指标值
                for (const auto& metric : point.metrics) {
                    fan_point[metric.first] = metric.second;
                }
                
                // 添加标签信息
                for (const auto& label : point.labels) {
                    fan_point[label.first] = label.second;
                }
                
                if (fan_data.find(fan_key) == fan_data.end()) {
                    fan_data[fan_key] = nlohmann::json::array();
                }
                fan_data[fan_key].push_back(fan_point);
            }
            
            for (const auto& fan : fan_data) {
                fan_groups[fan.first] = fan.second;
            }
            metrics["fan"] = fan_groups;
            
        } else if (ts.metric_type == "sensor") {
            // 传感器数据按slot_id和sensor_seq分组
            nlohmann::json sensor_groups;
            map<string, nlohmann::json> sensor_data;
            
            for (const auto& point : ts.data_points) {
                string slot_key = point.labels.count("slot_id") ? 
                                point.labels.at("slot_id") : "0";
                string sensor_key = point.labels.count("sensor_seq") ? 
                                  point.labels.at("sensor_seq") : "0";
                string combined_key = "slot_" + slot_key + "_sensor_" + sensor_key;
                
                nlohmann::json sensor_point;
                sensor_point["timestamp"] = chrono::duration_cast<chrono::milliseconds>(
                    point.timestamp.time_since_epoch()).count();
                
                // 添加所有指标值
                for (const auto& metric : point.metrics) {
                    sensor_point[metric.first] = metric.second;
                }
                
                // 添加标签信息
                for (const auto& label : point.labels) {
                    sensor_point[label.first] = label.second;
                }
                
                if (sensor_data.find(combined_key) == sensor_data.end()) {
                    sensor_data[combined_key] = nlohmann::json::array();
                }
                sensor_data[combined_key].push_back(sensor_point);
            }
            
            for (const auto& sensor : sensor_data) {
                sensor_groups[sensor.first] = sensor.second;
            }
            metrics["sensor"] = sensor_groups;
        }
    }
    
    j["metrics"] = metrics;
    
    return j;
}

// 新增的连接池相关方法
TDengineConnectionPool::PoolStats BMCStorage::getConnectionPoolStats() const {
    if (!m_connection_pool) {
        return TDengineConnectionPool::PoolStats{};
    }
    return m_connection_pool->getStats();
}

void BMCStorage::updateConnectionPoolConfig(const TDenginePoolConfig& config) {
    m_pool_config = config;
    if (m_connection_pool) {
        m_connection_pool->updateConfig(config);
    }
    logInfo("Connection pool configuration updated");
}

TDenginePoolConfig BMCStorage::createDefaultPoolConfig() const {
    TDenginePoolConfig config;
    config.host = "localhost";
    config.port = 6030;
    config.user = "test";
    config.password = "HZ715Net";
    config.database = "resource";
    config.locale = "C";
    config.charset = "UTF-8";
    config.timezone = "";
    
    // 连接池配置
    config.min_connections = 2;
    config.max_connections = 8;
    config.initial_connections = 3;
    
    // 超时配置
    config.connection_timeout = 30;
    config.idle_timeout = 600;      // 10分钟
    config.max_lifetime = 3600;     // 1小时
    config.acquire_timeout = 10;
    
    // 健康检查配置
    config.health_check_interval = 60;
    config.health_check_query = "SELECT SERVER_VERSION()";
    
    return config;
}

// 日志辅助方法
void BMCStorage::logInfo(const string& message) const {
    LogManager::getLogger()->info("BMCStorage: {}", message);
}

void BMCStorage::logError(const string& message) const {
    LogManager::getLogger()->error("BMCStorage: {}", message);
}

void BMCStorage::logDebug(const string& message) const {
    LogManager::getLogger()->debug("BMCStorage: {}", message);
}