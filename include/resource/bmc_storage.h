#ifndef BMC_STORAGE_H
#define BMC_STORAGE_H

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include "bmc_listener.h"
#include "json.hpp"

// BMC查询结果结构
struct BMCQueryResult {
    std::map<std::string, std::string> labels;   // 标签：box_id, slot_id, sensor_seq等
    std::map<std::string, double> metrics;       // 指标值：alarm_type, work_mode, speed等
    std::chrono::system_clock::time_point timestamp;
};

// BMC时间段数据结构
struct BMCRangeData {
    uint8_t box_id;
    std::string time_range;
    std::vector<std::string> metrics_types;  // "fan", "sensor"
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    
    // 时序数据，每个指标类型对应一个数据序列
    struct TimeSeriesData {
        std::string metric_type;  // "fan" or "sensor"
        std::vector<BMCQueryResult> data_points;
    };
    std::vector<TimeSeriesData> time_series;
    
    nlohmann::json to_json() const;
};

struct HistoricalBMCRequest {
    uint8_t box_id;
    std::string time_range;
    std::vector<std::string> metrics;
};

struct HistoricalBMCResponse {
    bool success = false;
    std::string error_message;
    BMCRangeData data;
};

/**
 * BMC数据存储类
 * 负责将BMC数据存储到TDengine数据库中
 */
class BMCStorage {
public:
    /**
     * 构造函数
     * @param host TDengine主机地址
     * @param user 用户名
     * @param password 密码
     * @param database 数据库名称
     */
    BMCStorage(const std::string& host, const std::string& user, 
               const std::string& password, const std::string& database);
    
    /**
     * 析构函数
     */
    ~BMCStorage();
    
    /**
     * 连接到TDengine数据库
     * @return 成功返回true，失败返回false
     */
    bool connect();
    
    /**
     * 断开数据库连接
     */
    void disconnect();
    
    /**
     * 创建BMC相关的超级表
     * @return 成功返回true，失败返回false
     */
    bool createBMCTables();
    
    /**
     * 存储风扇数据
     * @param udp_info UdpInfo结构体数据
     * @return 成功返回true，失败返回false
     */
    bool storeFanData(const UdpInfo& udp_info);
    
    /**
     * 存储传感器数据
     * @param udp_info UdpInfo结构体数据
     * @return 成功返回true，失败返回false
     */
    bool storeSensorData(const UdpInfo& udp_info);
    
    /**
     * 存储完整的BMC数据（包括风扇和传感器）
     * @param udp_info UdpInfo结构体数据
     * @return 成功返回true，失败返回false
     */
    bool storeBMCData(const UdpInfo& udp_info);
    
    /**
     * 从JSON字符串存储BMC数据
     * @param json_data JSON格式的BMC数据
     * @return 成功返回true，失败返回false
     */
    bool storeBMCDataFromJson(const std::string& json_data);
    
    /**
     * 获取指定box_id在某个时间段内的BMC数据
     * @param box_id BMC设备ID
     * @param time_range 时间范围，如"1h", "30m", "24h"
     * @param metrics 指标类型，如{"fan", "sensor"}
     * @return BMC时间段数据
     */
    BMCRangeData getBMCRangeData(uint8_t box_id, 
                                 const std::string& time_range,
                                 const std::vector<std::string>& metrics);
    
    /**
     * 获取最后的错误信息
     * @return 错误信息字符串
     */
    std::string getLastError() const;

private:
    /**
     * 创建风扇超级表
     * fan表的tags: box_id, fan_seq
     * 字段: ts(timestamp), alarm_type, work_mode, speed
     */
    bool createFanSuperTable();
    
    /**
     * 创建传感器超级表
     * sensor表的tags: box_id, slot_id, sensor_seq, sensor_name, sensor_type
     * 字段: ts(timestamp), sensor_value, alarm_type
     */
    bool createSensorSuperTable();
    
    /**
     * 删除旧的BMC超级表（用于结构更新）
     */
    void dropOldBMCTables();
    
    /**
     * 执行SQL语句
     * @param sql SQL语句
     * @return 成功返回true，失败返回false
     */
    bool executeSql(const std::string& sql);
    
    /**
     * 清理字符串中的特殊字符（用于表名）
     * @param str 输入字符串
     * @return 清理后的字符串
     */
    std::string cleanString(const std::string& str);
    
    /**
     * 执行查询并返回BMC格式的结果
     * @param sql SQL查询语句
     * @return BMC查询结果列表
     */
    std::vector<BMCQueryResult> executeBMCQuerySQL(const std::string& sql);
    
    /**
     * 解析时间范围字符串
     * @param time_range 时间范围字符串，如"1h", "30m"
     * @return 对应的秒数
     */
    std::chrono::seconds parseTimeRange(const std::string& time_range);

private:
    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;
    void* taos_;  // TAOS连接句柄
    std::string last_error_;
};

#endif // BMC_STORAGE_H