# API v1 文档

本文档描述了节点监控和告警管理系统HTTP服务器提供的REST API接口。

## 基础URL

所有API接口都相对于服务器基础URL。默认服务器运行在 `http://localhost:8080`。

## 响应格式

除非另有说明，所有API响应都使用JSON格式。错误响应遵循以下结构：

```json
{
  "error": "错误信息描述"
}
```

## CORS支持

所有接口都支持跨域资源共享(CORS)，包含以下响应头：
- `Access-Control-Allow-Origin: *`
- `Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS`
- `Access-Control-Allow-Headers: Content-Type`

## 分页功能

多个接口支持分页功能，使用以下请求参数和响应头：

### 请求参数
- `page` (可选, 整数): 页码，从1开始 (默认: 1)
- `page_size` (可选, 整数): 每页项目数量 (默认: 20, 最大: 1000)

### 响应头
- `X-Page`: 当前页码
- `X-Page-Size`: 当前页面大小
- `X-Total-Count`: 总项目数量
- `X-Total-Pages`: 总页数
- `X-Has-Next`: 是否有下一页，`true` 或 `false`
- `X-Has-Prev`: 是否有上一页，`true` 或 `false`

---

## 接口列表

### 1. Web管理界面

**GET** `/`

提供系统的Web管理界面。

**响应:** 包含交互式管理UI的HTML页面

**功能特性:**
- 节点指标表格，支持分页
- 告警规则管理，支持增删改查操作
- 告警事件监控，支持过滤
- 实时指标刷新 (10秒间隔)

---

### 2. 节点管理

#### 2.1 节点心跳

**POST** `/heartbeat`

接收节点心跳数据以维护节点状态和元数据。

**请求体:**
```json
{
  "api_version": 1,
  "data": {
    "box_id": 1,
    "slot_id": 1,
    "cpu_id": 1,
    "srio_id": 0,
    "host_ip": "192.168.10.29",
    "hostname": "localhost.localdomain",
    "service_port": 23980,
    "box_type": "计算I型",
    "board_type": "GPU",
    "cpu_type": "Phytium,D2000/8",
    "os_type": "Kylin Linux Advanced Server V10",
    "resource_type": "GPU I",
    "cpu_arch": "aarch64",
    "gpu": [
      {
        "index": 0,
        "name": " Iluvatar MR-V50A"
      }
    ]
  }
}
```

**响应:**
```json
{
  "status": "success"
}
```

**错误响应:**
- `400`: 缺少或无效的 `data` 字段
- `400`: 缺少或无效的 `host_ip` 字段
- `500`: 存储节点数据失败

#### 2.2 资源数据接收

**POST** `/resource`

接收节点的资源指标数据进行存储和监控。

**请求体:**
```json
{
  "data": {
    "host_ip": "192.168.1.100",
    "resource": {
      "cpu": {
        "usage_percent": 75.5,
        "load_avg_1m": 1.2,
        "load_avg_5m": 1.1,
        "load_avg_15m": 1.0,
        "core_count": 8,
        "core_allocated": 4,
        "temperature": 55.0,
        "power": 65.0,
        "voltage": 1.2,
        "current": 2.5
      },
      "memory": {
        "total": 17179869184,
        "used": 8589934592,
        "free": 8589934592,
        "usage_percent": 50.0
      },
      "disk": [
        {
          "device": "/dev/sda1",
          "mount_point": "/",
          "total": 1099511627776,
          "used": 549755813888,
          "free": 549755813888,
          "usage_percent": 50.0
        }
      ],
      "network": [
        {
          "interface": "eth0",
          "rx_bytes": 1024000,
          "tx_bytes": 512000,
          "rx_packets": 1000,
          "tx_packets": 500,
          "rx_errors": 0,
          "tx_errors": 0,
          "rx_rate": 1024.0,
          "tx_rate": 512.0
        }
      ],
      "gpu_allocated": 0,
      "gpu_num": 1,
      "gpu": [
        {
          "index": 0,
          "name": "NVIDIA RTX 4090",
          "compute_usage": 85.0,
          "mem_total": 25769803776,
          "mem_used": 12884901888,
          "mem_usage": 50.0,
          "temperature": 72.0,
          "power": 350.0
        }
      ]
    },
    "component": [
      {
        "instance_id": "874811e8-9e53-41ea-a543-275a4bff3864",
        "uuid": "1233211234569",
        "index": 1,
        "config": {
          "name": "192.168.10.58:5000/sleep3:latest",
          "id": "4c34877129713614d7a5eb89a00d117a72ffcc1e560bd7ea57962cb2e26b0000"
        },
        "state": "RUNNING",
        "resource": {
          "cpu": {
            "load": 0.0
          },
          "memory": {
            "mem_used": 524288,
            "mem_limit": 15647768576
          },
          "network": {
            "tx": 0,
            "rx": 0
          }
        }
      }
    ]
  }
}
```

**响应:**
```json
{
  "status": "success"
}
```

**错误响应:**
- `400`: 缺少或无效的 `data` 字段
- `400`: 缺少 `host_ip` 或 `resource` 字段
- `500`: 存储资源数据失败

#### 2.3 获取节点列表

**GET** `/node`

获取所有已注册节点的信息，或根据host_ip参数获取特定节点信息。

**查询参数:**
- `host_ip` (可选, 字符串): 特定节点的IP地址，如果提供则返回该节点的详细信息

**响应 (获取所有节点):**
```json
{
  "api_version": 1,
  "data": {
    "nodes": [
      {
        "board_type": "GPU",
        "box_id": 1,
        "box_type": "计算I型",
        "cpu_arch": "aarch64",
        "cpu_id": 1,
        "cpu_type": " Phytium,D2000/8",
        "created_at": 1750038100,
        "gpu": [
          {
            "index": 0,
            "name": " Iluvatar MR-V50A"
          }
        ],
        "host_ip": "192.168.10.58",
        "hostname": "localhost.localdomain",
        "id": 1,
        "os_type": "Kylin Linux Advanced Server V10",
        "resource_type": "GPU I",
        "service_port": 23980,
        "slot_id": 1,
        "srio_id": 5,
        "status": "online",
        "updated_at": 1750122059,
        "ipmb_address": 124,
        "module_type": 1,
        "bmc_company": 1,
        "bmc_version": "1.0.0"
      }
    ]
  },
  "status": "success"
}
```

**响应 (获取特定节点，使用host_ip参数):**
```json
{
  "api_version": 1,
  "data": {
    "board_type": "GPU",
    "box_id": 1,
    "box_type": "计算I型",
    "cpu_arch": "aarch64",
    "cpu_id": 1,
    "cpu_type": " Phytium,D2000/8",
    "created_at": 1750038100,
    "gpu": [
      {
        "index": 0,
        "name": " Iluvatar MR-V50A"
      }
    ],
    "host_ip": "192.168.10.58",
    "hostname": "localhost.localdomain",
    "id": 1,
    "os_type": "Kylin Linux Advanced Server V10",
    "resource_type": "GPU I",
    "service_port": 23980,
    "slot_id": 1,
    "srio_id": 5,
    "status": "online",
    "updated_at": 1750122059,
    "ipmb_address": 124,
    "module_type": 1,
    "bmc_company": 1,
    "bmc_version": "1.0.0"
  },
  "status": "success"
}
```

**字段详细说明:**

##### 基础节点信息
| 字段名 | 类型 | 说明 |
|-------|------|------|
| `host_ip` | String | 节点IP地址，唯一标识节点 |
| `hostname` | String | 节点主机名 |
| `box_id` | Integer | 机箱号，用于标识物理机箱 |
| `slot_id` | Integer | 槽位号，标识节点在机箱中的位置 |
| `cpu_id` | Integer | CPU号，标识CPU单元 |
| `srio_id` | Integer | SRIO号，用于高速互连 |
| `service_port` | Integer | 服务端口号 |
| `box_type` | String | 机箱类型（如：计算I型） |
| `board_type` | String | 板卡类型（如：GPU） |
| `cpu_type` | String | CPU型号和规格 |
| `cpu_arch` | String | CPU架构（如：aarch64, x86_64） |
| `os_type` | String | 操作系统类型和版本 |
| `resource_type` | String | 资源类型（如：GPU I） |
| `status` | String | 节点在线状态（online/offline） |
| `created_at` | Integer | 节点创建时间戳（Unix时间戳） |
| `updated_at` | Integer | 最后更新时间戳（Unix时间戳） |
| `id` | Integer | 节点ID，与box_id相同 |

##### BMC管理信息
| 字段名 | 类型 | 说明 |
|-------|------|------|
| `ipmb_address` | Integer | IPMB地址，用于BMC通信 |
| `module_type` | Integer | 模块类型，标识硬件模块类型 |
| `bmc_company` | Integer | BMC厂商代码 |
| `bmc_version` | String | BMC固件版本号 |

##### GPU硬件信息
| 字段名 | 类型 | 说明 |
|-------|------|------|
| `gpu` | Array | GPU设备列表 |
| `gpu[].index` | Integer | GPU设备索引号 |
| `gpu[].name` | String | GPU设备名称和型号 |

**状态说明:**
- `online`: 节点在线，最后心跳时间在5秒内
- `offline`: 节点离线，最后心跳时间超过5秒

**使用示例:**
```bash
# 获取所有节点列表
curl "http://localhost:8080/node"

# 获取特定节点信息
curl "http://localhost:8080/node?host_ip=192.168.10.58"
```

**错误响应:**
- `404`: 指定的节点未找到（使用host_ip参数时）
- `500`: ResourceManager不可用或服务器内部错误

---

### 3. 指标API

#### 3.1 当前节点指标

**GET** `/node/metrics`

获取所有节点的当前资源指标，包括CPU、内存、磁盘、网络、GPU和容器等各类指标的实时数据。

**查询参数:**
- `page` (可选, 整数): 分页页码，从1开始 (默认: 1)
- `page_size` (可选, 整数): 每页项目数 (默认: 20, 最大: 1000)

**特性:**
- 支持分页查询，提高大量节点场景下的查询性能
- 同时保持向后兼容，不提供分页参数时返回所有节点
- 分页信息通过HTTP响应头传递
- 10秒自动刷新机制（Web界面）

**响应 (分页模式):**
```json
{
  "api_version": 1,
  "data": {
    "nodes_metrics": [
      {
        "board_type": "GPU",
        "box_id": 1,
        "box_type": "计算I型",
        "cpu_arch": "aarch64",
        "cpu_id": 1,
        "cpu_type": "Phytium,D2000/8",
        "created_at": 1640995200,
        "gpu": [
          {
            "index": 0,
            "name": "Iluvatar MR-V50A"
          }
        ],
        "host_ip": "192.168.1.100",
        "hostname": "node-01",
        "id": 1,
        "ipmb_address": 124,
        "module_type": 1,
        "bmc_company": 1,
        "bmc_version": "1.0.0",
        "latest_cpu_metrics": {
          "core_allocated": 4,
          "core_count": 8,
          "current": 2.5,
          "load_avg_15m": 1.0,
          "load_avg_1m": 1.2,
          "load_avg_5m": 1.1,
          "power": 65.0,
          "temperature": 55.0,
          "timestamp": 1640995200,
          "usage_percent": 75.5,
          "voltage": 1.2
        },
        "latest_memory_metrics": {
          "free": 8589934592,
          "timestamp": 1640995200,
          "total": 17179869184,
          "usage_percent": 50.0,
          "used": 8589934592
        },
        "latest_disk_metrics": {
          "disk_count": 1,
          "disks": [
            {
              "device": "/dev/sda1",
              "free": 549755813888,
              "mount_point": "/",
              "total": 1099511627776,
              "usage_percent": 50.0,
              "used": 549755813888
            }
          ],
          "timestamp": 1640995200
        },
        "latest_network_metrics": {
          "network_count": 1,
          "networks": [
            {
              "interface": "eth0",
              "rx_bytes": 1024000,
              "rx_errors": 0,
              "rx_packets": 1000,
              "tx_bytes": 512000,
              "tx_errors": 0,
              "tx_packets": 500,
              "rx_rate": 1024.0,
              "tx_rate": 512.0
            }
          ],
          "timestamp": 1640995200
        },
        "latest_gpu_metrics": {
          "gpu_count": 1,
          "gpus": [
            {
              "compute_usage": 85.0,
              "current": 0.0,
              "index": 0,
              "mem_total": 25769803776,
              "mem_usage": 50.0,
              "mem_used": 12884901888,
              "name": "Iluvatar MR-V50A",
              "power": 350.0,
              "temperature": 72.0,
              "voltage": 0.0
            }
          ],
          "timestamp": 1640995200
        },
        "latest_docker_metrics": {
          "component": [
            {
              "component_index": 1,
              "container_id": "4c34877129713614d7a5eb89a00d117a72ffcc1e560bd7ea57962cb2e26b0000",
              "container_name": "192.168.10.58:5000/sleep3:latest",
              "instance_id": "874811e8-9e53-41ea-a543-275a4bff3864",
              "resource": {
                "cpu": {
                  "load": 0.0
                },
                "memory": {
                  "mem_limit": 15647768576,
                  "mem_used": 524288,
                  "usage_percent": 0.0033505608001139194
                },
                "network": {
                  "rx": 0,
                  "tx": 0
                }
              },
              "status": "RUNNING",
              "timestamp": 1752713939,
              "uuid": "1233211234569"
            }
          ],
          "container_count": 40,
          "paused_count": 0,
          "running_count": 0,
          "stopped_count": 0,
          "timestamp": 1752713939
        },
        "os_type": "Kylin Linux Advanced Server V10",
        "resource_type": "GPU I",
        "service_port": 23980,
        "slot_id": 1,
        "srio_id": 5,
        "status": "online",
        "updated_at": 1640995200
      }
    ]
  },
  "status": "success"
}
```

**响应 (兼容模式):**
```json
{
  "api_version": 1,
  "data": {
    "nodes_metrics": [
      // 与上面相同的节点对象
    ]
  },
  "status": "success"
}
```

**分页响应头** (使用分页时):
- `X-Page`: 当前页码
- `X-Page-Size`: 当前页面大小  
- `X-Total-Count`: 节点总数
- `X-Total-Pages`: 总页数
- `X-Has-Next`: 是否有更多页面
- `X-Has-Prev`: 是否有上一页

**字段详细说明:**

##### 节点基本信息
| 字段名 | 类型 | 说明 |
|-------|------|------|
| `host_ip` | String | 节点IP地址，唯一标识节点 |
| `hostname` | String | 节点主机名 |
| `box_id` | Integer | 机箱号，用于标识物理机箱 |
| `slot_id` | Integer | 槽位号，标识节点在机箱中的位置 |
| `cpu_id` | Integer | CPU号，标识CPU单元 |
| `srio_id` | Integer | SRIO号，用于高速互连 |
| `service_port` | Integer | 服务端口号 |
| `box_type` | String | 机箱类型（如：计算I型） |
| `board_type` | String | 板卡类型（如：GPU） |
| `cpu_type` | String | CPU型号和规格 |
| `cpu_arch` | String | CPU架构（如：aarch64, x86_64） |
| `os_type` | String | 操作系统类型和版本 |
| `resource_type` | String | 资源类型（如：GPU I） |
| `status` | String | 节点在线状态（online/offline） |
| `created_at` | Integer | 节点创建时间戳（Unix时间戳） |
| `updated_at` | Integer | 最后更新时间戳（Unix时间戳） |

##### CPU指标 (latest_cpu_metrics)
| 字段名 | 类型 | 单位 | 说明 |
|-------|------|------|------|
| `usage_percent` | Float | % | CPU使用率百分比 |
| `load_avg_1m` | Float | - | 1分钟平均负载 |
| `load_avg_5m` | Float | - | 5分钟平均负载 |
| `load_avg_15m` | Float | - | 15分钟平均负载 |
| `core_count` | Integer | 个 | CPU总核心数 |
| `core_allocated` | Integer | 个 | 已分配的CPU核心数 |
| `temperature` | Float | ℃ | CPU温度 |
| `voltage` | Float | V | CPU电压 |
| `current` | Float | A | CPU电流 |
| `power` | Float | W | CPU功耗 |
| `timestamp` | Integer | - | 数据采集时间戳 |

##### 内存指标 (latest_memory_metrics)
| 字段名 | 类型 | 单位 | 说明 |
|-------|------|------|------|
| `total` | Integer | Bytes | 总内存大小 |
| `used` | Integer | Bytes | 已使用内存大小 |
| `free` | Integer | Bytes | 空闲内存大小 |
| `usage_percent` | Float | % | 内存使用率百分比 |
| `timestamp` | Integer | - | 数据采集时间戳 |

##### 磁盘指标 (latest_disk_metrics)
| 字段名 | 类型 | 单位 | 说明 |
|-------|------|------|------|
| `disk_count` | Integer | 个 | 磁盘设备总数 |
| `disks` | Array | - | 磁盘设备列表 |
| `disks[].device` | String | - | 设备名称（如：/dev/sda1） |
| `disks[].mount_point` | String | - | 挂载点路径（如：/, /data） |
| `disks[].total` | Integer | Bytes | 磁盘总容量 |
| `disks[].used` | Integer | Bytes | 已使用容量 |
| `disks[].free` | Integer | Bytes | 剩余容量 |
| `disks[].usage_percent` | Float | % | 磁盘使用率百分比 |
| `timestamp` | Integer | - | 数据采集时间戳 |

##### 网络指标 (latest_network_metrics)
| 字段名 | 类型 | 单位 | 说明 |
|-------|------|------|------|
| `network_count` | Integer | 个 | 网络接口总数 |
| `networks` | Array | - | 网络接口列表 |
| `networks[].interface` | String | - | 网络接口名称（如：eth0, bond0） |
| `networks[].rx_bytes` | Integer | Bytes | 累计接收字节数 |
| `networks[].tx_bytes` | Integer | Bytes | 累计发送字节数 |
| `networks[].rx_packets` | Integer | 个 | 累计接收数据包数 |
| `networks[].tx_packets` | Integer | 个 | 累计发送数据包数 |
| `networks[].rx_errors` | Integer | 个 | 接收错误计数 |
| `networks[].tx_errors` | Integer | 个 | 发送错误计数 |
| `networks[].rx_rate` | Float | Bytes/s | 实时接收速率 |
| `networks[].tx_rate` | Float | Bytes/s | 实时发送速率 |
| `timestamp` | Integer | - | 数据采集时间戳 |

##### GPU指标 (latest_gpu_metrics)
| 字段名 | 类型 | 单位 | 说明 |
|-------|------|------|------|
| `gpu_count` | Integer | 个 | GPU设备总数 |
| `gpus` | Array | - | GPU设备列表 |
| `gpus[].index` | Integer | - | GPU设备索引号 |
| `gpus[].name` | String | - | GPU设备名称和型号 |
| `gpus[].compute_usage` | Float | % | GPU计算使用率百分比 |
| `gpus[].mem_total` | Integer | Bytes | GPU显存总容量 |
| `gpus[].mem_used` | Integer | Bytes | GPU显存已使用容量 |
| `gpus[].mem_usage` | Float | % | GPU显存使用率百分比 |
| `gpus[].temperature` | Float | ℃ | GPU温度 |
| `gpus[].power` | Float | W | GPU功耗 |
| `gpus[].voltage` | Float | V | GPU电压（某些指标可能为0） |
| `gpus[].current` | Float | A | GPU电流（某些指标可能为0） |
| `timestamp` | Integer | - | 数据采集时间戳 |

##### 容器指标 (latest_docker_metrics)
| 字段名 | 类型 | 单位 | 说明 |
|-------|------|------|------|
| `container_count` | Integer | 个 | 容器总数 |
| `running_count` | Integer | 个 | 运行中容器数 |
| `stopped_count` | Integer | 个 | 已停止容器数 |
| `paused_count` | Integer | 个 | 暂停状态容器数 |
| `component` | Array | - | 容器详细信息列表 |
| `component[].instance_id` | String | - | 业务实例ID |
| `component[].uuid` | String | - | 组件实例UUID |
| `component[].container_id` | String | - | Docker容器ID |
| `component[].container_name` | String | - | 容器镜像名称 |
| `component[].status` | String | - | 容器状态（RUNNING/STOPPED/FAILED等） |
| `component[].resource.cpu.load` | Float | % | 容器CPU负载率 |
| `component[].resource.memory.mem_used` | Integer | Bytes | 容器内存使用量 |
| `component[].resource.memory.mem_limit` | Integer | Bytes | 容器内存限制 |
| `component[].resource.memory.usage_percent` | Float | % | 容器内存使用率 |
| `component[].resource.network.rx` | Integer | Bytes | 容器网络接收字节数 |
| `component[].resource.network.tx` | Integer | Bytes | 容器网络发送字节数 |
| `timestamp` | Integer | - | 数据采集时间戳 |

**使用示例:**
```bash
# 获取第1页，每页20个节点的指标
curl "http://localhost:8080/node/metrics?page=1&page_size=20"

# 获取所有节点指标（兼容模式）
curl "http://localhost:8080/node/metrics"
```

#### 3.2 历史节点指标

**GET** `/node/historical-metrics`

获取特定节点和时间范围的历史指标数据。

**查询参数:**
- `host_ip` (可选): 特定节点IP地址
- `time_range` (可选): 时间范围 (默认: "1h", 示例: "1h", "24h", "7d")
- `metrics` (可选): 逗号分隔的指标类型列表 (默认: "cpu,memory,disk,network,gpu")

**响应:**
```json
{
  "api_version": 1,
  "status": "success",
  "data": {
    "historical_metrics": {
      "box_id": 0,
      "cpu_id": 1,
      "host_ip": "192.168.1.100",
      "slot_id": 0,
      "time_range": "1h",
      "metrics": {
        "cpu": [
          {
            "core_allocated": 0,
            "core_count": 8,
            "load_avg_15m": 1.7400000095367432,
            "load_avg_1m": 1.1100000143051147,
            "load_avg_5m": 1.4299999475479126,
            "power": 0.0,
            "temperature": 0.0,
            "timestamp": 1753426161,
            "usage_percent": 13.561190605163574
          }
        ],
        "memory": [
          {
            "free": 7097614336,
            "timestamp": 1753426161,
            "total": 15647768576,
            "usage_percent": 54.64136505126953,
            "used": 8550154240
          }
        ],
        "disk": {
          "_dev_mapper_klas_root": [
            {
              "device": "/dev/mapper/klas-root",
              "free": 1241542656,
              "mount_point": "/",
              "timestamp": 1753426161,
              "total": 107321753600,
              "usage_percent": 98.84315490722656,
              "used": 106080210944
            }
          ]
        },
        "network": {
          "enaphyt4i0": [
            {
              "interface": "enaphyt4i0",
              "rx_bytes": 4634635016,
              "rx_errors": 0,
              "rx_packets": 15660345,
              "timestamp": 1753426161,
              "tx_bytes": 4626000890,
              "tx_errors": 0,
              "tx_packets": 6736346
            }
          ]
        },
        "gpu": {
          "gpu_0": [
            {
              "compute_usage": 0.0,
              "index": 0,
              "mem_total": 17179869184,
              "mem_usage": 0.009999999776482582,
              "mem_used": 119537664,
              "name": " Iluvatar MR-V50A",
              "power": 19.0,
              "temperature": 44.0,
              "timestamp": 1753426161
            }
          ]
        },
        "container": {}
      }
    }
  }
}
```

**错误响应:**
- `400`: 参数无效或验证错误

---

### 4. 告警规则API

#### 4.1 创建告警规则

**POST** `/alarm/rules`

创建新的指标监控告警规则。支持对CPU、内存、磁盘、网络、GPU等各类资源指标设置告警条件，当指标值满足条件并持续指定时间后触发告警。

**请求体:**
```json
{
  "alert_name": "CPU使用率过高",
  "expression": {
    "stable": "cpu",
    "metric": "usage_percent",
    "conditions": [
      {
        "operator": ">",
        "threshold": 80.0
      }
    ]
  },
  "for": "5m",
  "severity": "严重",
  "summary": "CPU使用率过高",
  "description": "节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%",
  "alert_type": "硬件资源"
}
```

**响应:**
```json
{
  "id": "uuid-generated-rule-id",
  "status": "success"
}
```

**请求参数详细说明:**

| 字段名 | 类型 | 必需 | 说明 |
|-------|------|------|------|
| `alert_name` | String | ✅ | 告警规则的唯一名称，应具有描述性（如：HighCpuUsage） |
| `expression` | Object | ✅ | 告警触发条件表达式对象 |
| `expression.stable` | String | ✅ | 超级表名称，指定资源类型：`cpu`, `memory`, `disk`, `network`, `gpu`, `node` |
| `expression.metric` | String | ✅ | 要监控的具体指标名称（如：`usage_percent`, `load_avg_1m`） |
| `expression.conditions` | Array | ✅ | 条件数组，支持多个条件组合 |
| `expression.conditions[].operator` | String | ✅ | 比较操作符：`>`, `<`, `>=`, `<=`, `=`, `!=` |
| `expression.conditions[].threshold` | Number | ✅ | 用于比较的阈值（支持整数和浮点数） |
| `expression.tags` | Array | ❌ | 标签过滤条件，用于筛选特定设备或接口 |
| `for` | String | ✅ | 持续时间，条件满足多长时间后触发告警（如：`5m`, `30s`, `1h`, `0s`） |
| `severity` | String | ✅ | 告警严重等级：`提示`, `一般`, `严重` |
| `summary` | String | ✅ | 告警的简短摘要描述 |
| `description` | String | ✅ | 告警的详细描述，支持模板变量 |
| `alert_type` | String | ✅ | 告警分类：`硬件资源`, `业务链路`, `系统故障` |

**支持的资源类型和指标:**

##### CPU资源 (`stable: "cpu"`)
| 指标名 | 说明 | 单位 | 示例阈值 |
|-------|------|------|---------|
| `usage_percent` | CPU使用率 | % | 80.0 |
| `load_avg_1m` | 1分钟平均负载 | - | 2.0 |
| `load_avg_5m` | 5分钟平均负载 | - | 1.5 |
| `load_avg_15m` | 15分钟平均负载 | - | 1.0 |
| `core_count` | CPU核心数 | 个 | 8 |
| `core_allocated` | 已分配核心数 | 个 | 4 |
| `temperature` | 温度 | ℃ | 70.0 |
| `voltage` | 电压 | V | 1.2 |
| `current` | 电流 | A | 3.0 |
| `power` | 功率 | W | 100.0 |

##### 内存资源 (`stable: "memory"`)
| 指标名 | 说明 | 单位 | 示例阈值 |
|-------|------|------|---------|
| `usage_percent` | 内存使用率 | % | 85.0 |
| `total` | 总内存 | Bytes | 17179869184 |
| `used` | 已用内存 | Bytes | 8589934592 |
| `free` | 空闲内存 | Bytes | 1073741824 |

##### 磁盘资源 (`stable: "disk"`)
| 指标名 | 说明 | 单位 | 示例阈值 |
|-------|------|------|---------|
| `usage_percent` | 磁盘使用率 | % | 90.0 |
| `total` | 总空间 | Bytes | 1099511627776 |
| `used` | 已用空间 | Bytes | 549755813888 |
| `free` | 空闲空间 | Bytes | 107374182400 |

**磁盘标签过滤:**
- `device`: 设备名（如：`/dev/sda1`）
- `mount_point`: 挂载点（如：`/`, `/data`）

##### 网络资源 (`stable: "network"`)
| 指标名 | 说明 | 单位 | 示例阈值 |
|-------|------|------|---------|
| `rx_bytes` | 累计接收字节 | Bytes | 1073741824 |
| `tx_bytes` | 累计发送字节 | Bytes | 1073741824 |
| `rx_packets` | 累计接收包数 | 个 | 1000000 |
| `tx_packets` | 累计发送包数 | 个 | 1000000 |
| `rx_errors` | 接收错误数 | 个 | 100 |
| `tx_errors` | 发送错误数 | 个 | 100 |
| `rx_rate` | 接收速率 | Bytes/s | 104857600 |
| `tx_rate` | 发送速率 | Bytes/s | 104857600 |

**网络标签过滤:**
- `interface`: 网络接口名（如：`eth0`, `bond0`）

##### GPU资源 (`stable: "gpu"`)
| 指标名 | 说明 | 单位 | 示例阈值 |
|-------|------|------|---------|
| `compute_usage` | GPU计算使用率 | % | 80.0 |
| `mem_usage` | GPU显存使用率 | % | 85.0 |
| `mem_used` | 已用显存 | Bytes | 8589934592 |
| `mem_total` | 总显存 | Bytes | 17179869184 |
| `temperature` | GPU温度 | ℃ | 80.0 |
| `power` | GPU功耗 | W | 300.0 |

**GPU标签过滤:**
- `gpu_index`: GPU索引（如：`0`, `1`）
- `gpu_name`: GPU名称

##### 节点资源 (`stable: "node"`)
| 指标名 | 说明 | 单位 | 示例阈值 |
|-------|------|------|---------|
| `gpu_allocated` | 已分配GPU数量 | 个 | 2 |
| `gpu_num` | 节点GPU总数 | 个 | 4 |

**模板变量支持:**
在 `description` 字段中可使用以下模板变量：
- `{{host_ip}}`: 节点IP地址
- `{{value}}`: 触发告警的指标值
- `{{usage_percent}}`: 使用率百分比
- `{{mount_point}}`: 磁盘挂载点
- `{{interface}}`: 网络接口名
- `{{gpu_index}}`: GPU索引
- 其他指标字段名

**高级用法示例:**

1. **多条件告警**（区间判断）:
```json
{
  "alert_name": "DiskSpaceWarning",
  "expression": {
    "stable": "disk",
    "metric": "usage_percent",
    "tags": [{"mount_point": "/data"}],
    "conditions": [
      {"operator": ">", "threshold": 70.0},
      {"operator": "<", "threshold": 90.0}
    ]
  },
  "for": "2m",
  "severity": "一般",
  "summary": "磁盘空间预警",
  "description": "节点 {{host_ip}} 磁盘 {{mount_point}} 使用率为 {{value}}%，需要关注",
  "alert_type": "硬件资源"
}
```

2. **GPU告警**:
```json
{
  "alert_name": "HighGpuUsage", 
  "expression": {
    "stable": "gpu",
    "metric": "compute_usage",
    "conditions": [{"operator": ">", "threshold": 85.0}]
  },
  "for": "3m",
  "severity": "严重",
  "summary": "GPU使用率过高",
  "description": "节点 {{host_ip}} GPU {{gpu_index}} 计算使用率达到 {{value}}%",
  "alert_type": "硬件资源"
}
```

**错误响应:**
- `400`: 缺少或无效的必需字段
- `500`: 创建规则失败

#### 4.2 获取告警规则列表

**GET** `/alarm/rules`

获取告警规则列表，支持可选的分页和过滤。返回所有已配置的告警规则，包括启用和禁用的规则。

**查询参数:**
- `page` (可选, 整数): 分页页码，从1开始 (默认: 1)
- `page_size` (可选, 整数): 每页项目数 (默认: 20, 最大: 1000)
- `enabled_only` (可选, 字符串): 仅筛选启用的规则 ("true"/"false")

**响应字段说明:**
| 字段名 | 类型 | 说明 |
|-------|------|------|
| `id` | String | 告警规则的唯一标识符（UUID格式） |
| `alert_name` | String | 告警规则名称 |
| `expression_json` | String | 告警表达式的JSON字符串格式 |
| `for_duration` | String | 持续时间设置 |
| `severity` | String | 严重等级（提示/一般/严重） |
| `summary` | String | 告警摘要 |
| `description` | String | 告警详细描述，包含模板变量 |
| `alert_type` | String | 告警类型分类 |
| `enabled` | Boolean | 规则是否启用 |
| `created_at` | String | 创建时间（MySQL datetime格式） |
| `updated_at` | String | 最后更新时间（MySQL datetime格式） |

**响应 (分页模式):**
```json
{
  "api_version": 1,
  "status": "success",
  "data": [
    {
      "id": "uuid-rule-id",
      "alert_name": "CPU使用率过高",
      "expression_json": "{\"stable\":\"cpu\",\"metric\":\"usage_percent\",\"conditions\":[{\"operator\":\">\",\"threshold\":80.0}]}",
    "for_duration": "5m",
    "severity": "严重",
    "summary": "CPU使用率过高",
    "description": "节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%",
    "alert_type": "硬件资源",
    "enabled": true,
    "created_at": "2024-01-01 00:00:00",
    "updated_at": "2024-01-01 00:00:00"
    }
  ]
}
```

**分页响应头** (使用分页时):
- `X-Page`, `X-Page-Size`, `X-Total-Count`, `X-Total-Pages`, `X-Has-Next`, `X-Has-Prev`

#### 4.3 获取告警规则

**GET** `/alarm/rules/{id}`

获取特定告警规则的详情。

**路径参数:**
- `id`: 告警规则UUID

**响应:**
```json
{
  "api_version": 1,
  "status": "success",
  "data": {
    "id": "uuid-rule-id",
    "alert_name": "CPU使用率过高",
    "expression_json": "{\"stable\":\"cpu\",\"metric\":\"usage_percent\",\"conditions\":[{\"operator\":\">\",\"threshold\":80.0}]}",
  "for_duration": "5m",
  "severity": "严重",
  "summary": "CPU使用率过高",
  "description": "节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%",
  "alert_type": "硬件资源",
  "enabled": true,
  "created_at": "2024-01-01 00:00:00",
  "updated_at": "2024-01-01 00:00:00"
  }
}
```

**错误响应:**
- `404`: 规则未找到
- `500`: 服务器错误

#### 4.4 更新告警规则

**POST** `/alarm/rules/{id}/update`

更新现有的告警规则。

**路径参数:**
- `id`: 告警规则UUID

**请求体:** 与创建规则相同

**响应:**
```json
{
  "status": "success"
}
```

**错误响应:**
- `400`: 参数无效
- `404`: 规则未找到
- `500`: 更新失败

#### 4.5 删除告警规则

**POST** `/alarm/rules/{id}/delete`

删除告警规则。

**路径参数:**
- `id`: 告警规则UUID

**响应:**
```json
{
  "status": "success"
}
```

**错误响应:**
- `404`: 规则未找到
- `500`: 删除失败

---

### 5. 机箱控制API

机箱控制API提供对机箱板卡的电源管理功能，支持对指定槽位的板卡进行复位、下电和上电操作。这些操作通过TCP协议与机箱控制器通信，遵循特定的通信协议规范。

#### 5.1 复位机箱板卡

**POST** `/chassis/reset`

对指定槽位的机箱板卡执行复位操作。支持单个槽位或多个槽位的批量操作。

**请求体:**

单槽位操作:
```json
{
  "target_ip": "192.168.1.180",
  "slot_number": 3,
  "request_id": 1001
}
```

多槽位操作:
```json
{
  "target_ip": "192.168.1.180", 
  "slot_numbers": [1, 2, 4],
  "request_id": 1002
}
```

**请求参数详细说明:**
| 字段名 | 类型 | 必需 | 说明 |
|-------|------|------|------|
| `target_ip` | String | ✅ | 机箱控制器的IP地址 |
| `slot_number` | Integer | ❌ | 单个槽位号 (1-12)，与slot_numbers二选一 |
| `slot_numbers` | Array[Integer] | ❌ | 多个槽位号数组，与slot_number二选一 |
| `request_id` | Integer | ❌ | 请求序列号，用于请求跟踪 (默认: 0) |

**响应:**
```json
{
  "status": "success",
  "message": "Reset operation completed successfully",
  "operation_result": "SUCCESS",
  "slot_results": [
    {
      "slot_number": 3,
      "status": "SUCCESS",
      "message": "Slot 3 reset successful"
    }
  ],
  "raw_response": "4554485357420000..."
}
```

**响应字段说明:**
| 字段名 | 类型 | 说明 |
|-------|------|------|
| `status` | String | HTTP操作状态 ("success"/"error") |
| `message` | String | 操作结果描述信息 |
| `operation_result` | String | 操作结果类型 (SUCCESS/PARTIAL_SUCCESS/NETWORK_ERROR/TIMEOUT_ERROR/INVALID_RESPONSE) |
| `slot_results` | Array | 各槽位的操作结果详情 |
| `slot_results[].slot_number` | Integer | 槽位号 |
| `slot_results[].status` | String | 槽位操作状态 (SUCCESS/FAILED/REQUEST_OPERATION/NO_OPERATION) |
| `slot_results[].message` | String | 槽位操作结果描述 |
| `raw_response` | String | 原始响应数据的十六进制表示，用于调试 |

**错误响应:**
- `400`: 请求参数无效 (缺少target_ip、槽位号无效等)
- `500`: 机箱控制器通信失败或内部错误

#### 5.2 下电机箱板卡

**POST** `/chassis/power-off`

对指定槽位的机箱板卡执行下电操作。支持单个槽位或多个槽位的批量操作。

**请求体:** 与复位操作相同

**响应:** 与复位操作相同，但operation字段为"power-off"

#### 5.3 上电机箱板卡

**POST** `/chassis/power-on`

对指定槽位的机箱板卡执行上电操作。支持单个槽位或多个槽位的批量操作。

**请求体:** 与复位操作相同

**响应:** 与复位操作相同，但operation字段为"power-on"

#### 机箱控制协议说明

**槽位编号规则:**
- 支持的槽位号范围: 1-12
- 槽位号与内部数组索引的映射: 槽位1对应索引0，槽位12对应索引11

**操作状态说明:**
- `NO_OPERATION` (0): 该槽位无操作
- `REQUEST_OPERATION` (1): 请求操作该槽位
- `SUCCESS` (2): 操作成功
- `FAILED` (-1): 操作失败

**通信协议:**
- 通信端口: 33000 (TCP)
- 协议标识: "ETHSWB"
- 支持的命令: "RESET", "PWOFF", "PWON"
- 数据格式: 二进制结构体，包含标识符、目标IP、命令、槽位数组和请求ID

**使用示例:**
```bash
# 复位单个槽位
curl -X POST "http://localhost:8080/chassis/reset" \
  -H "Content-Type: application/json" \
  -d '{
    "target_ip": "192.168.1.180",
    "slot_number": 3,
    "request_id": 1001
  }'

# 下电多个槽位
curl -X POST "http://localhost:8080/chassis/power-off" \
  -H "Content-Type: application/json" \
  -d '{
    "target_ip": "192.168.1.180",
    "slot_numbers": [1, 2, 4],
    "request_id": 1002
  }'

# 上电指定槽位
curl -X POST "http://localhost:8080/chassis/power-on" \
  -H "Content-Type: application/json" \
  -d '{
    "target_ip": "192.168.1.180",
    "slot_number": 1,
    "request_id": 1003
  }'
```

---

### 6. 告警事件API

#### 6.1 获取告警事件列表

**GET** `/alarm/events`

获取告警事件列表，支持可选的过滤和分页。返回已触发的告警事件，包括正在触发的和已解决的告警。

**查询参数:**
- `status` (可选, 字符串): 按事件状态过滤
  - `active` 或 `firing`: 只返回当前处于"触发中"状态的告警
  - 不提供此参数时返回所有最近的事件（包括已解决的）
- `page` (可选, 整数): 分页页码，从1开始 (默认: 1)
- `page_size` (可选, 整数): 每页项目数 (默认: 20, 最大: 1000)
- `limit` (可选, 整数): 兼容模式限制结果数量参数 (默认: 100)

**响应字段说明:**
| 字段名 | 类型 | 说明 |
|-------|------|------|
| `id` | String | 告警事件的唯一标识符 |
| `fingerprint` | String | 告警指纹，用于标识同一类告警 |
| `status` | String | 告警状态：`firing`（触发中）、`resolved`（已解决） |
| `labels` | Object | 告警标签信息，包含告警来源和分类信息 |
| `labels.alertname` | String | 触发此事件的告警规则名称 |
| `labels.instance` | String | 触发告警的节点实例（通常是IP地址） |
| `labels.severity` | String | 告警严重等级 |
| `annotations` | Object | 告警注释信息，包含描述性内容 |
| `annotations.summary` | String | 告警摘要信息 |
| `annotations.description` | String | 告警详细描述，已替换模板变量 |
| `starts_at` | String | 告警开始时间（ISO 8601格式） |
| `ends_at` | String\|null | 告警结束时间，null表示仍在触发中 |
| `generator_url` | String | 生成此告警的URL地址 |
| `created_at` | String | 事件创建时间（MySQL datetime格式） |
| `updated_at` | String | 事件最后更新时间（MySQL datetime格式） |

**状态说明:**
- `firing`: 告警正在触发，条件仍然满足
- `resolved`: 告警已解决，条件不再满足

**使用示例:**
```bash
# 获取最近50条告警事件
curl "http://localhost:8080/alarm/events?limit=50"

# 获取所有活跃的告警
curl "http://localhost:8080/alarm/events?status=active"

# 分页获取告警事件
curl "http://localhost:8080/alarm/events?page=1&page_size=20"
```

**响应 (分页模式):**
```json
{
  "api_version": 1,
  "status": "success",
  "data": [
    {
      "id": "uuid-event-id",
      "fingerprint": "alert-fingerprint-hash",
      "status": "firing",
    "labels": {
      "alertname": "CPU使用率过高",
      "instance": "192.168.1.100",
      "severity": "warning"
    },
    "annotations": {
      "summary": "CPU使用率过高",
      "description": "CPU使用率超过80%持续2分钟以上"
    },
    "starts_at": "2024-01-01T00:00:00Z",
    "ends_at": null,
    "generator_url": "http://localhost:8080/alerts",
    "created_at": "2024-01-01 00:00:00",
    "updated_at": "2024-01-01 00:00:00"
    }
  ]
}
```

**分页响应头** (使用分页时):
- `X-Page`, `X-Page-Size`, `X-Total-Count`, `X-Total-Pages`, `X-Has-Next`, `X-Has-Prev`

**兼容响应** (无分页):
相同的数组格式，但没有响应头。

#### 6.2 获取告警事件数量

**GET** `/alarm/events/count`

获取告警事件数量，支持按状态过滤。可以获取所有告警事件的总数量，或仅获取当前活跃告警的数量。

**查询参数:**
- `status` (可选, 字符串): 告警状态过滤条件
  - `active` 或 `firing`: 只返回当前处于"触发中"状态的告警数量
  - 不提供此参数或其他值: 返回所有告警事件的总数量（包括已触发和已解决的）

**响应:**
```json
{
  "api_version": 1,
  "status": "success", 
  "data": {
    "count": 156
  }
}
```

**响应字段说明:**
| 字段名 | 类型 | 说明 |
|-------|------|------|
| `api_version` | Integer | API版本号，固定为1 |
| `status` | String | 响应状态，固定为"success" |
| `data.count` | Integer | 告警事件数量，根据status参数返回对应的统计结果 |

**使用示例:**
```bash
# 获取所有告警事件总数量（包括已解决的）
curl "http://localhost:8080/alarm/events/count"

# 获取当前活跃告警数量（仅firing状态）
curl "http://localhost:8080/alarm/events/count?status=active"

# 获取当前触发中的告警数量（仅firing状态）
curl "http://localhost:8080/alarm/events/count?status=firing"
```

**错误响应:**
- `500`: AlarmManager不可用或服务器内部错误

---

### 7. WebSocket实时通知API

WebSocket API提供实时通知功能，允许客户端通过WebSocket连接接收实时的告警事件和系统状态更新。WebSocket服务器运行在独立的端口上，提供低延迟的实时通信。

#### 7.1 WebSocket连接

**WebSocket URL:** `ws://localhost:9002`

**连接方式:**
```javascript
const ws = new WebSocket('ws://localhost:9002');
```

**连接状态:**
- `CONNECTING` (0): 连接建立中
- `OPEN` (1): 连接已建立，可以通信
- `CLOSING` (2): 连接正在关闭
- `CLOSED` (3): 连接已关闭


#### 7.2 告警事件通知

当告警事件触发或状态发生变化时，服务器会向所有连接的客户端广播告警事件：

**告警事件消息格式:**
```json
{
  "type": "alarm_event",
  "fingerprint": "alert-fingerprint-hash",
  "status": "firing",
  "starts_at": "2024-01-01T00:00:00Z",
  "ends_at": null,
  "labels": {
    "alertname": "CPU使用率过高",
    "instance": "192.168.1.100",
    "severity": "warning"
  },
  "annotations": {
    "summary": "CPU使用率过高",
    "description": "节点 192.168.1.100 CPU使用率达到 85.5%"
  }
}
```

**告警事件字段说明:**
| 字段名 | 类型 | 说明 |
|-------|------|------|
| `type` | String | 消息类型，固定为 "alarm_event" |
| `fingerprint` | String | 告警指纹，用于标识同一类告警 |
| `status` | String | 告警状态：`firing`（触发中）、`resolved`（已解决） |
| `starts_at` | String | 告警开始时间（ISO 8601格式） |
| `ends_at` | String\|null | 告警结束时间，null表示仍在触发中 |
| `labels` | Object | 告警标签信息 |
| `annotations` | Object | 告警注释信息 |

#### 7.3 心跳与连接健康

WebSocket服务器会自动定期向所有客户端发送 ping，客户端收到后自动回复 pong（大多数浏览器和WebSocket库会自动处理）。

- 服务器每30秒自动发ping
- 客户端无需主动发心跳
- 如果客户端长时间未响应pong，服务器会自动断开连接

**客户端示例：**
```javascript
const ws = new WebSocket('ws://localhost:9002');
ws.onopen = () => {
  // 连接建立即可，无需手动心跳
};
ws.onclose = () => {
  // 可实现自动重连
};
```

---

## 数据类型和单位

### 时间戳
- 数字字段使用Unix时间戳 (从1970年1月1日开始的秒数)
- 字符串时间戳使用ISO 8601格式
- 某些字段使用毫秒 (会明确标注)

### 内存和存储
- 除非另有说明，所有大小都以字节为单位
- UI中显示GB/MB转换 (1 GB = 1,073,741,824 字节)

### 网络
- 数据传输量以字节为单位
- 传输速率以字节/秒为单位
- 数据包计数为整数

### CPU
- 使用率百分比为浮点数 (0.0-100.0)
- 负载平均值为浮点数
- 温度以摄氏度为单位
- 功率以瓦特为单位
- 电压以伏特为单位
- 电流以安培为单位

### GPU
- 内存大小以字节为单位
- 使用率百分比为浮点数 (0.0-100.0)
- 温度以摄氏度为单位
- 功率以瓦特为单位

---

## 错误处理

### 常见HTTP状态码
- `200`: 成功
- `400`: 请求错误 (参数无效、缺少必需字段)
- `404`: 未找到 (资源不存在)
- `500`: 内部服务器错误 (服务器端问题)

### 错误响应格式
```json
{
  "error": "描述性错误信息"
}
```

所有接口都包含完善的错误处理，使用适当的HTTP状态码和描述性错误信息以便调试和客户端错误处理。

---

## 告警规则字段详细说明

### Expression表达式结构
| 字段                                | 类型     | 必需  | 说明                                      |
|-----------------------------------|--------|-----|-----------------------------------------|
| stable                            | String | ✅   | 超级表名称 (cpu, memory, disk, network, gpu) |
| metric                            | String | ✅   | 指标名称 (usage_percent, compute_usage等)    |
| conditions                        | Array  | ✅   | 条件数组                                    |
| conditions[].operator             | String | ✅   | 操作符 (>, <, >=, <=, =, !=)               |
| conditions[].threshold            | Number | ✅   | 阈值                                      |
| tags                              | Array  | ❌   | 标签过滤条件                                  |

### 严重等级选项
- `提示`: 信息性告警
- `一般`: 普通警告
- `严重`: 严重问题

### 告警类型选项
- `硬件资源`: 硬件资源类告警
- `业务链路`: 业务链路类告警  
- `系统故障`: 系统故障类告警

### 持续时间格式
支持的时间单位：
- `s`: 秒 (如: "30s")
- `m`: 分钟 (如: "5m") 
- `h`: 小时 (如: "1h")
- `0s`: 立即触发

### 模板变量
告警描述中支持使用以下模板变量：
- `{{host_ip}}`: 节点IP地址
- `{{usage_percent}}`: 使用率百分比
- `{{mount_point}}`: 磁盘挂载点
- 其他指标字段名