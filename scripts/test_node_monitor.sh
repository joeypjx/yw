#!/bin/bash

# 节点状态监控测试脚本

echo "🧪 开始测试节点状态监控功能..."

# 1. 启动系统
echo "📡 启动告警系统..."
./MyProject &
SYSTEM_PID=$!

# 等待系统启动
sleep 5

# 2. 模拟节点心跳
echo "💓 模拟节点心跳..."
curl -X POST http://localhost:8080/heartbeat \
  -H "Content-Type: application/json" \
  -d '{
    "api_version": "v1",
    "data": {
      "box_id": "test-node-001",
      "slot_id": "slot-1",
      "cpu_id": "cpu-001",
      "srio_id": "srio-001",
      "host_ip": "192.168.1.100",
      "hostname": "test-node-001",
      "service_port": 8080,
      "box_type": "compute",
      "board_type": "x86",
      "cpu_type": "Intel",
      "cpu_arch": "x86_64",
      "os_type": "Linux",
      "resource_type": "compute",
      "gpu": "NVIDIA RTX 4090"
    }
  }'

echo "✅ 节点心跳已发送"

# 3. 检查节点是否在线
echo "🔍 检查节点状态..."
sleep 2
curl -s http://localhost:8080/api/v1/nodes/list | jq '.'

# 4. 等待节点离线（超过5秒无心跳）
echo "⏰ 等待节点离线检测..."
sleep 7

# 5. 检查告警事件
echo "🚨 检查告警事件..."
curl -s http://localhost:8080/api/v1/alarm/events | jq '.'

# 6. 再次发送心跳（恢复在线）
echo "💓 重新发送心跳..."
curl -X POST http://localhost:8080/heartbeat \
  -H "Content-Type: application/json" \
  -d '{
    "api_version": "v1",
    "data": {
      "box_id": "test-node-001",
      "slot_id": "slot-1",
      "cpu_id": "cpu-001",
      "srio_id": "srio-001",
      "host_ip": "192.168.1.100",
      "hostname": "test-node-001",
      "service_port": 8080,
      "box_type": "compute",
      "board_type": "x86",
      "cpu_type": "Intel",
      "cpu_arch": "x86_64",
      "os_type": "Linux",
      "resource_type": "compute",
      "gpu": "NVIDIA RTX 4090"
    }
  }'

echo "✅ 节点心跳已重新发送"

# 7. 等待告警解决
echo "⏰ 等待告警解决..."
sleep 3

# 8. 再次检查告警事件
echo "🚨 检查告警事件（应该已解决）..."
curl -s http://localhost:8080/api/v1/alarm/events | jq '.'

# 9. 停止系统
echo "🛑 停止告警系统..."
kill $SYSTEM_PID
wait $SYSTEM_PID

echo "✅ 节点状态监控测试完成！" 