告警子系统详细设计说明书
1. 系统概述
1.1. 目标
本告警子系统旨在为服务器集群监控平台提供实时、准确、可管理的告警能力。系统能够接收并处理各服务器节点上报的资源状态数据，通过灵活配置的告警规则检测异常状态，并以智能、低噪的方式将告警通知分发给相关负责人。

1.2. 核心架构
本子系统采用模块化设计，由四个核心组件构成，各组件职责分明、低耦合，确保系统的高可靠性与可扩展性。

graph TD
    A[服务器集群节点] -->|推送JSON指标| B(HTTP数据接收服务);
    B -->|写入时序数据| C(时序数据库 - TDengine);
    
    subgraph 告警子系统
        D(告警规则引擎) -->|查询数据| C;
        D -->|发送告警事件| E(告警管理器);
        E -->|发送通知指令| F(告警通知分发器);
    end

    F -->|Email| G[邮件系统];
    F -->|Webhook| H[即时通讯工具(Slack/钉钉)];
    F -->|API Call| I[电话告警平台(PagerDuty)];

时序数据库 (Time-Series Database - TDengine): 负责高效存储和查询所有服务器节点上报的时间序列指标数据，是整个系统的数据基石。

告警规则引擎 (Alerting Engine): 告警功能的大脑。负责定期从数据库查询数据，根据用户定义的规则进行评估，并生成原始的告警事件。

告警管理器 (Alert Manager): 告警的“调度中心”。负责对原始告警事件进行去重、分组、静默和抑制等降噪处理，并根据路由规则决定通知策略。

告警通知分发器 (Notification Dispatcher): 告警的“传声筒”。负责将格式化好的告警消息，通过不同渠道（Email, Webhook等）发送给最终用户。

2. 设计重点详解
2.1. 时序数据库 (TDengine) 数据表设计
我们采用TDengine的“一个采集点一张表”核心模型，为不同类型的监控实体设计专属的超级表 (STable)。

设计原则:

超级表 (STable): 作为同类采集点（如所有服务器、所有磁盘）的模板，定义统一的指标列和元数据标签。

子表 (Sub-table): 代表一个具体的物理实体（如一台服务器、一块磁盘），由TDengine在首次写入数据时自动创建。

列 (Columns): 用于存储随时间频繁变化的数值型指标。

标签 (Tags): 用于存储描述采集点身份的、基本不变的元数据。查询应优先利用标签进行过滤。

标签冗余: 为了避免查询时进行高成本的JOIN操作，建议在子级超级表（如disks）中冗余父级实体（如nodes）的关键标签（如cluster, region）。

表结构定义:

CPU指标超级表 (cpu_metrics): 存储CPU相关指标。

CREATE STABLE IF NOT EXISTS cpu_metrics (
    ts                   TIMESTAMP,
    usage_percent        DOUBLE,     -- CPU使用率
    load_avg_1m          DOUBLE,     -- 1分钟平均负载
    load_avg_5m          DOUBLE,     -- 5分钟平均负载
    load_avg_15m         DOUBLE,     -- 15分钟平均负载
    core_count           INT,        -- CPU核心数
    core_allocated       INT,        -- 已分配核心数
    temperature          DOUBLE,     -- CPU温度
    voltage              DOUBLE,     -- 电压
    current              DOUBLE,     -- 电流
    power                DOUBLE      -- 功耗
) TAGS (
    host_ip              NCHAR(16)   -- 主机IP地址
);

内存指标超级表 (memory_metrics): 存储内存相关指标。

CREATE STABLE IF NOT EXISTS memory_metrics (
    ts                   TIMESTAMP,
    total                BIGINT,     -- 总内存
    used                 BIGINT,     -- 已使用内存
    free                 BIGINT,     -- 可用内存
    usage_percent        DOUBLE      -- 内存使用率
) TAGS (
    host_ip              NCHAR(16)   -- 主机IP地址
);

磁盘指标超级表 (disk_metrics): 存储每个独立磁盘分区的指标。

CREATE STABLE IF NOT EXISTS disk_metrics (
    ts                   TIMESTAMP,
    total                BIGINT,     -- 总容量
    used                 BIGINT,     -- 已使用容量
    free                 BIGINT,     -- 可用容量
    usage_percent        DOUBLE      -- 磁盘使用率
) TAGS (
    host_ip              NCHAR(16),  -- 主机IP地址
    device               NCHAR(32),  -- 磁盘设备名 (e.g., /dev/sda1)
    mount_point          NCHAR(64)   -- 挂载点 (e.g., /)
);

网络接口指标超级表 (network_metrics): 存储每个独立网络接口的指标。

CREATE STABLE IF NOT EXISTS network_metrics (
    ts                   TIMESTAMP,
    rx_bytes             BIGINT,     -- 接收字节数
    tx_bytes             BIGINT,     -- 发送字节数
    rx_packets           BIGINT,     -- 接收包数
    tx_packets           BIGINT,     -- 发送包数
    rx_errors            INT,        -- 接收错误数
    tx_errors            INT         -- 发送错误数
) TAGS (
    host_ip              NCHAR(16),  -- 主机IP地址
    interface            NCHAR(32)   -- 网卡名 (e.g., eth0)
);

GPU指标超级表 (gpu_metrics): 存储GPU相关指标。

CREATE STABLE IF NOT EXISTS gpu_metrics (
    ts                   TIMESTAMP,
    compute_usage        DOUBLE,     -- 计算使用率
    mem_usage            DOUBLE,     -- 显存使用率
    mem_used             BIGINT,     -- 已使用显存
    mem_total            BIGINT,     -- 总显存
    temperature          DOUBLE,     -- GPU温度
    voltage              DOUBLE,     -- 电压
    current              DOUBLE,     -- 电流
    power                DOUBLE      -- 功耗
) TAGS (
    host_ip              NCHAR(16),  -- 主机IP地址
    gpu_index            INT,        -- GPU索引
    gpu_name             NCHAR(64)   -- GPU名称
);

2.2. 告警规则结构设计
告警规则采用结构化的YAML/JSON格式定义，以支持复杂的逻辑判断，而非简单的字符串。

核心字段:

alert_name: 告警规则的唯一名称。

expression: 告警的核心判断逻辑，为一个可递归嵌套的对象。

for: 持续时间。条件必须持续满足此时间才触发告警，用于防抖。

severity: 告警的严重等级（如 warning, critical）。

summary: 告警的简短摘要。

description: 告警的详细描述，支持模板变量。

规则示例:

# 规则1：简单指标告警
- alert_name: HighCpuUsage
  for: 5m
  severity: critical
  summary: "CPU使用率过高"
  description: "节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%。"
  expression:
    stable: cpu_metrics
    metric: usage_percent
    operator: '>'
    threshold: 90.0

# 规则2：AND 逻辑 - 特定主机的高CPU使用率
- alert_name: HighCpuOnSpecificHost
  for: 2m
  severity: warning
  summary: "特定主机CPU使用率过高"
  description: "主机 {{host_ip}} CPU使用率达到 {{usage_percent}}%。"
  expression:
    and:
    - stable: cpu_metrics
      metric: usage_percent
      operator: '>'
      threshold: 80.0
    - stable: cpu_metrics
      tag: host_ip
      operator: '=='
      value: '192.168.1.100'

# 规则3：OR 逻辑 - 磁盘使用率过高或可用空间过低
- alert_name: DiskSpaceIssue
  for: 5m
  severity: critical
  summary: "磁盘空间问题"
  description: "磁盘 {{device}} 空间不足：使用率 {{usage_percent}}%，可用空间 {{free}} 字节。"
  expression:
    or:
    - stable: disk_metrics
      metric: usage_percent
      operator: '>'
      threshold: 85.0
    - stable: disk_metrics
      metric: free
      operator: '<'
      threshold: 1000000000  # 1GB

# 规则4：嵌套逻辑 - 特定主机且(CPU高或内存高)
- alert_name: NestedComplexRule
  for: 3m
  severity: critical
  summary: "嵌套复杂告警"
  description: "主机 {{host_ip}} 出现资源问题。"
  expression:
    and:
    - stable: cpu_metrics
      tag: host_ip
      operator: '=='
      value: '192.168.1.100'
    - or:
      - stable: cpu_metrics
        metric: usage_percent
        operator: '>'
        threshold: 90.0
      - stable: memory_metrics  # 注意：跨超级表，当前只处理第一个stable
        metric: usage_percent
        operator: '>'
        threshold: 95.0

# 规则5：GPU计算使用率过高告警
- alert_name: HighGpuUsage
  for: 3m
  severity: warning
  summary: "GPU计算使用率过高"
  description: "节点 {{host_ip}} GPU {{gpu_name}} 计算使用率达到 {{compute_usage}}%。"
  expression:
    stable: gpu_metrics
    metric: compute_usage
    operator: '>'
    threshold: 80.0

2.3. 告警规则与数据库查询语句转换设计
告警规则引擎内置一个"规则到SQL转换器"，负责将结构化的规则对象动态转换为可执行的TDengine SQL。

## 当前实现功能

**支持的表达式类型**：
- 简单条件：单个指标或标签条件
- AND 逻辑：多个条件的逻辑AND组合
- OR 逻辑：多个条件的逻辑OR组合  
- 嵌套逻辑：AND和OR的任意层级嵌套

**支持的操作符**：
- 比较操作符：`>`, `<`, `>=`, `<=`, `==`, `!=`
- 逻辑操作符：`and`, `or`

**支持的条件类型**：
- 标签条件：基于标签的过滤（转换为WHERE子句）
- 指标条件：基于指标值的直接过滤（转换为WHERE子句）

## 转换策略

**单超级表规则**：

1. **表达式解析**：递归遍历expression树，识别标签条件和指标条件
2. **WHERE子句构建**：将包含`tag`字段的条件转换为WHERE子句
3. **指标条件构建**：将包含`metric`字段的条件转换为WHERE子句
4. **逻辑操作符转换**：`and`→`AND`, `or`→`OR`
5. **时间窗口添加**：自动添加时间过滤条件
6. **时间排序**：按时间戳降序排序，获取最新的记录

**SQL生成模板**：
```sql
SELECT {metric} , ts, {tag_fields...}
FROM {stable}
WHERE {tag_conditions} AND {metric_conditions} 
ORDER BY ts DESC
```

**SQL生成模板**：
```sql
SELECT LAST({metric}) AS value, {tag_fields...}, ts
FROM {stable}
WHERE {tag_conditions} AND {metric_conditions} AND (ts > NOW() - 10s)
GROUP BY host_ip, {tag_fields...};
```

operator < <= > >= = !=

expression:
  stable: cpu
  metric: usage_percent
  operator: '>'
  threshold: 90.0

expression:
  stable: disk
  metric: usage_percent
  tags:
  - mount_point: '/data'
  operator: '>'
  threshold: 90.0

expression:
  stable: disk
  metric: usage_percent
  tags:
  - mount_point: '/data'
  and:
  - operator: '>'
    threshold: 90.0
  - operator: '<'
    threshold: 95.0

expression:
  stable: disk
  metric: usage_percent
  tags:
  - mount_point: '/data'
  or:
  - operator: '>'
    threshold: 95.0
  - operator: '<'
    threshold: 90.0

SELECT LAST(usage_percent) AS data, ts, host_ip, mount_point
FROM disk
WHERE usage_percent > 90.0 AND usage_percent < 95.0 AND mount_point = '/data' AND (ts > now() - 10s)
GROUP BY host_ip, mount_point;

## 转换示例

### 示例1：简单指标告警
```yaml
# 输入规则
expression:
  stable: cpu_metrics
  metric: usage_percent
  operator: '>'
  threshold: 90.0
```

```sql
-- 生成的SQL
SELECT usage_percent , ts, host_ip 
FROM cpu_metrics 
WHERE usage_percent > 90.0  
ORDER BY ts DESC
```

### 示例2：AND逻辑 - 特定主机的高CPU使用率
```yaml
# 输入规则
expression:
  and:
  - stable: cpu_metrics
    metric: usage_percent
    operator: '>'
    threshold: 80.0
  - stable: cpu_metrics
    tag: host_ip
    operator: '=='
    value: '192.168.1.100'
```

```sql
-- 生成的SQL
SELECT usage_percent , ts, host_ip 
FROM cpu_metrics 
WHERE (host_ip = '192.168.1.100') AND (usage_percent > 80.0)  
ORDER BY ts DESC
```

### 示例3：OR逻辑 - 磁盘使用率过高或可用空间过低
```yaml
# 输入规则
expression:
  or:
  - stable: disk_metrics
    metric: usage_percent
    operator: '>'
    threshold: 85.0
  - stable: disk_metrics
    metric: free
    operator: '<'
    threshold: 1000000000
```

```sql
-- 生成的SQL
SELECT usage_percent , ts, host_ip, device, mount_point 
FROM disk_metrics 
WHERE (usage_percent > 85.0 OR free < 1000000000)  
ORDER BY ts DESC
```

### 示例4：嵌套逻辑 - 特定主机且(CPU高或内存高)
```yaml
# 输入规则
expression:
  and:
  - stable: cpu_metrics
    tag: host_ip
    operator: '=='
    value: '192.168.1.100'
  - or:
    - stable: cpu_metrics
      metric: usage_percent
      operator: '>'
      threshold: 90.0
    - stable: memory_metrics  # 注意：跨超级表条件被忽略
      metric: usage_percent
      operator: '>'
      threshold: 95.0
```

```sql
-- 生成的SQL（只处理第一个stable：cpu_metrics）
SELECT usage_percent , ts, host_ip 
FROM cpu_metrics 
WHERE (host_ip = '192.168.1.100') AND (usage_percent > 90.0 OR usage_percent > 95.0)  
ORDER BY ts DESC
```

## 标签字段映射

不同超级表包含不同的标签字段，转换器会根据stable类型自动选择对应的标签字段：

- **cpu_metrics**: `host_ip`
- **memory_metrics**: `host_ip`
- **disk_metrics**: `host_ip`, `device`, `mount_point`
- **network_metrics**: `host_ip`, `interface`
- **gpu_metrics**: `host_ip`, `gpu_index`, `gpu_name`

## 当前限制

**单超级表限制**：
- 当前实现只支持单超级表规则
- 涉及多个超级表的复杂规则（如嵌套规则示例4）会被简化处理
- 系统会选择第一个遇到的stable，忽略其他stable的条件

**跨超级表规则**：
- 计划支持，但当前版本未实现
- 需要实现"分而治之，应用层合并"策略
- 将在后续版本中支持多超级表JOIN查询

**触发机制**：
- 基于时间点的触发，不使用聚合函数
- 查询返回满足条件的最新记录
- 按时间戳降序排序，实时检测异常状态

2.4. 告警实例状态管理设计
告警实例的状态管理职责被明确地划分到告警规则引擎和告警管理器两个组件中，以实现高内聚、低耦合。

告警规则引擎的职责 (检测层状态):

核心任务: 维护告警实例基于数据的生命周期状态。

告警指纹 (Fingerprint): 为每个独立的告警实例生成唯一指纹（alert_name + 实例的唯一标签组合，如node_id）。

状态机: 对每个指纹，独立维护一个状态机：

Inactive: 条件不满足。

Pending: 条件首次满足，开始计时（等待for时长）。如果在Pending期间，该测试实例发生条件不满足，则退回到Inactive。

Firing: 条件持续满足超过for时长，正式触发告警。

Resolved: 此前处于Firing状态的告警，条件不再满足。如果在Resolved期间，该测试实例发生条件不满足，则退回到Inactive。

告警管理器的职责 (通知层状态):

核心任务: 维护所有与通知行为相关的、更上层的聚合状态。

管理的状态包括:

分组 (Grouping): 哪些告警被分为一组。

静默 (Silencing): 用户定义的静默规则及其生效状态。

抑制 (Inhibition): 告警间的抑制关系。

通知计时 (Notification Timers): 包括初次发送延迟 (group_wait) 和重复发送间隔 (repeat_interval)。

2.5. 告警事件结构设计
告警规则引擎在告警实例状态变为Firing或Resolved时，会生成一个结构化的告警事件，发送给告警管理器。

事件JSON结构:

{
  "fingerprint": "alertname=HighCpuUsage,host_ip=192.168.1.100",
  "status": "firing", // "firing" 或 "resolved"
  "labels": {
    "alertname": "HighCpuUsage",
    "host_ip": "192.168.1.100",
    "severity": "critical"
  },
  "annotations": {
    "summary": "CPU使用率过高",
    "description": "节点 192.168.1.100 CPU使用率达到 95.2%。"
  },
  "starts_at": "2025-07-15T15:50:00Z", // 首次满足条件的时间 (进入Pending状态的时间)
  "ends_at": null, // 告警恢复的时间 (仅在status为resolved时有值)
}

磁盘告警事件示例:

{
  "fingerprint": "alertname=HighDiskUsage,host_ip=192.168.1.100,device=/dev/sda1",
  "status": "firing",
  "labels": {
    "alertname": "HighDiskUsage",
    "host_ip": "192.168.1.100",
    "device": "/dev/sda1",
    "mount_point": "/",
    "severity": "warning"
  },
  "annotations": {
    "summary": "磁盘使用率过高",
    "description": "节点 192.168.1.100 磁盘 /dev/sda1 使用率达到 88.5%。"
  },
  "starts_at": "2025-07-15T15:50:00Z",
  "ends_at": null,
}

2.6. 告警管理器工作逻辑设计
告警管理器是实现告警降噪和智能分发的关键。

工作流程:

事件处理流水线 (同步):

接收: 通过API接收来自引擎的告警事件。

静默检查: 将事件与所有活跃的静默规则进行匹配，若匹配成功则丢弃。

抑制检查: 将事件与抑制规则进行匹配，若发现有更高优先级的告警正在触发，则丢弃。

分组: 对通过筛选的事件，根据group_by配置（如按cluster, alertname分组）计算group_key，并将其放入对应的告警组 (AlertGroup)。

异步通知循环 (独立任务):

该循环定期（如每30秒）执行，遍历所有活跃的AlertGroup。

计时判断: 对每个组，检查是否满足发送条件（已度过group_wait初次等待期，或已度过repeat_interval重复发送间隔）。

路由: 若满足发送条件，根据组的标签匹配路由规则，确定接收方 (receiver)。

模板渲染: 使用模板引擎，将组内所有告警信息渲染成对人类友好的消息。

分发: 将渲染好的消息和接收方信息，交给告警通知分发器处理。

状态更新: 更新该组的“上次通知时间”。

2.7. 告警通知分发器工作逻辑设计
该组件作为告警管理器的子模块，负责最终的发送动作。

设计:

输入: 接收一个包含“接收方名称”和“格式化消息内容”的指令。

适配器模式 (Adapter Pattern): 内部为每种通知渠道（Email, Webhook, Slack等）实现一个独立的适配器。

逻辑:

根据传入的“接收方名称”查找对应的适配器。

从配置中获取该接收方所需的凭证（如API Token, SMTP服务器地址等）。

调用适配器的send()方法，将消息内容发送出去。

处理发送结果，记录日志。

这种设计使得增加新的通知渠道变得非常简单，只需添加一个新的适配器并更新配置即可，无需改动核心逻辑。