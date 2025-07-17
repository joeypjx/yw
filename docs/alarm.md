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
severity: 告警的严重等级（可自定义，如严重,一般,提示）。
alert_type: 告警类型（可自定义，如 硬件资源,业务链路,系统故障）
summary: 告警的简短摘要。
description: 告警的详细描述，支持模板变量。

规则示例:

# 规则1：简单指标告警
- alert_name: HighCpuUsage
  for: 5m
  severity: "严重"
  alert_type: "硬件资源"
  summary: "CPU使用率过高"
  description: "节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%。"
  expression:
    stable: cpu
    metric: usage_percent
    conditions:
    - operator: ">"
      threshold: 80.0

# 规则2：指定磁盘空间使用率过高
- alert_name: DiskSpaceIssue
  for: 0s
  severity: "严重"
  alert_type: "硬件资源"
  summary: "磁盘空间问题"
  description: "节点 {{host_ip}} 的磁盘 {{mount_point}} 空间不足：使用率 {{usage_percent}}%。"
  expression:
    stable: disk
    tags:
    - mount_point: "/data"
    metric: usage_percent
    conditions: 
    - operator: ">"
      threshold: 90.0
      

# 规则3：指定磁盘空间使用率区间
- alert_name: DiskSpaceIssue2
  for: 0s
  severity: "提示"
  alert_type: "硬件资源"
  summary: "磁盘空间问题"
  description: "节点 {{host_ip}} 的磁盘 {{mount_point}} 空间风险提示：使用率 {{usage_percent}}%。"
  expression:
    stable: disk
    tags:
    - mount_point: "/data"
    metric: usage_percent
    conditions: 
    - operator: ">"
      threshold: 50.0
    - operator: "<"
      threshold: 90.0

2.3. 告警规则与数据库查询语句转换设计
告警规则引擎内置一个"规则到SQL转换器"，负责将结构化的规则对象动态转换为可执行的TDengine SQL。

## 当前实现功能

**支持的表达式类型**：
- 简单条件：单个条件
- 复杂条件：多个条件

**支持的操作符**：
- operator操作符：`>`, `<`, `>=`, `<=`, `=`, `!=`

**支持的条件类型**：
- 标签条件：基于标签的过滤（转换为WHERE子句）
- 指标条件：基于指标值的直接过滤（转换为WHERE子句）

## 转换策略

**SQL生成模板**：
```sql
SELECT LAST({metric}), host_ip, {tag_fields...}, ts
FROM {stable}
WHERE {tag_conditions} AND {metric_conditions} AND (ts > NOW() - 10s)
GROUP BY host_ip, {tag_fields...};
```

## 转换示例

### 示例1：简单指标告警
```yaml
# 输入规则
  expression:
    stable: cpu
    metric: usage_percent
    conditions:
    - operator: ">"
      threshold: 80.0
```

```sql
-- 生成的SQL
SELECT LAST(usage_percent), host_ip, ts
FROM cpu
WHERE usage_percent > 80.0 AND (ts > NOW() - 10s)
GROUP BY host_ip;
```

### 示例2：复杂逻辑 - 指定磁盘空间使用率过高
```yaml
# 输入规则
  expression:
    stable: disk
    tags:
    - mount_point: "/data"
    metric: usage_percent
    conditions: 
    - operator: ">"
      threshold: 90.0
```

```sql
-- 生成的SQL
SELECT LAST(usage_percent), host_ip, mount_point, ts
FROM disk 
WHERE (mount_point = '/data') AND (usage_percent > 90.0) AND (ts > NOW() - 10s)
GROUP BY host_ip, mount_point;
```

### 示例3：复杂逻辑 - 指定磁盘空间使用率区间
```yaml
# 输入规则
  expression:
    stable: disk
    tags:
    - mount_point: "/data"
    metric: usage_percent
    conditions: 
    - operator: ">"
      threshold: 50.0
    - operator: "<"
      threshold: 90.0
```

```sql
-- 生成的SQL
SELECT LAST(usage_percent), host_ip, mount_point, ts
FROM disk 
WHERE (mount_point = '/data') AND (usage_percent > 50.0) AND (usage_percent < 90.0) AND (ts > NOW() - 10s)
GROUP BY host_ip, mount_point;
```
2.4. 告警实例产生与状态管理

在执行完数据库查询后，我们得到的是一个“当前满足阈值条件的节点列表”，而告警引擎需要将这个瞬时快照与它在内存中维护的历史状态进行对比，从而判断出每个告警实例的准确状态。

这个过程可以概括为一个“状态协调 (State Reconciliation)”算法。下面是详细的步骤和逻辑。

前提：引擎的两个关键数据
在每个评估周期开始时，引擎手头有两份关键数据：
当前查询结果 (active_from_db): 一个从数据库查询返回的、当前所有满足条件的告警实例的指纹 (Fingerprint) 列表。例如 ['alertname=HighDiskUsage,host_ip=192.168.1.100,device=/dev/sda1', 'alertname=HighDiskUsage,host_ip=192.168.1.200,device=/dev/sda2']。Fingerprint:告警实例全局唯一指纹，由alertname和排序后的tags生成
上一周期的状态表 (previous_state_map): 一个存储在内存中的哈希表（Map），记录了上一周期所有处于 Pending 或 Firing 状态的告警实例及其状态。
Key: 告警指纹
Value: 一个状态对象，如 { status: 'Pending', startTime: '2025-07-17T12:30:00Z' }

状态判断的完整算法
告警引擎会遍历这两个数据，通过对比来决定每个告警实例的新状态。我们可以把这个过程分解为三个主要场景。

场景一：新出现的告警 (New Alerts)
判断条件: 一个指纹存在于 active_from_db 中，但不存在于 previous_state_map 中。
逻辑含义: 这是我们第一次检测到这个告警实例满足条件。
引擎动作:
在内存中为这个新的指纹创建一个状态条目。
将其状态设置为 Pending。
记录下当前时间作为 startTime。
此时不发送任何通知。引擎需要等待，看这个状态是否会持续。
示例:
active_from_db: ['alertname=HighDiskUsage,host_ip=192.168.1.100,device=/dev/sda1']
previous_state_map: {} (空的)
结果: previous_state_map 更新为 {'alertname=HighDiskUsage,host_ip=192.168.1.100,device=/dev/sda1': {status: 'Pending', startTime: NOW}}。

场景二：持续存在的告警 (Ongoing Alerts)
判断条件: 一个指纹同时存在于 active_from_db 和 previous_state_map 中。
逻辑含义: 这个告警实例在上一个周期就已经被检测到，现在依然满足条件。
引擎动作: 检查它在 previous_state_map 中的当前状态：
如果当前状态是 Pending:
引擎需要检查计时器：当前时间 - startTime >= 规则中定义的 for 时长？
如果 true (时间已到): 告警正式触发！
将该指纹的状态更新为 Firing。
生成一个 Firing 状态的告警事件，并将其发送给告警管理器。这是告警首次对外发出的时刻。
如果 false (时间未到): 保持现状，状态依然是 Pending。
如果当前状态是 Firing:
说明告警早已触发，并且现在依然持续。
引擎什么也不做。状态保持为 Firing。是否需要重复发送通知，由下游的告警管理器根据其 repeat_interval 策略来决定。

场景三：已恢复的告警 (Resolved Alerts)
判断条件: 一个指纹存在于 previous_state_map 中，但不存在于 active_from_db 中。
逻辑含义: 上一周期还在告警的实例，现在已经恢复正常了。
引擎动作: 检查它在 previous_state_map 中的状态：
如果状态是 Firing:
说明一个正式触发的告警已经解决。
生成一个 Resolved 状态的告警事件，并将其发送给告警管理器，用于通知用户问题已恢复。
从 previous_state_map 中删除这个指纹的条目。
如果状态是 Pending:
说明这个告警在还未正式触发（未达到for时长）时就已经恢复了。这属于短暂的“毛刺”，不需要通知用户。
直接从 previous_state_map 中删除这个指纹的条目即可。

流程总结与示例
假设一个告警规则是 CPU > 90% for 5m，评估周期为1分钟。

12:00: SQL查询返回 node-A。
判断: 新出现的告警。
状态: node-A -> {status: 'Pending', startTime: '12:00'}。

12:01 -> 12:04: SQL查询持续返回 node-A。
判断: 持续存在的告警，状态为 Pending。
动作: 检查计时器，发现 NOW - 12:00 < 5m。状态保持 Pending。

12:05: SQL查询依然返回 node-A。
判断: 持续存在的告警，状态为 Pending。
动作: 检查计时器，发现 12:05 - 12:00 >= 5m。

状态更新为 node-A -> {status: 'Firing', ...}。

发送 Firing 事件给告警管理器！

12:06: SQL查询结果为空，不再包含 node-A。
判断: 已恢复的告警，之前状态是 Firing。

动作:
发送 Resolved 事件给告警管理器！
从状态表中删除 node-A。
这个“查询-对比-更新”的循环，确保了告警引擎能够精确地跟踪每一个告警实例从诞生、触发到恢复的全过程，并通过 for 机制有效过滤了噪音。

2.5. 告警事件结构设计
告警规则引擎在告警实例状态变为Firing或Resolved时，会生成一个结构化的告警事件，发送给告警管理器。

事件JSON结构:
{
  "fingerprint": "alertname=HighDiskUsage,host_ip=192.168.1.100",
  "status": "firing", // "firing" 或 "resolved"
  "host_ip": "192.168.1.100",
  "stable": "disk",
  “metrics”: "usage_percent",
  "tags":[
    "mount_point": "/data"
  ]
  "value": 99.0, // 指标当前值
  "alertname": "HighDiskUsage",
  "for": "0s",
  "severity": "严重",
  "alert_type": "硬件资源",
  "summary": "磁盘使用率过高",
  "description": "节点 {{host_ip}} 的磁盘 {{mount_point}} 空间风险提示：使用率 {{usage_percent}}%。"
  "starts_at": "2025-07-15T15:50:00Z", // 首次满足条件的时间 (进入Pending状态的时间)
  "ends_at": null, // 告警恢复的时间 (仅在status为resolved时有值)
}

2.6. 告警管理器的工作逻辑设计

核心任务: 维护所有与通知行为相关的、更上层的聚合状态。
管理的状态包括:
分组 (Grouping): 哪些告警被分为一组。
静默 (Silencing): 用户定义的静默规则及其生效状态。
抑制 (Inhibition): 告警间的抑制关系。
通知计时 (Notification Timers): 包括初次发送延迟 (group_wait) 和重复发送间隔 (repeat_interval)。

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