#!/bin/bash

# 告警事件API测试脚本
# 测试获取告警事件的各种查询模式

SERVER_URL="http://localhost:8080"
EVENTS_ENDPOINT="/alarm/events"

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

print_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

# 检查服务器连接
check_server() {
    print_info "检查服务器连接..."
    if curl -s --connect-timeout 5 "${SERVER_URL}" > /dev/null 2>&1; then
        print_success "服务器连接正常"
        return 0
    else
        print_error "无法连接到服务器: $SERVER_URL"
        return 1
    fi
}

# 发送请求并处理响应
send_request() {
    local endpoint="$1"
    local description="$2"
    
    print_info "$description"
    
    response=$(curl -s -X GET "${SERVER_URL}${endpoint}" \
        -w "HTTPSTATUS:%{http_code}")
    
    http_code=$(echo "$response" | grep -o "HTTPSTATUS:.*" | cut -d: -f2)
    body=$(echo "$response" | sed 's/HTTPSTATUS:.*//')
    
    echo "HTTP状态码: $http_code"
    echo "响应内容:"
    echo "$body" | jq . 2>/dev/null || echo "$body"
    echo
    
    return $http_code
}

# 主程序开始
print_header "告警事件API测试"

# 检查服务器连接
if ! check_server; then
    exit 1
fi

# 检查jq是否安装
if ! command -v jq &> /dev/null; then
    print_info "jq未安装，JSON输出将不会格式化"
fi

# 1. 获取所有告警事件（默认最近100条）
print_header "1. 获取所有告警事件 (GET /alarm/events)"

send_request "$EVENTS_ENDPOINT" "获取所有告警事件"
if [ $? -eq 200 ]; then
    print_success "成功获取告警事件"
    event_count=$(echo "$body" | jq length 2>/dev/null)
    if [ "$event_count" != "null" ] && [ -n "$event_count" ]; then
        print_info "当前共有 $event_count 个告警事件"
    fi
else
    print_error "获取告警事件失败"
fi

# 2. 获取活跃的告警事件
print_header "2. 获取活跃的告警事件 (GET /alarm/events?status=active)"

send_request "$EVENTS_ENDPOINT?status=active" "获取活跃的告警事件"
if [ $? -eq 200 ]; then
    print_success "成功获取活跃告警事件"
    active_count=$(echo "$body" | jq length 2>/dev/null)
    if [ "$active_count" != "null" ] && [ -n "$active_count" ]; then
        print_info "当前共有 $active_count 个活跃告警事件"
    fi
else
    print_error "获取活跃告警事件失败"
fi

# 3. 获取firing状态的告警事件
print_header "3. 获取firing状态的告警事件 (GET /alarm/events?status=firing)"

send_request "$EVENTS_ENDPOINT?status=firing" "获取firing状态的告警事件"
if [ $? -eq 200 ]; then
    print_success "成功获取firing状态告警事件"
    firing_count=$(echo "$body" | jq length 2>/dev/null)
    if [ "$firing_count" != "null" ] && [ -n "$firing_count" ]; then
        print_info "当前共有 $firing_count 个firing状态告警事件"
    fi
else
    print_error "获取firing状态告警事件失败"
fi

# 4. 获取最近10条告警事件
print_header "4. 获取最近10条告警事件 (GET /alarm/events?limit=10)"

send_request "$EVENTS_ENDPOINT?limit=10" "获取最近10条告警事件"
if [ $? -eq 200 ]; then
    print_success "成功获取最近10条告警事件"
    limited_count=$(echo "$body" | jq length 2>/dev/null)
    if [ "$limited_count" != "null" ] && [ -n "$limited_count" ]; then
        print_info "返回了 $limited_count 个告警事件"
    fi
else
    print_error "获取最近10条告警事件失败"
fi

# 5. 获取最近5条告警事件
print_header "5. 获取最近5条告警事件 (GET /alarm/events?limit=5)"

send_request "$EVENTS_ENDPOINT?limit=5" "获取最近5条告警事件"
if [ $? -eq 200 ]; then
    print_success "成功获取最近5条告警事件"
    limited_count=$(echo "$body" | jq length 2>/dev/null)
    if [ "$limited_count" != "null" ] && [ -n "$limited_count" ]; then
        print_info "返回了 $limited_count 个告警事件"
    fi
else
    print_error "获取最近5条告警事件失败"
fi

# 6. 测试错误情况
print_header "6. 测试错误情况"

# 6.1 无效的limit值
print_info "测试无效的limit值..."
send_request "$EVENTS_ENDPOINT?limit=invalid" "测试无效的limit值"
if [ $? -eq 400 ] || [ $? -eq 500 ]; then
    print_success "正确处理无效的limit值"
else
    print_info "服务器接受了无效的limit值（可能有默认处理）"
fi

# 6.2 无效的status值
print_info "测试无效的status值..."
send_request "$EVENTS_ENDPOINT?status=invalid_status" "测试无效的status值"
if [ $? -eq 200 ]; then
    print_info "无效status值被当作默认查询处理"
else
    print_error "无效status值导致错误"
fi

# 7. 测试组合参数
print_header "7. 测试组合参数"

# 7.1 同时指定status和limit（status优先）
print_info "测试同时指定status=active和limit=20..."
send_request "$EVENTS_ENDPOINT?status=active&limit=20" "测试status和limit组合"
if [ $? -eq 200 ]; then
    print_success "成功处理组合参数"
    combo_count=$(echo "$body" | jq length 2>/dev/null)
    if [ "$combo_count" != "null" ] && [ -n "$combo_count" ]; then
        print_info "返回了 $combo_count 个告警事件"
    fi
else
    print_error "组合参数处理失败"
fi

print_header "告警事件API测试完成"
print_info "所有告警事件API接口测试完成！"