#!/bin/bash

# Test script for /resource API endpoint
# Periodically sends resource status data with random values
# Includes periods of high resource usage to trigger alarms

# Server configuration
SERVER_URL="http://localhost:8080"
RESOURCE_ENDPOINT="$SERVER_URL/resource"

# Test configuration
NORMAL_INTERVAL=10    # Normal sending interval in seconds
HIGH_LOAD_INTERVAL=5  # High load period interval in seconds
HIGH_LOAD_DURATION=60 # Duration of high load period in seconds
CYCLE_DURATION=300    # Total cycle duration in seconds (5 minutes)

# Node configuration
HOST_IP="192.168.1.100"
HOSTNAME="test-node-$(date +%s)"

# Stable component identifiers for consistent monitoring across requests
INSTANCE_BASE="test-$(date +%s)"
COMP1_INSTANCE_ID="${INSTANCE_BASE}-1"
COMP2_INSTANCE_ID="${INSTANCE_BASE}-2"
UUID_BASE="uuid-$(date +%s)"
COMP1_UUID="${UUID_BASE}-1"
COMP2_UUID="${UUID_BASE}-2"

echo "========================================================"
echo "Resource API Test Script"
echo "========================================================"
echo "Server URL: $SERVER_URL"
echo "Resource endpoint: $RESOURCE_ENDPOINT"
echo "Host IP: $HOST_IP"
echo "Normal interval: ${NORMAL_INTERVAL}s"
echo "High load interval: ${HIGH_LOAD_INTERVAL}s"
echo "High load duration: ${HIGH_LOAD_DURATION}s"
echo "Cycle duration: ${CYCLE_DURATION}s"
echo "Press Ctrl+C to stop"
echo ""

# Counter for messages
counter=1
start_time=$(date +%s)

# Function to generate random value in range
random_range() {
    local min=$1
    local max=$2
    local scale=${3:-0}
    
    if [ $scale -eq 0 ]; then
        echo $((min + RANDOM % (max - min + 1)))
    else
        # For floating point numbers
        python3 -c "import random; print(round(random.uniform($min, $max), $scale))" 2>/dev/null || echo "$min"
    fi
}

# Function to generate random bytes value
random_bytes() {
    local min_gb=$1
    local max_gb=$2
    local min_bytes=$((min_gb * 1024 * 1024 * 1024))
    local max_bytes=$((max_gb * 1024 * 1024 * 1024))
    echo $((min_bytes + RANDOM % (max_bytes - min_bytes + 1)))
}

# Function to determine if we're in high load period
is_high_load_period() {
    local current_time=$(date +%s)
    local elapsed=$((current_time - start_time))
    local cycle_pos=$((elapsed % CYCLE_DURATION))
    
    # High load period starts at 60s and lasts for HIGH_LOAD_DURATION
    if [ $cycle_pos -ge 60 ] && [ $cycle_pos -lt $((60 + HIGH_LOAD_DURATION)) ]; then
        return 0  # true
    else
        return 1  # false
    fi
}

# Function to determine if component 1 is in a failure window
# Simulate a sustained FAILED period (e.g., from 120s to 200s in each cycle)
is_component_failure_period() {
    local current_time=$(date +%s)
    local elapsed=$((current_time - start_time))
    local cycle_pos=$((elapsed % CYCLE_DURATION))
    if [ $cycle_pos -ge 120 ] && [ $cycle_pos -lt 200 ]; then
        return 0  # true -> FAILED
    else
        return 1  # false -> non-FAILED
    fi
}

# Function to generate CPU data
generate_cpu_data() {
    local high_load=$1
    
    if [ "$high_load" = "true" ]; then
        # High load: 80-95% usage
        usage=$(random_range 80 95 1)
        load_1m=$(random_range 8 12 1)
        load_5m=$(random_range 7 10 1)
        load_15m=$(random_range 6 9 1)
        temp=$(random_range 70 85 1)
        power=$(random_range 90 120 1)
    else
        # Normal load: 10-40% usage
        usage=$(random_range 10 40 1)
        load_1m=$(random_range 1 3 1)
        load_5m=$(random_range 1 2 1)
        load_15m=$(random_range 1 2 1)
        temp=$(random_range 45 65 1)
        power=$(random_range 45 80 1)
    fi
    
    core_count=$(random_range 8 16)
    core_allocated=$(random_range 2 $((core_count / 2)))
    voltage=$(random_range 11 13 1)
    current=$(random_range 2 4 1)
    
    cat <<EOF
        "cpu": {
            "usage_percent": $usage,
            "load_avg_1m": $load_1m,
            "load_avg_5m": $load_5m,
            "load_avg_15m": $load_15m,
            "core_count": $core_count,
            "core_allocated": $core_allocated,
            "temperature": $temp,
            "power": $power,
            "voltage": $voltage,
            "current": $current
        }
EOF
}

# Function to generate memory data
generate_memory_data() {
    local high_load=$1
    total=$(random_bytes 16 32)
    
    if [ "$high_load" = "true" ]; then
        # High load: 85-95% usage
        usage_percent=$(random_range 85 95)
    else
        # Normal load: 30-60% usage
        usage_percent=$(random_range 30 60)
    fi
    
    used=$((total * usage_percent / 100))
    free=$((total - used))
    
    cat <<EOF
        "memory": {
            "total": $total,
            "used": $used,
            "free": $free,
            "usage_percent": $usage_percent
        }
EOF
}

# Function to generate disk data
generate_disk_data() {
    local high_load=$1
    total=$(random_bytes 500 1000)
    
    if [ "$high_load" = "true" ]; then
        # High load: 85-95% usage
        usage_percent=$(random_range 85 95)
    else
        # Normal load: 40-70% usage
        usage_percent=$(random_range 40 70)
    fi
    
    used=$((total * usage_percent / 100))
    free=$((total - used))
    
    # Second disk values
    total2=$((total / 2))
    used2=$((used / 3))
    free2=$((total2 - used2))
    usage_percent2=$((usage_percent / 2))
    
    cat <<EOF
        "disk": [
            {
                "device": "/dev/sda1",
                "mount_point": "/",
                "total": $total,
                "used": $used,
                "free": $free,
                "usage_percent": $usage_percent
            },
            {
                "device": "/dev/sdb1",
                "mount_point": "/data",
                "total": $total2,
                "used": $used2,
                "free": $free2,
                "usage_percent": $usage_percent2
            }
        ]
EOF
}

# Function to generate network data
generate_network_data() {
    local high_load=$1
    
    if [ "$high_load" = "true" ]; then
        # High load: high traffic rates
        rx_rate=$(random_range 80000000 120000000)  # 80-120 MB/s
        tx_rate=$(random_range 60000000 100000000)  # 60-100 MB/s
    else
        # Normal load: moderate traffic rates
        rx_rate=$(random_range 1000000 10000000)    # 1-10 MB/s
        tx_rate=$(random_range 800000 8000000)      # 0.8-8 MB/s
    fi
    
    rx_bytes=$(random_range 1000000000 5000000000)  # 1-5 GB total
    tx_bytes=$(random_range 800000000 4000000000)   # 0.8-4 GB total
    rx_packets=$(random_range 1000000 5000000)
    tx_packets=$(random_range 800000 4000000)
    rx_errors=$(random_range 0 10)
    tx_errors=$(random_range 0 8)
    
    # Second interface values
    rx_bytes2=$((rx_bytes / 3))
    tx_bytes2=$((tx_bytes / 3))
    rx_packets2=$((rx_packets / 3))
    tx_packets2=$((tx_packets / 3))
    rx_rate2=$((rx_rate / 4))
    tx_rate2=$((tx_rate / 4))
    
    cat <<EOF
        "network": [
            {
                "interface": "eth0",
                "rx_bytes": $rx_bytes,
                "tx_bytes": $tx_bytes,
                "rx_packets": $rx_packets,
                "tx_packets": $tx_packets,
                "rx_errors": $rx_errors,
                "tx_errors": $tx_errors,
                "rx_rate": $rx_rate,
                "tx_rate": $tx_rate
            },
            {
                "interface": "eth1",
                "rx_bytes": $rx_bytes2,
                "tx_bytes": $tx_bytes2,
                "rx_packets": $rx_packets2,
                "tx_packets": $tx_packets2,
                "rx_errors": 0,
                "tx_errors": 0,
                "rx_rate": $rx_rate2,
                "tx_rate": $tx_rate2
            }
        ]
EOF
}

# Function to generate GPU data
generate_gpu_data() {
    local high_load=$1
    mem_total=$(random_bytes 24 32)  # 24-32 GB VRAM
    
    if [ "$high_load" = "true" ]; then
        # High load: 85-98% usage
        compute_usage=$(random_range 85 98)
        mem_usage=$(random_range 80 95)
        temperature=$(random_range 75 85)
        power=$(random_range 300 400)
    else
        # Normal load: 10-50% usage
        compute_usage=$(random_range 10 50)
        mem_usage=$(random_range 15 60)
        temperature=$(random_range 40 65)
        power=$(random_range 80 200)
    fi
    
    mem_used=$((mem_total * mem_usage / 100))
    
    # Second GPU values
    mem_total2=$((mem_total * 3 / 4))
    mem_used2=$((mem_used * 3 / 4))
    if [ $compute_usage -gt 10 ]; then
        compute_usage2=$((compute_usage - 10))
    else
        compute_usage2=$compute_usage
    fi
    if [ $mem_usage -gt 5 ]; then
        mem_usage2=$((mem_usage - 5))
    else
        mem_usage2=$mem_usage
    fi
    if [ $temperature -gt 5 ]; then
        temperature2=$((temperature - 5))
    else
        temperature2=$temperature
    fi
    if [ $power -gt 50 ]; then
        power2=$((power - 50))
    else
        power2=$power
    fi
    
    # GPU allocation info
    gpu_num=2
    gpu_allocated=$(random_range 0 2)
    
    cat <<EOF
        "gpu_allocated": $gpu_allocated,
        "gpu_num": $gpu_num,
        "gpu": [
            {
                "index": 0,
                "name": "NVIDIA RTX 4090",
                "compute_usage": $compute_usage,
                "mem_total": $mem_total,
                "mem_used": $mem_used,
                "mem_usage": $mem_usage,
                "temperature": $temperature,
                "power": $power
            },
            {
                "index": 1,
                "name": "NVIDIA RTX 4080",
                "compute_usage": $compute_usage2,
                "mem_total": $mem_total2,
                "mem_used": $mem_used2,
                "mem_usage": $mem_usage2,
                "temperature": $temperature2,
                "power": $power2
            }
        ]
EOF
}

# Function to generate component/container data
generate_component_data() {
    local high_load=$1
    mem_limit=$(random_bytes 4 8)
    
    if [ "$high_load" = "true" ]; then
        cpu_load=$(random_range 70 95)
        mem_usage_factor=$(random_range 80 95)
    else
        cpu_load=$(random_range 5 30)
        mem_usage_factor=$(random_range 20 50)
    fi
    
    mem_used=$((mem_limit * mem_usage_factor / 100))
    # Use stable IDs for consistent component tracking
    instance_id="$COMP1_INSTANCE_ID"
    uuid="$COMP1_UUID"
    container_id="$(date +%s | md5sum | cut -c1-32)"
    
    # Second container values
    cpu_load2=$((cpu_load / 2))
    mem_used2=$((mem_used / 3))
    mem_limit2=$((mem_limit / 2))
    instance_id2="$COMP2_INSTANCE_ID"
    uuid2="$COMP2_UUID"
    container_id2="$(echo "${container_id}2" | md5sum | cut -c1-32)"

    # Determine component states with simulated failure window for component 1
    state1="RUNNING"
    if is_component_failure_period; then
        state1="FAILED"
    fi
    state2="RUNNING"
    
    cat <<EOF
        "component": [
            {
                "instance_id": "$instance_id",
                "uuid": "$uuid",
                "index": 1,
                "config": {
                    "name": "nginx:latest",
                    "id": "$container_id"
                },
                "state": "$state1",
                "resource": {
                    "cpu": {
                        "load": $cpu_load
                    },
                    "memory": {
                        "mem_used": $mem_used,
                        "mem_limit": $mem_limit
                    },
                    "network": {
                        "tx": $(random_range 10000 100000),
                        "rx": $(random_range 20000 200000)
                    }
                }
            },
            {
                "instance_id": "$instance_id2",
                "uuid": "$uuid2",
                "index": 2,
                "config": {
                    "name": "redis:alpine",
                    "id": "$container_id2"
                },
                "state": "$state2",
                "resource": {
                    "cpu": {
                        "load": $cpu_load2
                    },
                    "memory": {
                        "mem_used": $mem_used2,
                        "mem_limit": $mem_limit2
                    },
                    "network": {
                        "tx": $(random_range 1000 10000),
                        "rx": $(random_range 2000 20000)
                    }
                }
            }
        ]
EOF
}

# Function to generate complete resource data
generate_resource_data() {
    local high_load=$1
    
    cat <<EOF
{
    "data": {
        "host_ip": "$HOST_IP",
        "resource": {
$(generate_cpu_data $high_load),
$(generate_memory_data $high_load),
$(generate_disk_data $high_load),
$(generate_network_data $high_load),
$(generate_gpu_data $high_load)
        },
$(generate_component_data $high_load)
    }
}
EOF
}

# Function to handle cleanup on exit
cleanup() {
    echo ""
    echo "========================================================"
    echo "Test script stopped."
    echo "Total resource updates sent: $((counter-1))"
    echo "========================================================"
    exit 0
}

# Set up signal handler for graceful exit
trap cleanup SIGINT SIGTERM

# Main loop
while true; do
    current_time=$(date +%s)
    elapsed=$((current_time - start_time))
    cycle_pos=$((elapsed % CYCLE_DURATION))
    
    # Determine if we're in high load period
    if is_high_load_period; then
        high_load="true"
        interval=$HIGH_LOAD_INTERVAL
        load_indicator="🔥 HIGH LOAD"
    else
        high_load="false"
        interval=$NORMAL_INTERVAL
        load_indicator="📊 NORMAL"
    fi
    
    echo "[$counter] $(date '+%Y-%m-%d %H:%M:%S') [$load_indicator] [Cycle: ${cycle_pos}s/${CYCLE_DURATION}s] - Sending resource data..."
    
    # Generate resource data
    resource_data=$(generate_resource_data $high_load)
    
    # Send POST request to /resource endpoint
    response=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X POST \
        -H "Content-Type: application/json" \
        -d "$resource_data" \
        "$RESOURCE_ENDPOINT" 2>/dev/null)
    
    # Extract HTTP status code and response body
    http_code=$(echo "$response" | tail -n1 | sed 's/.*HTTP_CODE://')
    response_body=$(echo "$response" | sed '$d')
    
    # Check if request was successful
    if [ "$http_code" = "200" ]; then
        echo "  ✓ SUCCESS: $response_body"
        if [ "$high_load" = "true" ]; then
            echo "    🚨 Sent high resource usage data (may trigger alarms)"
        fi
    else
        echo "  ✗ FAILED (HTTP $http_code): $response_body"
    fi
    
    counter=$((counter + 1))
    
    # Wait for next interval
    sleep $interval
done