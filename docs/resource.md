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
###

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
                    "container_0_0_0155b93746663d1fce2cfdf1048ce81618d94be86a17cb793099e9429e880747": [],
                    "container_0_0_016963f7ea7ecae47608260c01d29f85b251811a340ab6af402fc2033f026c8e": [],
                    "container_0_0_0c5cee9ab102ebb18f60a7a00abf9f68ac29cfada63cc4b0d5a7ff43b9cfe7e8": [],
                    "container_0_0_0e3b3ed863aa1679be8bf8f9adeb0505745aac6f9cbab4375dee136334217005": [],
                    "container_0_0_1f6657db01c94462c4b5b8ff1a55043cacbe2aacbbfbceec550342d1fb7b3b7e": [],
                    "container_0_0_20b08c2a05d6601610196abf25d812e7b98be6d951b53ff6f0bb888fae1665d1": [],
                    "container_0_0_2c0cedc24f94698b1647a4b62bca0ef42c0aa87f5d51bad3b2f6ab91d7a0c35e": [],
                    "container_0_0_2cdee5457d351f5c511358f3196e562de1cd82fcd0b4b03bb608a56c8a6378d3": [],
                    "container_0_0_2f66e573bf5596db7649cbd8a707aee075094a8cb359c2fa27f7bae27b7e5029": [],
                    "container_0_0_34d7e4f0374d9fe853660d81a3873d494722ec9a481cbee5893ed77dbbd516c0": [],
                    "container_0_0_3544be3a8f775c7447a944a38d80115c2b38178ec7e16e76cec7aedfa43767a4": [],
                    "container_0_0_36dfb0fff6583685486ce003ce95b5529552fbfc1fdb4cf106c07705196bb7d0": [],
                    "container_0_0_3f65cd2d8e2973374048fc6d06fe2844e3039387aadd0404b6ba26eed67c0010": [],
                    "container_0_0_485e5bad36133efbb233d9154aea6dc6b1d98276b75f3fef2189ea8a4b7d7c90": [],
                    "container_0_0_4c34877129713614d7a5eb89a00d117a72ffcc1e560bd7ea57962cb2e26b0000": [],
                    "container_0_0_5ff62dab23a468a34885226d265a8a2801abc9a1a60f35eadf615ea16e050e22": [],
                    "container_0_0_7078d02cc52f1afdd994f8495f80d3c8e635135b429955caea4f5c7dc77bedc9": [],
                    "container_0_0_74c4e179b47f5528736fbc11a38a1e7fade95a51f3555edd38e794ebc2c581d1": [],
                    "container_0_0_75c8ade4a6f444a8c0d0da1c878b0d9850308428d216ec37d28dcaf02cc56926": [],
                    "container_0_0_7d23f8c2ec0df0150266d765bdd8cf36f50d6842ce5553cd9fb6de3fadef6573": [],
                    "container_0_0_7da6f93d92600bac0651b444c2656c024218872edc104cb10a9acb737aee2ece": [],
                    "container_0_0_8366b19a68234d55794cc9258118bc167669b1c445e8923690b98f684a27c3b5": [],
                    "container_0_0_8d786fc894a4dd1cf3b2f7cc65ced993defd68d4d16f755fa755d4f6c0098256": [],
                    "container_0_0_8f112e36c73d96a728bee93a722c74cca7ecdfb26001ac16088163a537ca6179": [],
                    "container_0_0_8f9614ff81f0d7c650a2649001a4c1e02a2a53d0533a19e5ec238f94ea2292bd": [],
                    "container_0_0_95341ac3c711466c52db0da447467616f4fe8417abe943c6adb97f23bce034e3": [],
                    "container_0_0_99768218b443bc6341a1de41f8a3fbfd1b046419ff8205f34ddeef583fe253ea": [],
                    "container_0_0_99bfa4cf93e758f252675bb77c3d40d4a3738b30d7afa3a7473b5c099e97e5bc": [],
                    "container_0_0_a86b7fbd621d5dc5dbbbbcdcf6f9578ef991cfc64ecb71630cb2780c35206f54": [],
                    "container_0_0_aa74c11ca9237861a619066cdfe1db57f2e64504647ac547bf88b8ed654a977d": [],
                    "container_0_0_af3182bff7af47b5d5c7b3baf06d6da554b1462e4c630fe25d5c1ba0a40e4b59": [],
                    "container_0_0_bf7279c7e2fcb3bb0e1e556a929b58b916cc080e09f0bc7c13253294995b6bdf": [],
                    "container_0_0_c39397150a47021c5e2629e8d9903fe933ddf6f32a5eb7b9ea018fd7eb36a81e": [],
                    "container_0_0_c9c391097bfe8e7614584f31ceda902593330022bf80bf5adbe9be9fe3e89d89": [],
                    "container_0_0_d09bbe348d475100d111ebe8fd39f7913778b53ece2e7b3c82cd7913b091b239": [],
                    "container_0_0_d5b6a89d08dcee9934ad4404a434fb1209a1b729aff675d2cdbdedd08e9ea8c8": [],
                    "container_0_0_dd3850fba931c058732f67deb3327621746c07e8ae53056463d6a04251a3db73": [],
                    "container_0_0_f42171284a78528e91f75399a2725bd28033e82324d96e4e0335f4ffde55cc7f": [],
                    "container_0_0_fec7c85b23edcd9b49aa9170d7472f87725ec18b9f606ad93ccdf565ef08ddbd": [],
                    "container_0_0_ff79014d04d0e0ebc90b340f2cd01f16c806834a15a1109e0447bd8e76a40506": []
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
                    "timestamp": 1750122089668,
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
                    "timestamp": 1750122089668
                },
                "latest_docker_metrics": {
                    "component": [],
                    "container_count": 0,
                    "paused_count": 0,
                    "running_count": 0,
                    "stopped_count": 0,
                    "timestamp": 1750122089668
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
                    "timestamp": 1750122089668
                },
                "latest_memory_metrics": {
                    "free": 12675907584,
                    "timestamp": 1750122089668,
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
                            "tx_packets": 0
                        },
                        {
                            "interface": "docker0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "tx_bytes": 716,
                            "tx_errors": 0,
                            "tx_packets": 8
                        },
                        {
                            "interface": "virbr0",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "tx_bytes": 0,
                            "tx_errors": 0,
                            "tx_packets": 0
                        },
                        {
                            "interface": "docker_gwbridge",
                            "rx_bytes": 0,
                            "rx_errors": 0,
                            "rx_packets": 0,
                            "tx_bytes": 446,
                            "tx_errors": 0,
                            "tx_packets": 5
                        },
                        {
                            "interface": "enaphyt4i0",
                            "rx_bytes": 3947210,
                            "rx_errors": 0,
                            "rx_packets": 40992,
                            "tx_bytes": 12865772,
                            "tx_errors": 0,
                            "tx_packets": 19010
                        }
                    ],
                    "timestamp": 1750122089668
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