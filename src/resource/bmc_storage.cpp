#include "../../include/resource/bmc_storage.h"
#include "../../include/resource/log_manager.h"
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

BMCStorage::BMCStorage(const string& host, const string& user, 
                       const string& password, const string& database)
    : host_(host), user_(user), password_(password), database_(database), taos_(nullptr) {
}

BMCStorage::~BMCStorage() {
    disconnect();
}

bool BMCStorage::connect() {
    // 初始化TDengine客户端
    taos_init();
    
    // 连接数据库
    taos_ = taos_connect(host_.c_str(), user_.c_str(), password_.c_str(), nullptr, 0);
    if (taos_ == nullptr) {
        last_error_ = "连接TDengine失败: " + string(taos_errstr(nullptr));
        LogManager::getLogger()->error("BMC存储连接失败: {}", last_error_);
        return false;
    }
    
    // 使用数据库
    string use_db_sql = "USE " + database_;
    if (!executeSql(use_db_sql)) {
        last_error_ = "切换到数据库 " + database_ + " 失败";
        LogManager::getLogger()->error("BMC存储切换数据库失败: {}", last_error_);
        return false;
    }
    
    LogManager::getLogger()->info("✅ BMC存储连接成功: {}", host_);
    return true;
}

void BMCStorage::disconnect() {
    if (taos_) {
        taos_close(taos_);
        taos_ = nullptr;
        taos_cleanup();
    }
}

bool BMCStorage::createBMCTables() {
    LogManager::getLogger()->info("📊 创建BMC相关超级表...");
    
    if (!createFanSuperTable()) {
        return false;
    }
    
    if (!createSensorSuperTable()) {
        return false;
    }
    
    LogManager::getLogger()->info("✅ BMC超级表创建成功");
    return true;
}

bool BMCStorage::createFanSuperTable() {
    string sql = R"(
        CREATE TABLE IF NOT EXISTS bmc_fan_super (
            ts TIMESTAMP,
            alarm_type TINYINT,
            work_mode TINYINT,
            speed INT
        ) TAGS (
            box_id TINYINT,
            fan_seq TINYINT
        )
    )";
    
    if (!executeSql(sql)) {
        last_error_ = "创建风扇超级表失败";
        return false;
    }
    
    LogManager::getLogger()->debug("✅ 风扇超级表创建成功");
    return true;
}

bool BMCStorage::createSensorSuperTable() {
    string sql = R"(
        CREATE TABLE IF NOT EXISTS bmc_sensor_super (
            ts TIMESTAMP,
            sensor_value INT,
            alarm_type TINYINT
        ) TAGS (
            box_id TINYINT,
            slot_id TINYINT,
            sensor_seq TINYINT,
            sensor_name NCHAR(16),
            sensor_type TINYINT
        )
    )";
    
    if (!executeSql(sql)) {
        last_error_ = "创建传感器超级表失败";
        return false;
    }
    
    LogManager::getLogger()->debug("✅ 传感器超级表创建成功");
    return true;
}

bool BMCStorage::storeFanData(const UdpInfo& udp_info) {
    try {
        // 获取当前时间戳
        auto now = chrono::system_clock::now();
        auto timestamp = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count();
        
        for (int i = 0; i < 2; i++) {
            const auto& fan = udp_info.fan[i];
            
            // 创建子表名
            string table_name = "bmc_fan_" + to_string(udp_info.boxid) + "_" + to_string(fan.fanseq);
            
            // 创建子表（如果不存在）
            ostringstream create_sql;
            create_sql << "CREATE TABLE IF NOT EXISTS " << table_name 
                      << " USING bmc_fan_super TAGS ("
                      << static_cast<int>(udp_info.boxid) << ", "
                      << static_cast<int>(fan.fanseq) << ")";
            
            if (!executeSql(create_sql.str())) {
                continue;
            }
            
            // 插入数据
            ostringstream insert_sql;
            insert_sql << "INSERT INTO " << table_name << " VALUES ("
                      << timestamp << ", "
                      << ((fan.fanmode >> 4) & 0x0F) << ", "  // alarm_type
                      << (fan.fanmode & 0x0F) << ", "         // work_mode  
                      << fan.fanspeed << ")";
            
            if (!executeSql(insert_sql.str())) {
                LogManager::getLogger()->warn("插入风扇数据失败: box_id={}, fan_seq={}", 
                                            udp_info.boxid, fan.fanseq);
            }
        }
        
        return true;
    } catch (const exception& e) {
        last_error_ = "存储风扇数据异常: " + string(e.what());
        LogManager::getLogger()->error("存储风扇数据异常: {}", e.what());
        return false;
    }
}

bool BMCStorage::storeSensorData(const UdpInfo& udp_info) {
    try {
        // 获取当前时间戳
        auto now = chrono::system_clock::now();
        auto timestamp = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count();
        
        for (int i = 0; i < 14; i++) {
            const auto& board = udp_info.board[i];
            uint8_t slot_id = board.ipmbaddr;
            
            int sensor_count = board.sensornum < 5 ? board.sensornum : 5;
            for (int j = 0; j < sensor_count; j++) {
                const auto& sensor = board.sensor[j];
                
                // 创建子表名
                string table_name = "bmc_sensor_" + to_string(udp_info.boxid) + "_" + 
                                   to_string(slot_id) + "_" + to_string(sensor.sensorseq);
                
                // 传感器名称清理
                string sensor_name = cleanString(string(reinterpret_cast<const char*>(sensor.sensorname), 6));
                
                // 创建子表（如果不存在）
                ostringstream create_sql;
                create_sql << "CREATE TABLE IF NOT EXISTS " << table_name 
                          << " USING bmc_sensor_super TAGS ("
                          << static_cast<int>(udp_info.boxid) << ", "
                          << static_cast<int>(slot_id) << ", "
                          << static_cast<int>(sensor.sensorseq) << ", "
                          << "'" << sensor_name << "', "
                          << static_cast<int>(sensor.sensortype) << ")";
                
                if (!executeSql(create_sql.str())) {
                    continue;
                }
                
                // 合并传感器值
                uint16_t sensor_value = (sensor.sensorvalue_H << 8) | sensor.sensorvalue_L;
                
                // 插入数据
                ostringstream insert_sql;
                insert_sql << "INSERT INTO " << table_name << " VALUES ("
                          << timestamp << ", "
                          << sensor_value << ", "
                          << static_cast<int>(sensor.sensoralmtype) << ")";
                
                if (!executeSql(insert_sql.str())) {
                    LogManager::getLogger()->warn("插入传感器数据失败: box_id={}, slot_id={}, sensor_seq={}", 
                                                udp_info.boxid, slot_id, sensor.sensorseq);
                }
            }
        }
        
        return true;
    } catch (const exception& e) {
        last_error_ = "存储传感器数据异常: " + string(e.what());
        LogManager::getLogger()->error("存储传感器数据异常: {}", e.what());
        return false;
    }
}

bool BMCStorage::storeBMCData(const UdpInfo& udp_info) {
    bool fan_success = storeFanData(udp_info);
    bool sensor_success = storeSensorData(udp_info);
    
    if (fan_success && sensor_success) {
        LogManager::getLogger()->debug("✅ BMC数据存储成功: box_id={}", udp_info.boxid);
        return true;
    } else {
        LogManager::getLogger()->warn("⚠️ BMC数据部分存储失败: box_id={}, fan_ok={}, sensor_ok={}", 
                                    udp_info.boxid, fan_success, sensor_success);
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
            
            if (executeSql(create_sql.str())) {
                // 插入数据
                ostringstream insert_sql;
                insert_sql << "INSERT INTO " << table_name << " VALUES ("
                          << ts << ", "
                          << static_cast<int>(alarm_type) << ", "
                          << static_cast<int>(work_mode) << ", "
                          << speed << ")";
                
                executeSql(insert_sql.str());
            }
        }
        
        // 存储传感器数据
        auto boards = j["boards"];
        for (const auto& board : boards) {
            uint8_t slot_id = board["ipmb_address"];
            auto sensors = board["sensors"];
            
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
                          << static_cast<int>(sensor_type) << ")";
                
                if (executeSql(create_sql.str())) {
                    // 插入数据
                    ostringstream insert_sql;
                    insert_sql << "INSERT INTO " << table_name << " VALUES ("
                              << ts << ", "
                              << sensor_value << ", "
                              << static_cast<int>(alarm_type) << ")";
                    
                    executeSql(insert_sql.str());
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

bool BMCStorage::executeSql(const string& sql) {
    if (!taos_) {
        last_error_ = "数据库连接未建立";
        return false;
    }
    
    TAOS_RES* result = taos_query(taos_, sql.c_str());
    int code = taos_errno(result);
    
    if (code != 0) {
        last_error_ = "SQL执行失败: " + string(taos_errstr(result)) + " SQL: " + sql;
        LogManager::getLogger()->error("SQL执行失败: {} - SQL: {}", taos_errstr(result), sql);
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
    
    if (!taos_) {
        last_error_ = "数据库连接未建立";
        return results;
    }
    
    LogManager::getLogger()->debug("BMCStorage: 执行查询: {}", sql);
    
    TAOS_RES* res = taos_query(taos_, sql.c_str());
    int code = taos_errno(res);
    
    if (code != 0) {
        last_error_ = "SQL执行失败: " + string(taos_errstr(res)) + " SQL: " + sql;
        LogManager::getLogger()->error("BMCStorage: SQL执行失败: {} - SQL: {}", taos_errstr(res), sql);
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
    
    if (!taos_) {
        LogManager::getLogger()->error("BMCStorage: 数据库连接未建立");
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