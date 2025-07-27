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

## GET /node/historical-metrics?host_ip=192.168.10.58&time_range=1m&metrics=cpu,memory,disk,network,container,gpu
### 指定节点资源详情

```json
{
    "api_version": 1,
    "data": {
        "historical_metrics": {
            "box_id": 0,
            "cpu_id": 1,
            "host_ip": "192.168.10.58",
            "metrics": {
                "container": {

                },
                "cpu": [
                    {
                        "core_allocated": 0,
                        "core_count": 8,
                        "load_avg_15m": 1.7400000095367432,
                        "load_avg_1m": 1.1100000143051147,
                        "load_avg_5m": 1.4299999475479126,
                        "power": 0.0,
                        "temperature": 0.0,
                        "timestamp": 1753426161,
                        "usage_percent": 13.561190605163574
                    },
                    {
                        "core_allocated": 0,
                        "core_count": 8,
                        "load_avg_15m": 1.7400000095367432,
                        "load_avg_1m": 1.2300000190734863,
                        "load_avg_5m": 1.440000057220459,
                        "power": 0.0,
                        "temperature": 0.0,
                        "timestamp": 1753426176,
                        "usage_percent": 13.876651763916016
                    },
                    {
                        "core_allocated": 0,
                        "core_count": 8,
                        "load_avg_15m": 1.75,
                        "load_avg_1m": 1.4199999570846558,
                        "load_avg_5m": 1.4700000286102295,
                        "power": 0.0,
                        "temperature": 0.0,
                        "timestamp": 1753426191,
                        "usage_percent": 13.84615421295166
                    },
                    {
                        "core_allocated": 0,
                        "core_count": 8,
                        "load_avg_15m": 1.7400000095367432,
                        "load_avg_1m": 1.399999976158142,
                        "load_avg_5m": 1.4600000381469727,
                        "power": 0.0,
                        "temperature": 0.0,
                        "timestamp": 1753426206,
                        "usage_percent": 13.457329750061035
                    }
                ],
                "disk": {
                    "_dev_mapper_klas_data": [
                        {
                            "device": "/dev/mapper/klas-data",
                            "free": 24269406208,
                            "mount_point": "/data",
                            "timestamp": 1753426161,
                            "total": 255149084672,
                            "usage_percent": 90.48814392089844,
                            "used": 230879678464
                        },
                        {
                            "device": "/dev/mapper/klas-data",
                            "free": 24269361152,
                            "mount_point": "/data",
                            "timestamp": 1753426176,
                            "total": 255149084672,
                            "usage_percent": 90.48816680908203,
                            "used": 230879723520
                        },
                        {
                            "device": "/dev/mapper/klas-data",
                            "free": 24269361152,
                            "mount_point": "/data",
                            "timestamp": 1753426191,
                            "total": 255149084672,
                            "usage_percent": 90.48816680908203,
                            "used": 230879723520
                        },
                        {
                            "device": "/dev/mapper/klas-data",
                            "free": 24269381632,
                            "mount_point": "/data",
                            "timestamp": 1753426206,
                            "total": 255149084672,
                            "usage_percent": 90.4881591796875,
                            "used": 230879703040
                        }
                    ],
                    "_dev_mapper_klas_root": [
                        {
                            "device": "/dev/mapper/klas-root",
                            "free": 1241542656,
                            "mount_point": "/",
                            "timestamp": 1753426161,
                            "total": 107321753600,
                            "usage_percent": 98.84315490722656,
                            "used": 106080210944
                        },
                        {
                            "device": "/dev/mapper/klas-root",
                            "free": 1241542656,
                            "mount_point": "/",
                            "timestamp": 1753426176,
                            "total": 107321753600,
                            "usage_percent": 98.84315490722656,
                            "used": 106080210944
                        },
                        {
                            "device": "/dev/mapper/klas-root",
                            "free": 1241460736,
                            "mount_point": "/",
                            "timestamp": 1753426191,
                            "total": 107321753600,
                            "usage_percent": 98.84323120117188,
                            "used": 106080292864
                        },
                        {
                            "device": "/dev/mapper/klas-root",
                            "free": 1241481216,
                            "mount_point": "/",
                            "timestamp": 1753426206,
                            "total": 107321753600,
                            "usage_percent": 98.84321594238281,
                            "used": 106080272384
                        }
                    ]
                },
                "gpu": {
                    "gpu_0": [
                        {
                            "compute_usage": 0.0,
                            "index": 0,
                            "mem_total": 17179869184,
                            "mem_usage": 0.009999999776482582,
                            "mem_used": 119537664,
                            "name": " Iluvatar MR-V50A",
                            "power": 19.0,
                            "temperature": 44.0,
                            "timestamp": 1753426161
                        },
                        {
                            "compute_usage": 0.0,
                            "index": 0,
                            "mem_total": 17179869184,
                            "mem_usage": 0.009999999776482582,
                            "mem_used": 119537664,
                            "name": " Iluvatar MR-V50A",
                            "power": 19.0,
                            "temperature": 44.0,
                            "timestamp": 1753426176
                        },
                        {
                            "compute_usage": 0.0,
                            "index": 0,
                            "mem_total": 17179869184,
                            "mem_usage": 0.009999999776482582,
                            "mem_used": 119537664,
                            "name": " Iluvatar MR-V50A",
                            "power": 19.0,
                            "temperature": 44.0,
                            "timestamp": 1753426191
                        },
                        {
                            "compute_usage": 0.0,
                            "index": 0,
                            "mem_total": 17179869184,
                            "mem_usage": 0.009999999776482582,
                            "mem_used": 119537664,
                            "name": " Iluvatar MR-V50A",
                            "power": 19.0,
                            "temperature": 45.0,
                            "timestamp": 1753426206
                        }
                    ]
                },
                "memory": [
                    {
                        "free": 7097614336,
                        "timestamp": 1753426161,
                        "total": 15647768576,
                        "usage_percent": 54.64136505126953,
                        "used": 8550154240
                    },
                    {
                        "free": 7089618944,
                        "timestamp": 1753426176,
                        "total": 15647768576,
                        "usage_percent": 54.69245910644531,
                        "used": 8558149632
                    },
                    {
                        "free": 7085883392,
                        "timestamp": 1753426191,
                        "total": 15647768576,
                        "usage_percent": 54.716331481933594,
                        "used": 8561885184
                    },
                    {
                        "free": 7113146368,
                        "timestamp": 1753426206,
                        "total": 15647768576,
                        "usage_percent": 54.5421028137207,
                        "used": 8534622208
                    }
                ],
                "network": {
                    "bond0": [
                        {
                            "interface": "bond0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "timestamp": 1753426161,
                            "tx_bytes": 0,
                            "tx_errors": 0,
                            "tx_packets": 0
                        },
                        {
                            "interface": "bond0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "timestamp": 1753426176,
                            "tx_bytes": 0,
                            "tx_errors": 0,
                            "tx_packets": 0
                        },
                        {
                            "interface": "bond0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "timestamp": 1753426191,
                            "tx_bytes": 0,
                            "tx_errors": 0,
                            "tx_packets": 0
                        },
                        {
                            "interface": "bond0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "timestamp": 1753426206,
                            "tx_bytes": 0,
                            "tx_errors": 0,
                            "tx_packets": 0
                        }
                    ],
                    "docker0": [
                        {
                            "interface": "docker0",
                            "rx_bytes": 487211465,
                            "rx_errors": 0,
                            "rx_packets": 99764,
                            "timestamp": 1753426161,
                            "tx_bytes": 475393371,
                            "tx_errors": 0,
                            "tx_packets": 86587
                        },
                        {
                            "interface": "docker0",
                            "rx_bytes": 495479045,
                            "rx_errors": 0,
                            "rx_packets": 100648,
                            "timestamp": 1753426176,
                            "tx_bytes": 475587313,
                            "tx_errors": 0,
                            "tx_packets": 87256
                        },
                        {
                            "interface": "docker0",
                            "rx_bytes": 495542909,
                            "rx_errors": 0,
                            "rx_packets": 101015,
                            "timestamp": 1753426191,
                            "tx_bytes": 475693703,
                            "tx_errors": 0,
                            "tx_packets": 87539
                        },
                        {
                            "interface": "docker0",
                            "rx_bytes": 495657390,
                            "rx_errors": 0,
                            "rx_packets": 101611,
                            "timestamp": 1753426206,
                            "tx_bytes": 475832873,
                            "tx_errors": 0,
                            "tx_packets": 87998
                        }
                    ],
                    "docker_gwbridge": [
                        {
                            "interface": "docker_gwbridge",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "timestamp": 1753426161,
                            "tx_bytes": 446,
                            "tx_errors": 0,
                            "tx_packets": 5
                        },
                        {
                            "interface": "docker_gwbridge",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "timestamp": 1753426176,
                            "tx_bytes": 446,
                            "tx_errors": 0,
                            "tx_packets": 5
                        },
                        {
                            "interface": "docker_gwbridge",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "timestamp": 1753426191,
                            "tx_bytes": 446,
                            "tx_errors": 0,
                            "tx_packets": 5
                        },
                        {
                            "interface": "docker_gwbridge",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "timestamp": 1753426206,
                            "tx_bytes": 446,
                            "tx_errors": 0,
                            "tx_packets": 5
                        }
                    ],
                    "enaphyt4i0": [
                        {
                            "interface": "enaphyt4i0",
                            "rx_bytes": 4634635016,
                            "rx_errors": 0,
                            "rx_packets": 15660345,
                            "timestamp": 1753426161,
                            "tx_bytes": 4626000890,
                            "tx_errors": 0,
                            "tx_packets": 6736346
                        },
                        {
                            "interface": "enaphyt4i0",
                            "rx_bytes": 4634752194,
                            "rx_errors": 0,
                            "rx_packets": 15662103,
                            "timestamp": 1753426176,
                            "tx_bytes": 4626050781,
                            "tx_errors": 0,
                            "tx_packets": 6736688
                        },
                        {
                            "interface": "enaphyt4i0",
                            "rx_bytes": 4634862821,
                            "rx_errors": 0,
                            "rx_packets": 15663818,
                            "timestamp": 1753426191,
                            "tx_bytes": 4626083034,
                            "tx_errors": 0,
                            "tx_packets": 6736966
                        },
                        {
                            "interface": "enaphyt4i0",
                            "rx_bytes": 4634965372,
                            "rx_errors": 0,
                            "rx_packets": 15665361,
                            "timestamp": 1753426206,
                            "tx_bytes": 4626121662,
                            "tx_errors": 0,
                            "tx_packets": 6737247
                        }
                    ],
                    "virbr0": [
                        {
                            "interface": "virbr0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "timestamp": 1753426161,
                            "tx_bytes": 0,
                            "tx_errors": 0,
                            "tx_packets": 0
                        },
                        {
                            "interface": "virbr0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "timestamp": 1753426176,
                            "tx_bytes": 0,
                            "tx_errors": 0,
                            "tx_packets": 0
                        },
                        {
                            "interface": "virbr0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "timestamp": 1753426191,
                            "tx_bytes": 0,
                            "tx_errors": 0,
                            "tx_packets": 0
                        },
                        {
                            "interface": "virbr0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "timestamp": 1753426206,
                            "tx_bytes": 0,
                            "tx_errors": 0,
                            "tx_packets": 0
                        }
                    ]
                }
            },
            "slot_id": 0,
            "time_range": "1m"
        }
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
                "box_id": 0,
                "box_type": "",
                "cpu_arch": "aarch64",
                "cpu_id": 1,
                "cpu_type": " Phytium,D2000/8",
                "created_at": 1753425801,
                "gpu": [
                    {
                        "index": 0,
                        "name": " Iluvatar MR-V50A"
                    }
                ],
                "host_ip": "192.168.10.58",
                "hostname": "localhost.localdomain",
                "id": 1,
                "latest_container_metrics": {
                    "component": [
                        {
                            "component_index": 1,
                            "container_id": "4c34877129713614d7a5eb89a00d117a72ffcc1e560bd7ea57962cb2e26b0000",
                            "container_name": "192.168.10.58:5000/sleep3:latest",
                            "instance_id": "874811e8-9e53-41ea-a543-275a4bff3864",
                            "resource": {
                                "cpu": {
                                    "load": 0.0
                                },
                                "memory": {
                                    "mem_limit": 15647768576,
                                    "mem_used": 524288,
                                    "usage_percent": 0.0033505608001139194
                                },
                                "network": {
                                    "rx": 0,
                                    "tx": 0
                                }
                            },
                            "status": "RUNNING",
                            "timestamp": 1752713939,
                            "uuid": "1233211234569"
                        }
                    ],
                    "container_count": 40,
                    "paused_count": 0,
                    "running_count": 0,
                    "stopped_count": 0,
                    "timestamp": 1752713939
                },
                "latest_cpu_metrics": {
                    "core_allocated": 0,
                    "core_count": 8,
                    "current": 0.0,
                    "load_avg_15m": 1.2899999618530273,
                    "load_avg_1m": 1.2000000476837158,
                    "load_avg_5m": 1.190000057220459,
                    "power": 0.0,
                    "temperature": 0.0,
                    "timestamp": 1753428501,
                    "usage_percent": 14.112459182739258,
                    "voltage": 0.0
                },
                "latest_disk_metrics": {
                    "disk_count": 2,
                    "disks": [
                        {
                            "device": "/dev/mapper/klas-root",
                            "free": 1205067776,
                            "mount_point": "/",
                            "timestamp": 1753428501,
                            "total": 107321753600,
                            "usage_percent": 98.87714385986328,
                            "used": 106116685824
                        },
                        {
                            "device": "/dev/mapper/klas-data",
                            "free": 24150638592,
                            "mount_point": "/data",
                            "timestamp": 1753428501,
                            "total": 255149084672,
                            "usage_percent": 90.53469848632813,
                            "used": 230998446080
                        }
                    ]
                },
                "latest_gpu_metrics": {
                    "gpu_count": 1,
                    "gpus": [
                        {
                            "compute_usage": 0.0,
                            "current": 0.0,
                            "index": 0,
                            "mem_total": 17179869184,
                            "mem_usage": 0.009999999776482582,
                            "mem_used": 119537664,
                            "name": " Iluvatar MR-V50A",
                            "power": 19.0,
                            "temperature": 45.0,
                            "timestamp": 1753428501,
                            "voltage": 0.0
                        }
                    ]
                },
                "latest_memory_metrics": {
                    "free": 7225147392,
                    "timestamp": 1753428501,
                    "total": 15647768576,
                    "usage_percent": 53.82633972167969,
                    "used": 8422621184
                },
                "latest_network_metrics": {
                    "network_count": 5,
                    "networks": [
                        {
                            "interface": "bond0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "timestamp": 1753428501,
                            "tx_bytes": 0,
                            "tx_errors": 0,
                            "tx_packets": 0
                        },
                        {
                            "interface": "docker0",
                            "rx_bytes": 1201064492,
                            "rx_errors": 0,
                            "rx_packets": 191884,
                            "timestamp": 1753428501,
                            "tx_bytes": 503961699,
                            "tx_errors": 0,
                            "tx_packets": 157325
                        },
                        {
                            "interface": "virbr0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "timestamp": 1753428501,
                            "tx_bytes": 0,
                            "tx_errors": 0,
                            "tx_packets": 0
                        },
                        {
                            "interface": "enaphyt4i0",
                            "rx_bytes": 4653124362,
                            "rx_errors": 0,
                            "rx_packets": 15926364,
                            "timestamp": 1753428501,
                            "tx_bytes": 4634613068,
                            "tx_errors": 0,
                            "tx_packets": 6797803
                        },
                        {
                            "interface": "docker_gwbridge",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "timestamp": 1753428501,
                            "tx_bytes": 446,
                            "tx_errors": 0,
                            "tx_packets": 5
                        }
                    ]
                },
                "os_type": "Kylin Linux Advanced Server V10",
                "resource_type": "GPU I",
                "service_port": 23980,
                "slot_id": 0,
                "srio_id": 1,
                "status": "online",
                "updated_at": 1753428506
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