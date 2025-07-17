#!/bin/bash

# 告警规则API测试脚本
# 测试创建、查询、更新、删除等所有操作

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
    local method="$1"
    local endpoint="$2"
    local data="$3"
    local description="$4"
    
    print_info "$description"
    
    if [ "$method" = "POST" ] || [ "$method" = "PUT" ]; then
        response=$(curl -s -X "$method" "${SERVER_URL}${endpoint}" \
            -H "Content-Type: application/json" \
            -d "$data" \
            -w "HTTPSTATUS:%{http_code}")
    else
        response=$(curl -s -X "$method" "${SERVER_URL}${endpoint}" \
            -w "HTTPSTATUS:%{http_code}")
    fi
    
    http_code=$(echo "$response" | grep -o "HTTPSTATUS:.*" | cut -d: -f2)
    body=$(echo "$response" | sed 's/HTTPSTATUS:.*//')
    
    echo "HTTP状态码: $http_code"
    echo "响应内容:"
    echo "$body" | jq . 2>/dev/null || echo "$body"
    echo
    
    return $http_code
}

# 主程序开始
print_header "告警规则API测试"

# 检查服务器连接
if ! check_server; then
    exit 1
fi

# 检查jq是否安装
if ! command -v jq &> /dev/null; then
    print_info "jq未安装，JSON输出将不会格式化"
fi

# 存储创建的规则ID
RULE_ID=""

# 1. 创建告警规则
print_header "1. 创建告警规则 (POST /alarm/rules)"

create_rule_data='{
  "alert_name": "TestApiRule",
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
  "summary": "API测试规则",
  "description": "这是一个通过API创建的测试规则，节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%。",
  "alert_type": "硬件资源"
}'

send_request "POST" "$API_ENDPOINT" "$create_rule_data" "创建告警规则"
if [ $? -eq 200 ]; then
    print_success "告警规则创建成功"
    # 提取规则ID
    RULE_ID=$(echo "$body" | jq -r '.id' 2>/dev/null)
    if [ "$RULE_ID" != "null" ] && [ -n "$RULE_ID" ]; then
        print_info "创建的规则ID: $RULE_ID"
    fi
else
    print_error "告警规则创建失败"
fi

# 2. 查询所有告警规则
print_header "2. 查询所有告警规则 (GET /alarm/rules)"

send_request "GET" "$API_ENDPOINT" "" "查询所有告警规则"
if [ $? -eq 200 ]; then
    print_success "成功获取所有告警规则"
    rule_count=$(echo "$body" | jq length 2>/dev/null)
    if [ "$rule_count" != "null" ] && [ -n "$rule_count" ]; then
        print_info "当前共有 $rule_count 个告警规则"
    fi
else
    print_error "查询所有告警规则失败"
fi

# 3. 查询单个告警规则
if [ -n "$RULE_ID" ]; then
    print_header "3. 查询单个告警规则 (GET /alarm/rules/{id})"
    
    send_request "GET" "$API_ENDPOINT/$RULE_ID" "" "查询单个告警规则"
    if [ $? -eq 200 ]; then
        print_success "成功获取单个告警规则"
        alert_name=$(echo "$body" | jq -r '.alert_name' 2>/dev/null)
        if [ "$alert_name" != "null" ] && [ -n "$alert_name" ]; then
            print_info "规则名称: $alert_name"
        fi
    else
        print_error "查询单个告警规则失败"
    fi
else
    print_info "跳过单个规则查询（无有效规则ID）"
fi

# 4. 更新告警规则
if [ -n "$RULE_ID" ]; then
    print_header "4. 更新告警规则 (POST /alarm/rules/{id}/update)"
    
    update_rule_data='{
      "alert_name": "TestApiRuleUpdated",
      "expression": {
        "stable": "cpu",
        "metric": "usage_percent",
        "conditions": [
          {
            "operator": ">",
            "threshold": 90.0
          }
        ]
      },
      "for": "10m",
      "severity": "一般",
      "summary": "API测试规则（已更新）",
      "description": "这是一个通过API更新的测试规则，节点 {{host_ip}} CPU使用率达到 {{usage_percent}}%。",
      "alert_type": "硬件资源",
      "enabled": true
    }'
    
    send_request "POST" "$API_ENDPOINT/$RULE_ID/update" "$update_rule_data" "更新告警规则"
    if [ $? -eq 200 ]; then
        print_success "告警规则更新成功"
    else
        print_error "告警规则更新失败"
    fi
else
    print_info "跳过规则更新（无有效规则ID）"
fi

# 5. 再次查询单个规则，验证更新
if [ -n "$RULE_ID" ]; then
    print_header "5. 验证更新结果 (GET /alarm/rules/{id})"
    
    send_request "GET" "$API_ENDPOINT/$RULE_ID" "" "验证更新后的规则"
    if [ $? -eq 200 ]; then
        print_success "成功获取更新后的规则"
        alert_name=$(echo "$body" | jq -r '.alert_name' 2>/dev/null)
        severity=$(echo "$body" | jq -r '.severity' 2>/dev/null)
        if [ "$alert_name" != "null" ] && [ -n "$alert_name" ]; then
            print_info "更新后的规则名称: $alert_name"
        fi
        if [ "$severity" != "null" ] && [ -n "$severity" ]; then
            print_info "更新后的严重等级: $severity"
        fi
    else
        print_error "验证更新失败"
    fi
else
    print_info "跳过更新验证（无有效规则ID）"
fi

# 6. 测试错误情况
print_header "6. 测试错误情况"

# 6.1 查询不存在的规则
print_info "测试查询不存在的规则..."
send_request "GET" "$API_ENDPOINT/nonexistent-id" "" "查询不存在的规则"
if [ $? -eq 404 ]; then
    print_success "正确返回404错误"
else
    print_error "未正确处理不存在的规则查询"
fi

# 6.2 创建无效的规则
print_info "测试创建无效规则..."
invalid_rule_data='{"invalid": "json"}'
send_request "POST" "$API_ENDPOINT" "$invalid_rule_data" "创建无效规则"
if [ $? -eq 400 ] || [ $? -eq 500 ]; then
    print_success "正确返回错误响应"
else
    print_error "未正确处理无效规则创建"
fi

# 7. 删除告警规则
if [ -n "$RULE_ID" ]; then
    print_header "7. 删除告警规则 (POST /alarm/rules/{id}/delete)"
    
    send_request "POST" "$API_ENDPOINT/$RULE_ID/delete" "" "删除告警规则"
    if [ $? -eq 200 ]; then
        print_success "告警规则删除成功"
    else
        print_error "告警规则删除失败"
    fi
    
    # 7.1 验证删除结果
    print_info "验证删除结果..."
    send_request "GET" "$API_ENDPOINT/$RULE_ID" "" "验证规则已删除"
    if [ $? -eq 404 ]; then
        print_success "规则已成功删除"
    else
        print_error "规则删除验证失败"
    fi
else
    print_info "跳过规则删除（无有效规则ID）"
fi

# 8. 最终状态检查
print_header "8. 最终状态检查"

send_request "GET" "$API_ENDPOINT" "" "获取最终规则列表"
if [ $? -eq 200 ]; then
    print_success "成功获取最终规则列表"
    final_count=$(echo "$body" | jq length 2>/dev/null)
    if [ "$final_count" != "null" ] && [ -n "$final_count" ]; then
        print_info "最终规则数量: $final_count"
    fi
else
    print_error "获取最终规则列表失败"
fi

print_header "API测试完成"
print_info "所有告警规则API接口测试完成！"