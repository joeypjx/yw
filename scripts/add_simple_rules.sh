#!/bin/bash

# 简化版告警规则添加脚本
# 用于快速添加基础的测试告警规则

SERVER_URL="http://localhost:8080"
API_ENDPOINT="/alarm/rules"

echo "🚀 开始添加基础告警规则..."

# 1. CPU使用率告警
echo "📊 添加CPU使用率告警..."
curl -X POST "${SERVER_URL}${API_ENDPOINT}" \
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
echo -e "\n"

# 2. 内存使用率告警
echo "🧠 添加内存使用率告警..."
curl -X POST "${SERVER_URL}${API_ENDPOINT}" \
  -H "Content-Type: application/json" \
  -d '{
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
    "description": "节点 {{host_ip}} 内存使用率达到 {{usage_percent}}%。",
    "alert_type": "硬件资源"
  }'
echo -e "\n"

# 3. 磁盘空间告警
echo "💾 添加磁盘空间告警..."
curl -X POST "${SERVER_URL}${API_ENDPOINT}" \
  -H "Content-Type: application/json" \
  -d '{
    "alert_name": "HighDiskUsage",
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
    "for": "2m",
    "severity": "一般",
    "summary": "磁盘使用率过高",
    "description": "节点 {{host_ip}} 磁盘 {{device}} 使用率达到 {{usage_percent}}%。",
    "alert_type": "硬件资源"
  }'
echo -e "\n"

# 4. 数据分区特定告警
echo "📁 添加数据分区告警..."
curl -X POST "${SERVER_URL}${API_ENDPOINT}" \
  -H "Content-Type: application/json" \
  -d '{
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
          "threshold": 70.0
        }
      ]
    },
    "for": "1m",
    "severity": "严重",
    "summary": "数据分区空间不足",
    "description": "节点 {{host_ip}} 数据分区 {{mount_point}} 使用率达到 {{usage_percent}}%。",
    "alert_type": "硬件资源"
  }'
echo -e "\n"

# 5. GPU使用率告警
echo "🖥️  添加GPU使用率告警..."
curl -X POST "${SERVER_URL}${API_ENDPOINT}" \
  -H "Content-Type: application/json" \
  -d '{
    "alert_name": "HighGpuUsage",
    "expression": {
      "stable": "gpu",
      "metric": "compute_usage",
      "conditions": [
        {
          "operator": ">",
          "threshold": 80.0
        }
      ]
    },
    "for": "5m",
    "severity": "严重",
    "summary": "GPU使用率过高",
    "description": "节点 {{host_ip}} GPU {{gpu_name}} 计算使用率达到 {{compute_usage}}%。",
    "alert_type": "硬件资源"
  }'
echo -e "\n"

echo "✅ 基础告警规则添加完成！"