#!/bin/bash

# 增加告警规则脚本
# 用于添加各种类型的测试用告警规则

# 服务器配置
SERVER_URL="http://localhost:8080"
API_ENDPOINT="/alarm/rules"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印函数
print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

# 发送告警规则的函数
send_alarm_rule() {
    local rule_name="$1"
    local json_data="$2"
    
    print_info "正在添加告警规则: $rule_name"
    
    response=$(curl -s -X POST "${SERVER_URL}${API_ENDPOINT}" \
        -H "Content-Type: application/json" \
        -d "$json_data" \
        -w "HTTPSTATUS:%{http_code}")
    
    http_code=$(echo "$response" | grep -o "HTTPSTATUS:.*" | cut -d: -f2)
    body=$(echo "$response" | sed 's/HTTPSTATUS:.*//')
    
    if [ "$http_code" -eq 200 ]; then
        rule_id=$(echo "$body" | grep -o '"id":"[^"]*' | cut -d'"' -f4)
        print_success "$rule_name 添加成功 (ID: $rule_id)"
        return 0
    else
        print_error "$rule_name 添加失败 (HTTP $http_code)"
        echo "响应: $body"
        return 1
    fi
}

# 检查服务器连接
check_server() {
    print_info "检查服务器连接..."
    
    if curl -s --connect-timeout 5 "${SERVER_URL}" > /dev/null 2>&1; then
        print_success "服务器连接正常"
        return 0
    else
        print_error "无法连接到服务器: $SERVER_URL"
        print_warning "请确保服务器正在运行"
        return 1
    fi
}

# 主程序开始
print_header "告警规则批量添加脚本"

# 检查服务器连接
if ! check_server; then
    exit 1
fi

print_info "开始添加告警规则..."

# 规则计数器
total_rules=0
success_rules=0

# 1. CPU使用率告警
print_header "1. CPU相关告警规则"

# 1.1 高CPU使用率告警
((total_rules++))
cpu_rule_high='{
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
  "description": "节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%，请及时处理。",
  "alert_type": "硬件资源"
}'

if send_alarm_rule "高CPU使用率告警" "$cpu_rule_high"; then
    ((success_rules++))
fi

# 1.2 CPU负载告警
((total_rules++))
cpu_rule_load='{
  "alert_name": "HighCpuLoad",
  "expression": {
    "stable": "cpu",
    "metric": "load_avg_5m",
    "conditions": [
      {
        "operator": ">",
        "threshold": 4.0
      }
    ]
  },
  "for": "3m",
  "severity": "一般",
  "summary": "CPU负载过高",
  "description": "节点 {{host_ip}} 5分钟平均负载达到 {{load_avg_5m}}，超过正常范围。",
  "alert_type": "硬件资源"
}'

if send_alarm_rule "CPU负载告警" "$cpu_rule_load"; then
    ((success_rules++))
fi

# 2. 内存相关告警
print_header "2. 内存相关告警规则"

# 2.1 高内存使用率告警
((total_rules++))
memory_rule_high='{
  "alert_name": "HighMemoryUsage",
  "expression": {
    "stable": "memory",
    "metric": "usage_percent",
    "conditions": [
      {
        "operator": ">",
        "threshold": 85.0
      }
    ]
  },
  "for": "3m",
  "severity": "严重",
  "summary": "内存使用率过高",
  "description": "节点 {{host_ip}} 内存使用率达到 {{usage_percent}}%，可能影响系统性能。",
  "alert_type": "硬件资源"
}'

if send_alarm_rule "高内存使用率告警" "$memory_rule_high"; then
    ((success_rules++))
fi

# 2.2 内存使用率警告
((total_rules++))
memory_rule_warning='{
  "alert_name": "MemoryUsageWarning",
  "expression": {
    "stable": "memory",
    "metric": "usage_percent",
    "conditions": [
      {
        "operator": ">",
        "threshold": 70.0
      },
      {
        "operator": "<=",
        "threshold": 85.0
      }
    ]
  },
  "for": "5m",
  "severity": "提示",
  "summary": "内存使用率警告",
  "description": "节点 {{host_ip}} 内存使用率为 {{usage_percent}}%，建议关注。",
  "alert_type": "硬件资源"
}'

if send_alarm_rule "内存使用率警告" "$memory_rule_warning"; then
    ((success_rules++))
fi

# 3. 磁盘相关告警
print_header "3. 磁盘相关告警规则"

# 3.1 根分区磁盘空间告警
((total_rules++))
disk_rule_root='{
  "alert_name": "RootDiskSpaceHigh",
  "expression": {
    "stable": "disk",
    "tags": [
      {
        "mount_point": "/"
      }
    ],
    "metric": "usage_percent",
    "conditions": [
      {
        "operator": ">",
        "threshold": 80.0
      }
    ]
  },
  "for": "1m",
  "severity": "严重",
  "summary": "根分区磁盘空间不足",
  "description": "节点 {{host_ip}} 根分区 {{mount_point}} 使用率达到 {{usage_percent}}%，空间不足。",
  "alert_type": "硬件资源"
}'

if send_alarm_rule "根分区磁盘空间告警" "$disk_rule_root"; then
    ((success_rules++))
fi

# 3.2 数据分区磁盘空间告警
((total_rules++))
disk_rule_data='{
  "alert_name": "DataDiskSpaceHigh",
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
  "summary": "数据分区磁盘空间严重不足",
  "description": "节点 {{host_ip}} 数据分区 {{mount_point}} 使用率达到 {{usage_percent}}%，请立即清理。",
  "alert_type": "硬件资源"
}'

if send_alarm_rule "数据分区磁盘空间告警" "$disk_rule_data"; then
    ((success_rules++))
fi

# 3.3 通用磁盘空间预警
((total_rules++))
disk_rule_general='{
  "alert_name": "GeneralDiskSpaceWarning",
  "expression": {
    "stable": "disk",
    "metric": "usage_percent",
    "conditions": [
      {
        "operator": ">",
        "threshold": 75.0
      }
    ]
  },
  "for": "5m",
  "severity": "一般",
  "summary": "磁盘空间预警",
  "description": "节点 {{host_ip}} 磁盘 {{device}} ({{mount_point}}) 使用率达到 {{usage_percent}}%。",
  "alert_type": "硬件资源"
}'

if send_alarm_rule "通用磁盘空间预警" "$disk_rule_general"; then
    ((success_rules++))
fi

# 4. 网络相关告警
print_header "4. 网络相关告警规则"

# 4.1 网络接口错误告警
((total_rules++))
network_rule_errors='{
  "alert_name": "NetworkInterfaceErrors",
  "expression": {
    "stable": "network",
    "metric": "rx_errors",
    "conditions": [
      {
        "operator": ">",
        "threshold": 100
      }
    ]
  },
  "for": "2m",
  "severity": "一般",
  "summary": "网络接口接收错误",
  "description": "节点 {{host_ip}} 网络接口 {{interface}} 接收错误数达到 {{rx_errors}}。",
  "alert_type": "硬件资源"
}'

if send_alarm_rule "网络接口错误告警" "$network_rule_errors"; then
    ((success_rules++))
fi

# 4.2 特定网络接口告警
((total_rules++))
network_rule_bond='{
  "alert_name": "BondInterfaceErrors",
  "expression": {
    "stable": "network",
    "tags": [
      {
        "interface": "bond0"
      }
    ],
    "metric": "tx_errors",
    "conditions": [
      {
        "operator": ">",
        "threshold": 50
      }
    ]
  },
  "for": "1m",
  "severity": "严重",
  "summary": "Bond接口发送错误",
  "description": "节点 {{host_ip}} Bond接口 {{interface}} 发送错误数达到 {{tx_errors}}，可能影响网络稳定性。",
  "alert_type": "硬件资源"
}'

if send_alarm_rule "Bond接口错误告警" "$network_rule_bond"; then
    ((success_rules++))
fi

# 5. GPU相关告警
print_header "5. GPU相关告警规则"

# 5.1 GPU计算使用率告警
((total_rules++))
gpu_rule_compute='{
  "alert_name": "HighGpuComputeUsage",
  "expression": {
    "stable": "gpu",
    "metric": "compute_usage",
    "conditions": [
      {
        "operator": ">",
        "threshold": 90.0
      }
    ]
  },
  "for": "5m",
  "severity": "严重",
  "summary": "GPU计算使用率过高",
  "description": "节点 {{host_ip}} GPU {{gpu_name}} 计算使用率达到 {{compute_usage}}%。",
  "alert_type": "硬件资源"
}'

if send_alarm_rule "GPU计算使用率告警" "$gpu_rule_compute"; then
    ((success_rules++))
fi

# 5.2 GPU显存使用率告警
((total_rules++))
gpu_rule_memory='{
  "alert_name": "HighGpuMemoryUsage",
  "expression": {
    "stable": "gpu",
    "metric": "mem_usage",
    "conditions": [
      {
        "operator": ">",
        "threshold": 85.0
      }
    ]
  },
  "for": "3m",
  "severity": "一般",
  "summary": "GPU显存使用率过高",
  "description": "节点 {{host_ip}} GPU {{gpu_name}} 显存使用率达到 {{mem_usage}}%。",
  "alert_type": "硬件资源"
}'

if send_alarm_rule "GPU显存使用率告警" "$gpu_rule_memory"; then
    ((success_rules++))
fi

# 5.3 GPU温度告警
((total_rules++))
gpu_rule_temperature='{
  "alert_name": "HighGpuTemperature",
  "expression": {
    "stable": "gpu",
    "metric": "temperature",
    "conditions": [
      {
        "operator": ">",
        "threshold": 80.0
      }
    ]
  },
  "for": "2m",
  "severity": "严重",
  "summary": "GPU温度过高",
  "description": "节点 {{host_ip}} GPU {{gpu_name}} 温度达到 {{temperature}}°C，可能影响硬件寿命。",
  "alert_type": "硬件资源"
}'

if send_alarm_rule "GPU温度告警" "$gpu_rule_temperature"; then
    ((success_rules++))
fi

# 6. 复合条件告警
print_header "6. 复合条件告警规则"

# 6.1 磁盘空间区间告警
((total_rules++))
disk_rule_range='{
  "alert_name": "DiskSpaceRangeWarning",
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
        "threshold": 60.0
      },
      {
        "operator": "<=",
        "threshold": 90.0
      }
    ]
  },
  "for": "10m",
  "severity": "提示",
  "summary": "数据分区空间提醒",
  "description": "节点 {{host_ip}} 数据分区 {{mount_point}} 使用率为 {{usage_percent}}%，建议定期清理。",
  "alert_type": "硬件资源"
}'

if send_alarm_rule "磁盘空间区间告警" "$disk_rule_range"; then
    ((success_rules++))
fi

# 7. 业务相关告警
print_header "7. 业务相关告警规则"

# 7.1 系统负载综合告警
((total_rules++))
system_rule_load='{
  "alert_name": "SystemHighLoad",
  "expression": {
    "stable": "cpu",
    "metric": "load_avg_15m",
    "conditions": [
      {
        "operator": ">",
        "threshold": 8.0
      }
    ]
  },
  "for": "10m",
  "severity": "严重",
  "summary": "系统负载过高",
  "description": "节点 {{host_ip}} 15分钟平均负载达到 {{load_avg_15m}}，系统可能过载。",
  "alert_type": "系统故障"
}'

if send_alarm_rule "系统负载综合告警" "$system_rule_load"; then
    ((success_rules++))
fi

# 统计结果
print_header "添加结果统计"

echo -e "${BLUE}总共尝试添加: $total_rules 条告警规则${NC}"
echo -e "${GREEN}成功添加: $success_rules 条告警规则${NC}"
echo -e "${RED}失败数量: $((total_rules - success_rules)) 条告警规则${NC}"

if [ $success_rules -eq $total_rules ]; then
    print_success "所有告警规则添加成功！"
    exit 0
else
    print_warning "部分告警规则添加失败，请检查日志。"
    exit 1
fi