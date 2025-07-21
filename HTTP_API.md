# 软件HTTP接口说明

本文档详细说明了本软件提供的HTTP API接口，包括资源数据上报、告警规则管理和告警事件查询。

## 1. Web管理界面

提供了一个基于Web的图形化管理界面，方便用户查看和管理告警规则及事件。

- **URL**: `/
- **Method**: `GET`
- **说明**: 访问此URL将返回一个HTML页面，您可以在浏览器中直接进行操作。

---

## 2. 资源数据接口

用于客户端上报资源监控数据。

### 2.1 上报资源数据

- **URL**: `/resource`
- **Method**: `POST`
- **说明**: 接收并存储客户端上报的资源数据。
- **请求体 (Content-Type: application/json)**:

  ```json
  {
    "data": {
      "host_ip": "192.168.10.58",
      "resource": {
        "cpu": {
          "usage_percent": 26.13
        },
        "disk": [
          {
            "mount_point": "/",
            "usage_percent": 96.26
          },
          {
            "mount_point": "/data",
            "usage_percent": 90.47
          }
        ],
        "memory": {
          "usage_percent": 28.25
        }
      }
    }
  }
  ```
- **请求参数说明**:
  - `data` (Object, required): 包含所有上报数据的根对象。
    - `host_ip` (String, required): 上报数据的主机IP地址，用于唯一标识数据来源。
    - `resource` (Object, required): 包含具体资源指标的对象。该对象下可以有 `cpu`, `memory` 等子对象，以及 `disk`, `gpu`, `network` 等对象数组。告警规则的 `expression` 会根据这个结构来查询数据。

- **成功响应 (200 OK)**:

  ```json
  {
    "status": "success"
  }
  ```

- **失败响应 (4xx/5xx)**:

  ```json
  {
    "error": "具体的错误信息"
  }
  ```
- **示例 (curl)**:
  ```bash
  curl -X POST -H "Content-Type: application/json" \
  -d '{"data":{"host_ip":"192.168.10.58","resource":{"cpu":{"usage_percent":26.1}}}}' \
  http://localhost:8080/resource
  ```

---

## 3. 告警规则接口 (Alarm Rules API)

用于对告警规则进行增、删、改、查操作。

### 3.1 创建告警规则

- **URL**: `/alarm/rules`
- **Method**: `POST`
- **说明**: 创建一条新的告警规则。
- **请求体 (Content-Type: application/json)**:

  ```json
  {
    "alert_name": "HighCpuUsage",
    "expression": {
      "stable": "cpu",
      "metric": "usage_percent",
      "conditions": [
        {
          "operator": ">",
          "threshold": 85.0
        }
      ]
    },
    "for": "5m",
    "severity": "严重",
    "summary": "CPU使用率过高",
    "description": "节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%。",
    "alert_type": "硬件资源"
  }
  ```
  ```json
  {
    "alert_name": "HighDiskUsage",
    "expression": {
      "stable": "disk",
      "metric": "usage_percent",
      "tags": [
        {"mount_point": "/data"}
      ],
      "conditions": [
        {
          "operator": ">",
          "threshold": 85.0
        },
        {
          "operator": "<",
          "threshold": 95.0
        }
      ]
    },
    "for": "0s",
    "severity": "严重",
    "summary": "DISK使用率过高",
    "description": "节点 {{host_ip}} DISK使用率达到 {{value}}}%。",
    "alert_type": "硬件资源"
  }
  ```
- **请求参数说明**:
  - `alert_name` (String, required): 告警规则的名称，应具有描述性。
  - `expression` (Object, required): 告警触发条件表达式。
    - `stable` (String, required): "超级表"名称，指定要查询的资源类型，如 `cpu`, `memory`, `disk`。
    - `metric` (String, required): 要评估的指标名称，如 `usage_percent`。
    - `conditions` (Array, required): 一个或多个条件对象的数组。每个对象包含 `operator` 和 `threshold`。
      - `operator` (String, required): 比较操作符, 如 `>` (大于), `<` (小于), `=` (等于)。
      - `threshold` (Number, required): 用于比较的阈值。
    - `tags` (Array, optional): 标签过滤条件数组。用于在 `disk` 等数组类型的资源中筛选特定项。例如 `[{"mount_point": "/data"}]`。
  - `for` (String, required): 告警持续时间。表示`expression`条件需要持续满足多长时间才会触发告警。格式为数字加单位，如 `1m` (1分钟), `5s` (5秒)。
  - `severity` (String, required): 告警严重等级，如 `提示`, `一般`, `严重`。
  - `summary` (String, required): 告警的简短摘要。
  - `description` (String, required): 告警的详细描述。支持使用 `{{label_name}}` 格式的模板变量，例如 `{{host_ip}}` 会在触发时被替换为实际的主机IP, `{{value}}`替换为指标值。
  - `alert_type` (String, required): 告警类型, 如 `硬件资源`, `业务链路`。

- **成功响应 (200 OK)**:

  ```json
  {
    "status": "success",
    "id": "rule_-Nq_g..."
  }
  ```
- **示例 (curl)**:
  ```bash
  curl -X POST -H "Content-Type: application/json" \
  -d '{"alert_name":"HighCpuUsage","expression":{"stable":"cpu","metric":"usage_percent","conditions":[{"operator":">","threshold":85.0}]},"for":"5m","severity":"严重","summary":"CPU使用率过高","description":"节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%。","alert_type":"硬件资源"}' \
  http://localhost:8080/alarm/rules
  ```

### 3.2 获取所有告警规则

- **URL**: `/alarm/rules`
- **Method**: `GET`
- **说明**: 返回所有已配置的告警规则列表。此接口没有请求参数。
- **成功响应 (200 OK)**: 返回一个包含所有规则对象的JSON数组。

  ```json
  [
    {
      "id": "rule_...",
      "alert_name": "HighCpuUsage",
      "expression": {
        "stable": "cpu",
        "metric": "usage_percent",
        "conditions": [
          {
            "operator": ">",
            "threshold": 85.0
          }
        ]
      },
      "for": "5m",
      "severity": "严重",
      "summary": "CPU使用率过高",
      "description": "...",
      "alert_type": "硬件资源",
      "enabled": true,
      "created_at": "...",
      "updated_at": "..."
    }
  ]
  ```
- **示例 (curl)**:
  ```bash
  curl http://localhost:8080/alarm/rules
  ```

### 3.3 获取单个告警规则

- **URL**: `/alarm/rules/{rule_id}`
- **Method**: `GET`
- **说明**: 根据规则ID获取其详细信息。
- **路径参数说明**:
  - `rule_id` (String, required): 要查询的告警规则的唯一ID。
- **成功响应 (200 OK)**: 返回单个规则对象（结构同上）。
- **失败响应 (404 Not Found)**:
  ```json
  {
    "error": "Alarm rule not found"
  }
  ```
- **示例 (curl)**:
  ```bash
  curl http://localhost:8080/alarm/rules/rule_-Nq_g...
  ```

### 3.4 更新告警规则

- **URL**: `/alarm/rules/{rule_id}/update`
- **Method**: `POST`
- **说明**: 根据规则ID更新其内容。
- **路径参数说明**:
  - `rule_id` (String, required): 要更新的告警规则的唯一ID。
- **请求体 (Content-Type: application/json)**: 结构和参数说明同“创建告警规则”接口。所有字段都会被更新。
- **成功响应 (200 OK)**:
  ```json
  {
    "status": "success",
    "id": "rule_..."
  }
  ```
- **示例 (curl)**:
  ```bash
  curl -X POST -H "Content-Type: application/json" \
  -d '{"alert_name":"Updated CPU Alert", "expression": {"stable":"cpu",...}}' \
  http://localhost:8080/alarm/rules/rule_-Nq_g.../update
  ```

### 3.5 删除告警规则

- **URL**: `/alarm/rules/{rule_id}/delete`
- **Method**: `POST`
- **说明**: 根据规则ID删除指定的告警规则。
- **路径参数说明**:
  - `rule_id` (String, required): 要删除的告警规则的唯一ID。
- **成功响应 (200 OK)**:
  ```json
  {
    "status": "success",
    "id": "rule_..."
  }
  ```
- **示例 (curl)**:
  ```bash
  curl -X POST http://localhost:8080/alarm/rules/rule_-Nq_g.../delete
  ```

---

## 4. 告警事件接口 (Alarm Events API)

用于查询已触发的告警事件。

### 4.1 查询告警事件

- **URL**: `/alarm/events`
- **Method**: `GET`
- **说明**: 返回告警事件列表。支持通过查询参数进行过滤。
- **查询参数说明**:
  - `limit` (Integer, optional): 限制返回的最近事件数量。例如 `?limit=50` 返回最多50条。如果未提供，默认为100。
  - `status` (String, optional): 根据状态筛选事件。
    - `active` 或 `firing`: 只返回当前处于“触发中”状态的告警。
    - 如果不提供此参数，将返回所有最近的事件（包括已解决的）。
- **成功响应 (200 OK)**: 返回一个包含告警事件对象的JSON数组。
  ```json
  [
    {
      "id": 1,
      "fingerprint": "...",
      "status": "firing",
      "labels": { "host_ip": "192.168.1.100" },
      "annotations": { "summary": "CPU使用率过高", "description":""},
      "starts_at": "...",
      "ends_at": null,
      "generator_url": "...",
      "created_at": "...",
      "updated_at": "..."
    }
  ]
  ```
- **示例 (curl)**:
  - 获取最近50条事件:
    ```bash
    curl "http://localhost:8080/alarm/events?limit=50"
    ```
  - 获取所有活跃的告警:
    ```bash
    curl "http://localhost:8080/alarm/events?status=active"
    ```
---

## 5. 告警规则可用指标 (Metrics & Tags)

本节详细列出了在定义告警规则的 `expression` 时，可以使用的 `stable`, `metric` 和 `tags` 的所有可用值。

### 5.1 `stable: "cpu"`
| 类型 (Type) | 名称 (Name)      | 说明 (Description)      |
|-------------|------------------|-------------------------|
| **Metric**  | `usage_percent`  | CPU使用率 (%)           |
| **Metric**  | `load_avg_1m`    | 1分钟平均负载           |
| **Metric**  | `load_avg_5m`    | 5分钟平均负载           |
| **Metric**  | `load_avg_15m`   | 15分钟平均负载          |
| **Metric**  | `core_count`     | CPU核心数               |
| **Metric**  | `core_allocated` | 已分配的核心数          |
| **Metric**  | `temperature`    | 温度                    |
| **Metric**  | `voltage`        | 电压                    |
| **Metric**  | `current`        | 电流                    |
| **Metric**  | `power`          | 功率                    |
| **Tag**     | `host_ip`        | 主机IP地址              |

### 5.2 `stable: "memory"`
| 类型 (Type) | 名称 (Name)      | 说明 (Description)      |
|-------------|------------------|-------------------------|
| **Metric**  | `total`          | 总内存 (Bytes)          |
| **Metric**  | `used`           | 已用内存 (Bytes)        |
| **Metric**  | `free`           | 空闲内存 (Bytes)        |
| **Metric**  | `usage_percent`  | 内存使用率 (%)          |
| **Tag**     | `host_ip`        | 主机IP地址              |

### 5.3 `stable: "disk"`
| 类型 (Type) | 名称 (Name)      | 说明 (Description)      |
|-------------|------------------|-------------------------|
| **Metric**  | `total`          | 总空间 (Bytes)          |
| **Metric**  | `used`           | 已用空间 (Bytes)        |
| **Metric**  | `free`           | 空闲空间 (Bytes)        |
| **Metric**  | `usage_percent`  | 磁盘使用率 (%)          |
| **Tag**     | `host_ip`        | 主机IP地址              |
| **Tag**     | `device`         | 设备名 (e.g., /dev/sda1) |
| **Tag**     | `mount_point`    | 挂载点 (e.g., /data)    |

### 5.4 `stable: "network"`
| 类型 (Type) | 名称 (Name)      | 说明 (Description)      |
|-------------|------------------|-------------------------|
| **Metric**  | `rx_bytes`       | 接收字节数              |
| **Metric**  | `tx_bytes`       | 发送字节数              |
| **Metric**  | `rx_packets`     | 接收包数量              |
| **Metric**  | `tx_packets`     | 发送包数量              |
| **Metric**  | `rx_errors`      | 接收错误数              |
| **Metric**  | `tx_errors`      | 发送错误数              |
| **Metric**  | `rx_rate`        | 接收速率 (Bytes/s)      |
| **Metric**  | `tx_rate`        | 发送速率 (Bytes/s)      |
| **Tag**     | `host_ip`        | 主机IP地址              |
| **Tag**     | `interface`      | 网络接口名 (e.g., eth0) |

### 5.5 `stable: "gpu"`
| 类型 (Type) | 名称 (Name)      | 说明 (Description)      |
|-------------|------------------|-------------------------|
| **Metric**  | `compute_usage`  | 计算使用率 (%)          |
| **Metric**  | `mem_usage`      | 显存使用率 (%)          |
| **Metric**  | `mem_used`       | 已用显存 (Bytes)        |
| **Metric**  | `mem_total`      | 总显存 (Bytes)          |
| **Metric**  | `temperature`    | 温度                    |
| **Metric**  | `power`          | 功率                    |
| **Tag**     | `host_ip`        | 主机IP地址              |
| **Tag**     | `gpu_index`      | GPU索引 (e.g., 0, 1)    |
| **Tag**     | `gpu_name`       | GPU名称                 |

### 5.6 `stable: "node"`
| 类型 (Type) | 名称 (Name)      | 说明 (Description)      |
|-------------|------------------|-------------------------|
| **Metric**  | `gpu_allocated`  | 已分配的GPU数量         |
| **Metric**  | `gpu_num`        | 节点上的GPU总数         |
| **Tag**     | `host_ip`        | 主机IP地址              |

