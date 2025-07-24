# 资源相关接口

## 测试地址及端口

192.168.10.58:8080


## GET /node
### 节点列表

```json
{
    "api_version": 1,
    "data": {
        "nodes": [
            {
                "board_type": "GPU",
                "box_id": 1,
                "box_type": "计算I型",
                "cpu_arch": "aarch64",
                "cpu_id": 1,
                "cpu_type": " Phytium,D2000/8",
                "created_at": 1750038100,
                "gpu": [
                    {
                        "index": 0,
                        "name": " Iluvatar MR-V50A"
                    }
                ],
                "host_ip": "192.168.10.58",
                "hostname": "localhost.localdomain",
                "id": 1,
                "os_type": "Kylin Linux Advanced Server V10",
                "resource_type": "GPU I",
                "service_port": 23980,
                "slot_id": 1,
                "srio_id": 5,
                "status": "online",
                "updated_at": 1750122059
            }
        ]
    },
    "status": "success"
}
```

## GET /node/metrics
### 节点（实时资源详情）列表

```json
{
    "api_version": 1,
    "data": {
        "nodes_metrics": [
            {
                "board_type": "GPU",
                "box_id": 1,
                "box_type": "计算I型",
                "cpu_arch": "aarch64",
                "cpu_id": 1,
                "cpu_type": " Phytium,D2000/8",
                "created_at": 1750038100,
                "gpu": [
                    {
                        "index": 0,
                        "name": " Iluvatar MR-V50A"
                    }
                ],
                "host_ip": "192.168.10.58",
                "hostname": "localhost.localdomain",
                "id": 1,
                "latest_cpu_metrics": {
                    "core_allocated": 0,
                    "core_count": 8,
                    "current": 2.0,
                    "load_avg_15m": 0.69,
                    "load_avg_1m": 1.12,
                    "load_avg_5m": 1.12,
                    "power": 23.8,
                    "temperature": 35.0,
                    "timestamp": 1750122089,
                    "usage_percent": 12.83806343906511,
                    "voltage": 11.9
                },
                "latest_disk_metrics": {
                    "disk_count": 2,
                    "disks": [
                        {
                            "device": "/dev/mapper/klas-root",
                            "free": 6929584128,
                            "mount_point": "/",
                            "total": 107321753600,
                            "usage_percent": 93.54316911944309,
                            "used": 100392169472
                        },
                        {
                            "device": "/dev/mapper/klas-data",
                            "free": 34764398592,
                            "mount_point": "/data",
                            "total": 255149084672,
                            "usage_percent": 86.37486838853825,
                            "used": 220384686080
                        }
                    ],
                    "timestamp": 1750122089
                },
                "latest_docker_metrics": {
                    "component": [],
                    "container_count": 0,
                    "paused_count": 0,
                    "running_count": 0,
                    "stopped_count": 0,
                    "timestamp": 1750122089
                },
                "latest_gpu_metrics": {
                    "gpu_count": 1,
                    "gpus": [
                        {
                            "compute_usage": 0.0,
                            "current": 0.0,
                            "index": 0,
                            "mem_total": 17179869184,
                            "mem_usage": 1.0,
                            "mem_used": 119537664,
                            "name": " Iluvatar MR-V50A",
                            "power": 0.0,
                            "temperature": 0.0,
                            "voltage": 0.0
                        }
                    ],
                    "timestamp": 1750122089
                },
                "latest_memory_metrics": {
                    "free": 12675907584,
                    "timestamp": 1750122089,
                    "total": 15647768576,
                    "usage_percent": 18.992235075345736,
                    "used": 2971860992
                },
                "latest_network_metrics": {
                    "network_count": 5,
                    "networks": [
                        {
                            "interface": "bond0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "tx_bytes": 0,
                            "tx_errors": 0,
                            "tx_packets": 0,
                            "tx_rate": 384,
                            "rx_rate": 384
                        },
                        {
                            "interface": "docker0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "tx_bytes": 716,
                            "tx_errors": 0,
                            "tx_packets": 8,
                            "tx_rate": 384,
                            "rx_rate": 384
                        },
                        {
                            "interface": "virbr0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "tx_bytes": 0,
                            "tx_errors": 0,
                            "tx_packets": ,
                            "tx_rate": 384,
                            "rx_rate": 384
                        },
                        {
                            "interface": "docker_gwbridge",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "tx_bytes": 446,
                            "tx_errors": 0,
                            "tx_packets": 5,
                            "tx_rate": 384,
                            "rx_rate": 384
                        },
                        {
                            "interface": "enaphyt4i0",
                            "rx_bytes": 3947210,
                            "rx_errors": 0,
                            "rx_packets": 40992,
                            "tx_bytes": 12865772,
                            "tx_errors": 0,
                            "tx_packets": 19010,
                            "tx_rate": 384,
                            "rx_rate": 384
                        }
                    ],
                    "timestamp": 1750122089
                },
                "os_type": "Kylin Linux Advanced Server V10",
                "resource_type": "GPU I",
                "service_port": 23980,
                "slot_id": 1,
                "srio_id": 5,
                "status": "online",
                "updated_at": 1750122094
            }
        ]
    },
    "status": "success"
}
```

## GET /node/hierarchical
### 节点（层次化）列表


```json
{
    "api_version": 1,
    "data": {
        "nodes_hierarchical": [
            {
                "box_id": 1,
                "box_type": "计算I型",
                "slots": [
                    {
                        "board_type": "GPU",
                        "cpus": [
                            {
                                "cpu_arch": "aarch64",
                                "cpu_id": 1,
                                "cpu_type": " Phytium,D2000/8",
                                "created_at": 1750038100,
                                "gpu": [
                                    {
                                        "index": 0,
                                        "name": " Iluvatar MR-V50A"
                                    }
                                ],
                                "host_ip": "192.168.10.58",
                                "hostname": "localhost.localdomain",
                                "id": 1,
                                "os_type": "Kylin Linux Advanced Server V10",
                                "resource_type": "GPU I",
                                "service_port": 23980,
                                "srio_id": 5,
                                "status": "online",
                                "updated_at": 1750122159
                            }
                        ],
                        "slot_id": 1
                    }
                ]
            }
        ]
    },
    "status": "success"
}
```