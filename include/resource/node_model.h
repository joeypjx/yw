#ifndef NODE_MODEL_H
#define NODE_MODEL_H

#include <string>
#include <vector>
#include <cstdint>
#include "json.hpp"

namespace node {

// GPU设备信息结构体
struct GpuInfo {
    int index;              // GPU设备序号
    std::string name;       // GPU设备名称
    
    GpuInfo() : index(0) {}
    GpuInfo(int idx, const std::string& gpu_name) : index(idx), name(gpu_name) {}
};

// JSON序列化支持
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GpuInfo, index, name)

// 节点信息结构体，对应node.json中的data结构
struct BoxInfo {
    int box_id;                     // 机箱号
    int slot_id;                    // 槽位号
    int cpu_id;                     // CPU号
    int srio_id;                    // SRIO号
    std::string host_ip;            // 主机IP，点分十进制
    std::string hostname;           // 主机名
    uint16_t service_port;          // 命令响应服务端口
    std::string box_type;           // 机箱类型
    std::string board_type;         // 板卡类型
    std::string cpu_type;           // CPU类型
    std::string os_type;            // 操作系统类型
    std::string resource_type;      // GPU计算模块类型
    std::string cpu_arch;           // CPU架构
    std::vector<GpuInfo> gpu;       // GPU设备列表
    
    BoxInfo() : box_id(0), slot_id(0), cpu_id(0), srio_id(0), service_port(0) {}
    
    // 构造函数，用于初始化所有字段
    BoxInfo(int box, int slot, int cpu, int srio, 
             const std::string& ip, const std::string& host,
             uint16_t port, const std::string& box_t, const std::string& board_t,
             const std::string& cpu_t, const std::string& os_t,
             const std::string& resource_t, const std::string& arch)
        : box_id(box), slot_id(slot), cpu_id(cpu), srio_id(srio),
          host_ip(ip), hostname(host), service_port(port), box_type(box_t),
          board_type(board_t), cpu_type(cpu_t), os_type(os_t),
          resource_type(resource_t), cpu_arch(arch) {}
};

// JSON序列化支持
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BoxInfo, box_id, slot_id, cpu_id, srio_id, host_ip, hostname, service_port, box_type, board_type, cpu_type, os_type, resource_type, cpu_arch, gpu)

// CPU信息结构体
struct CpuInfo {
    double usage_percent;       // CPU使用率百分比
    double load_avg_1m;         // CPU1分钟负载
    double load_avg_5m;         // CPU5分钟负载
    double load_avg_15m;        // CPU15分钟负载
    int core_count;             // CPU核数
    int core_allocated;         // CPU已分配核数
    double temperature;         // 温度，摄氏度
    double voltage;             // 电压，伏特
    double current;             // 电流，安培
    double power;               // 功耗，瓦特
    
    CpuInfo() : usage_percent(0.0), load_avg_1m(0.0), load_avg_5m(0.0), load_avg_15m(0.0),
                 core_count(0), core_allocated(0), temperature(0.0), voltage(0.0), current(0.0), power(0.0) {}
};

// 内存信息结构体
struct MemoryInfo {
    uint64_t total;             // 内存总大小，字节
    uint64_t used;              // 内存已使用大小，字节
    uint64_t free;              // 内存剩余大小，字节
    double usage_percent;       // 内存使用率，百分比
    
    MemoryInfo() : total(0), used(0), free(0), usage_percent(0.0) {}
};

// 网络接口信息结构体
struct NetworkInfo {
    std::string interface;      // 网卡名
    uint64_t rx_bytes;         // 接收字节
    uint64_t tx_bytes;         // 发送字节
    uint64_t rx_packets;       // 接收报文
    uint64_t tx_packets;       // 发送报文
    uint64_t rx_errors;        // 接收错误
    uint64_t tx_errors;        // 发送错误
    uint64_t rx_rate;          // 接收速率，字节/秒
    uint64_t tx_rate;          // 发送速率，字节/秒
    
    NetworkInfo() : rx_bytes(0), tx_bytes(0), rx_packets(0), tx_packets(0),
                    rx_errors(0), tx_errors(0), rx_rate(0), tx_rate(0) {}
};

// 磁盘信息结构体
struct DiskInfo {
    std::string device;         // 设备名称
    std::string mount_point;    // 挂载路径
    uint64_t total;             // 总大小，字节
    uint64_t used;              // 已使用大小，字节
    uint64_t free;              // 剩余大小，字节
    double usage_percent;       // 使用率，百分比
    
    DiskInfo() : total(0), used(0), free(0), usage_percent(0.0) {}
};

// GPU资源信息结构体（对应resource中的gpu字段）
struct GpuResourceInfo {
    int index;                  // GPU设备序号
    std::string name;           // GPU设备名称
    double compute_usage;       // GPU计算使用率
    double mem_usage;           // GPU内存使用率
    uint64_t mem_used;         // 显存已使用大小，字节
    uint64_t mem_total;        // 显存总大小，字节
    double temperature;         // 温度，摄氏度
    double power;               // 功耗，瓦特
    
    GpuResourceInfo() : index(0), compute_usage(0.0), mem_usage(0.0), 
                        mem_used(0), mem_total(0), temperature(0.0), power(0.0) {}
};

// 资源信息结构体
struct ResourceData {
    CpuInfo cpu;                // CPU信息
    MemoryInfo memory;          // 内存信息
    std::vector<NetworkInfo> network;  // 网络接口列表
    std::vector<DiskInfo> disk;        // 磁盘列表
    std::vector<GpuResourceInfo> gpu;  // GPU资源列表
    int gpu_allocated;          // 已分配GPU数量
    int gpu_num;                // GPU数量
    
    ResourceData() : gpu_allocated(0), gpu_num(0) {}
};

// 容器配置信息结构体
struct ContainerConfig {
    std::string name;           // 容器name
    std::string id;             // 容器ID (docker id)
    
    ContainerConfig() {}
};

// 容器资源使用情况结构体
struct ContainerResource {
    double cpu_load;            // CPU负载率，百分比
    uint64_t mem_used;         // 内存占用，字节
    uint64_t mem_limit;        // 内存限制，字节
    uint64_t network_tx;       // 发送字节，字节
    uint64_t network_rx;       // 接收字节，字节
    
    ContainerResource() : cpu_load(0.0), mem_used(0), mem_limit(0), network_tx(0), network_rx(0) {}
};

// 组件信息结构体
struct ComponentInfo {
    std::string instance_id;    // 容器所属的业务示例id
    std::string uuid;           // 容器所属的组件实例uuid
    int index;                  // 索引
    ContainerConfig config;     // 容器配置
    std::string state;          // 容器运行状态：PENDING/RUNNING/FAILED/STOPPED/SLEEPING
    ContainerResource resource; // 容器资源使用情况
    
    ComponentInfo() : index(0) {}
};

// 资源信息结构体，对应resource_info.json中的data字段
struct ResourceInfo {
    std::string host_ip;        // 主机IP，点分十进制
    ResourceData resource;       // 资源信息
    std::vector<ComponentInfo> component;  // 组件列表
    
    ResourceInfo() {}
};

// JSON序列化支持
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CpuInfo, usage_percent, load_avg_1m, load_avg_5m, load_avg_15m, core_count, core_allocated, temperature, voltage, current, power)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MemoryInfo, total, used, free, usage_percent)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NetworkInfo, interface, rx_bytes, tx_bytes, rx_packets, tx_packets, rx_errors, tx_errors, rx_rate, tx_rate)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DiskInfo, device, mount_point, total, used, free, usage_percent)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GpuResourceInfo, index, name, compute_usage, mem_usage, mem_used, mem_total, temperature, power)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ResourceData, cpu, memory, network, disk, gpu, gpu_allocated, gpu_num)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ContainerConfig, name, id)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ContainerResource, cpu_load, mem_used, mem_limit, network_tx, network_rx)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ComponentInfo, instance_id, uuid, index, config, state, resource)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ResourceInfo, host_ip, resource, component)

} // namespace node

#endif // NODE_MODEL_H
