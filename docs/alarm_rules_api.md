增加告警规则的HTTP请求格式

  请求信息

  - 方法: POST
  - 路径: /alarm/rules
  - Content-Type: application/json

  请求体格式

  {
    "alert_name": "告警规则名称",
    "expression": {
      "stable": "超级表名称",
      "metric": "指标名称",
      "conditions": [
        {
          "operator": "操作符",
          "threshold": 阈值
        }
      ],
      "tags": [
        {
          "标签名": "标签值"
        }
      ]
    },
    "for": "持续时间",
    "severity": "严重等级",
    "summary": "告警摘要",
    "description": "告警详细描述",
    "alert_type": "告警类型"
  }

  具体示例

  1. 简单CPU使用率告警

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

  2. 带标签条件的磁盘告警

  {
    "alert_name": "DataDiskSpaceIssue",
    "expression": {
      "stable": "disk",
      "tags": [
        {
          "mount_point": "/data"
        }
      ],
      "metric": "usage_percent",
      "conditions": [
        {
          "operator": ">",
          "threshold": 90.0
        }
      ]
    },
    "for": "0s",
    "severity": "严重",
    "summary": "数据磁盘空间问题",
    "description": "节点 {{host_ip}} 的磁盘 {{mount_point}} 空间不足：使用率 {{usage_percent}}%。",
    "alert_type": "硬件资源"
  }

  3. 多条件告警（区间判断）

  {
    "alert_name": "DiskSpaceWarning",
    "expression": {
      "stable": "disk",
      "tags": [
        {
          "mount_point": "/data"
        }
      ],
      "metric": "usage_percent",
      "conditions": [
        {
          "operator": ">",
          "threshold": 50.0
        },
        {
          "operator": "<",
          "threshold": 90.0
        }
      ]
    },
    "for": "2m",
    "severity": "提示",
    "summary": "磁盘空间风险提示",
    "description": "节点 {{host_ip}} 的磁盘 {{mount_point}} 空间风险提示：使用率 {{usage_percent}}%。",
    "alert_type": "硬件资源"
  }

  字段说明

  | 字段                                | 类型     | 必需  | 说明                                      |
  |-----------------------------------|--------|-----|-----------------------------------------|
  | alert_name                        | String | ✅   | 告警规则的唯一名称                               |
  | expression                        | Object | ✅   | 告警表达式对象                                 |
  | expression.stable                 | String | ✅   | 超级表名称 (cpu, memory, disk, network, gpu) |
  | expression.metric                 | String | ✅   | 指标名称 (usage_percent, compute_usage等)    |
  | expression.conditions             | Array  | ✅   | 条件数组                                    |
  | expression.conditions[].operator  | String | ✅   | 操作符 (>, <, >=, <=, =, !=)               |
  | expression.conditions[].threshold | Number | ✅   | 阈值                                      |
  | expression.tags                   | Array  | ❌   | 标签过滤条件                                  |
  | for                               | String | ✅   | 持续时间 (如: "5m", "30s", "1h")             |
  | severity                          | String | ✅   | 严重等级 (提示, 一般, 严重)                       |
  | summary                           | String | ✅   | 告警摘要                                    |
  | description                       | String | ✅   | 告警详细描述，支持模板变量                           |
  | alert_type                        | String | ✅   | 告警类型 (硬件资源, 业务链路, 系统故障)                 |

  响应格式

  成功响应 (200)

  {
    "status": "success",
    "id": "uuid-generated-id"
  }

  错误响应 (400/500)

  {
    "error": "错误描述信息"
  }

  完整的curl请求示例

  curl -X POST http://localhost:8080/alarm/rules \
    -H "Content-Type: application/json" \
    -d '{
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
    }'