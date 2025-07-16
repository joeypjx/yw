#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include <taos.h>
#include "json.hpp"

// 查询结果结构
struct QueryResult {
    std::map<std::string, std::string> labels;  // 标签
    double value;                                // 值
    std::chrono::system_clock::time_point timestamp;
    std::string metric;                          // 指标名称，如 'usage_percent'
};

class ResourceStorage {
public:
    ResourceStorage(const std::string& host, const std::string& user, const std::string& password);
    ~ResourceStorage();

    bool connect();
    void disconnect();
    bool createDatabase(const std::string& dbName);
    bool createResourceTable();
    bool insertResourceData(const std::string& hostIp, const nlohmann::json& resourceData);
    
    // 查询接口
    std::vector<QueryResult> executeQuerySQL(const std::string& sql);

private:
    std::string m_host;
    std::string m_user;
    std::string m_password;
    TAOS* m_taos;
    bool m_connected;

    bool executeQuery(const std::string& sql);
    std::string generateCreateTableSQL();
    std::string generateInsertSQL(const std::string& hostIp, const nlohmann::json& resourceData);
    
    bool insertCpuData(const std::string& hostIp, const nlohmann::json& cpuData);
    bool insertMemoryData(const std::string& hostIp, const nlohmann::json& memoryData);
    bool insertNetworkData(const std::string& hostIp, const nlohmann::json& networkData);
    bool insertDiskData(const std::string& hostIp, const nlohmann::json& diskData);
    bool insertGpuData(const std::string& hostIp, const nlohmann::json& gpuData);
    bool insertNodeData(const std::string& hostIp, const nlohmann::json& resourceData);
};