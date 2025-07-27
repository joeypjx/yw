#!/bin/bash

# Test script for /heart API endpoint
# Simulates node heartbeat every 5 seconds with data from @docs/node.json

# Server configuration
SERVER_URL="http://localhost:8080"
HEART_ENDPOINT="$SERVER_URL/heartbeat"

# JSON data based on @docs/node.json
HEART_DATA='{
    "api_version": 1,
    "data": {
        "box_id": 1,
        "slot_id": 1,
        "cpu_id": 1,
        "srio_id": 0,
        "host_ip": "192.168.1.100",
        "hostname": "localhost.localdomain",
        "service_port": 23980,
        "box_type": "计算I型",
        "board_type": "GPU",
        "cpu_type": "Phytium,D2000/8",
        "os_type": "Kylin Linux Advanced Server V10",
        "resource_type": "GPU I",
        "cpu_arch": "aarch64",
        "gpu": [
            {
                "index": 0,
                "name": "NVIDIA Tesla V100"
            },
            {
                "index": 1,
                "name": "NVIDIA Tesla V100"
            }
        ]
    }
}'

echo "Starting heart API test script..."
echo "Server URL: $SERVER_URL"
echo "Heart endpoint: $HEART_ENDPOINT"
echo "Sending heartbeat every 5 seconds..."
echo "Press Ctrl+C to stop"
echo ""

# Counter for heartbeat messages
counter=1

# Function to handle cleanup on exit
cleanup() {
    echo ""
    echo "Test script stopped. Total heartbeats sent: $((counter-1))"
    exit 0
}

# Set up signal handler for graceful exit
trap cleanup SIGINT SIGTERM

# Main loop - send heartbeat every 5 seconds
while true; do
    echo "[$counter] $(date '+%Y-%m-%d %H:%M:%S') - Sending heartbeat..."
    
    # Send POST request to /heart endpoint
    response=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X POST \
        -H "Content-Type: application/json" \
        -d "$HEART_DATA" \
        "$HEART_ENDPOINT" 2>/dev/null)
    
    # Extract HTTP status code and response body
    http_code=$(echo "$response" | tail -n1 | sed 's/.*HTTP_CODE://')
    response_body=$(echo "$response" | sed '$d')
    
    # Check if request was successful
    if [ "$http_code" = "200" ]; then
        echo "  ✓ SUCCESS: $response_body"
    else
        echo "  ✗ FAILED (HTTP $http_code): $response_body"
    fi
    
    counter=$((counter + 1))
    
    # Wait 5 seconds before next heartbeat
    sleep 5
done