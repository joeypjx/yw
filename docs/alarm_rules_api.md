添加告警规则接口

这个接口用于创建一个新的告警规则。

*   **接口:** `POST /alarm/rules`
*   **功能:** 添加一个新的告警规则。
*   **请求报文格式:** `application/json`

告警规则JSON对象包含以下字段：

| 字段                  | 类型             | 必选 | 说明                                                                                              | 示例值                                                                  |
| --------------------- | ---------------- | ---- | ------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| `templateId`          | string           | 是   | 告警规则标识，可使用中文或者英文标识。                                                                        | `"CPU使用过高"`                                                      |
| `metricName`          | string           | 是   | 关联的监控指标名称。                                                                              | `"resource.cpu.usage_percent"`                                                   |
| `alarmType`           | string           | 是   | 告警类型，包括 "硬件状态" 或 "业务链路"。                                                          | `"system"`                                                              |
| `alarmLevel`          | string           | 是   | 告警级别，包括 "严重", "一般", "提示"。                                                   | `"critical"`                                                            |
| `triggerCountThreshold` | integer        | 是   | 连续多少次满足条件才触发告警, 每次间隔5秒。                                            | `3`                                                                     |
| `contentTemplate`     | string           | 是   | 告警内容的模板，可以使用占位符，占位符包括{nodeId}、{alarmType}、{alarmLevel}、{metricName}、{value}。                                                                  | `" 节点 {nodeId} 发生 {alarmLevel} 告警，..."`                     |
| `condition`           | object           | 是   | 触发条件的定义，包括简单条件和复合条件。                                                                                 | `{"type": "GreaterThan", "threshold": 5.0}`                             |
| `actions`             | array of objects | 是   | 告警触发后需要执行的动作列表，包括事件推送`Notify`和事件记录`Database`。                                          | `[{"type": "Log"}, {"type": "Database"}]`                               |

**`metricName` 对象详解:**
*   **不带条件的对象:**
    *  `resource.cpu.usage_percent` CPU 的 占用率
*   **带条件的对象:**
    *  `resource.disk[mount_point=/].usage_percent` 根目录的 磁盘 的使用率

**`condition` 对象详解:**
*   **简单条件:**
    *   `type`: `GreaterThan`, `LessThan` 等。
    *   `threshold`: 用于比较的阈值，比如大于90。
*   **复合条件:**
    *   `type`: `And` 或 `Or`。
    *   `conditions`: 一个包含多个条件对象的数组，用于进行逻辑组合，比如大于80且小于90。

**请求示例:**
```json
{
    "templateId": "CPU使用过高",
    "metricName": "resource.cpu.usage_percent",
    "alarmType": "硬件状态",
    "alarmLevel": "严重",
    "triggerCountThreshold": 3,
    "contentTemplate": "节点 {nodeId} 发生 {alarmLevel} 告警，CPU使用值为 {value}",
    "condition": {
        "type": "GreaterThan",
        "threshold": 90.0
    },
    "actions": [
        {
            "type": "Notify"
        },
        {
            "type": "Database"
        }
    ]
}
```

```json
{
    "templateId": "根目录的磁盘占用较高",
    "metricName": "resource.disk[mount_point=/].usage_percent",
    "alarmType": "硬件状态",
    "alarmLevel": "提示",
    "triggerCountThreshold": 3,
    "contentTemplate": "节点 {nodeId} 发生 {alarmLevel} 告警，根目录的磁盘占用为 {value}",
    "condition": {
        "type": "And",
        "conditions": [
            {
                "threshold": 60.0,
                "type": "GreaterThan"
            },
            {
                "threshold": 90.0,
                "type": "LessThan"
            }
        ]
    },
    "actions": [
        {
            "type": "Notify"
        },
        {
            "type": "Database"
        }
    ]
}
```

contentTemplate转换成description时，
占位符{nodeId}、{alarmLevel}、{value} 
变为 {{host_ip}} {{severity}} {{value}}

  "metricName": resource.disk[mount_point=/].usage_percent
  "condition": {
        "type": "GreaterThan",
        "threshold": 90.0
  }
  转换为
  expression:
    and:
    - stable: disk
      tag: mount_point
      operator: '=='
      threshold: '/'
    - stable: disk
      tag: usage_percent
      operator: '>'
      value: 90.0