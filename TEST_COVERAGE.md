# Test Coverage Report

## ResourceStorage Module Test Coverage

### Connection Tests
- ✅ **ConnectionTest**: Tests successful connection to TDengine
- ✅ **InvalidConnectionTest**: Tests connection failure with invalid host

### Database and Table Tests
- ✅ **DatabaseCreationTest**: Tests database creation
- ✅ **TableCreationTest**: Tests creation of all super tables (CPU, Memory, Network, Disk, GPU)

### Individual Resource Data Tests
- ✅ **InsertCpuDataTest**: Tests CPU metrics insertion
  - Fields: usage_percent, load_avg_1m, load_avg_5m, load_avg_15m, core_count, core_allocated, temperature, voltage, current, power
- ✅ **InsertMemoryDataTest**: Tests memory metrics insertion
  - Fields: total, used, free, usage_percent
- ✅ **InsertNetworkDataTest**: Tests network metrics insertion (multiple interfaces)
  - Fields: interface, rx_bytes, tx_bytes, rx_packets, tx_packets, rx_errors, tx_errors
  - Interfaces tested: eth0, wlan0
- ✅ **InsertDiskDataTest**: Tests disk metrics insertion (multiple devices)
  - Fields: device, mount_point, total, used, free, usage_percent
  - Devices tested: sda1 (/), sda2 (/home)
- ✅ **InsertGpuDataTest**: Tests GPU metrics insertion (multiple GPUs)
  - Fields: index, name, compute_usage, mem_usage, mem_used, mem_total, temperature, voltage, current, power
  - GPUs tested: RTX 3080, RTX 3090

### Complete Integration Tests
- ✅ **InsertCompleteResourceDataTest**: Tests insertion of all resource types together
  - Includes all CPU, Memory, Network, Disk, and GPU data in a single operation
  - Verifies end-to-end functionality matching docs/data.json format
- ✅ **InsertDataWithSpecialCharactersTest**: Tests handling of special characters in device names
  - Device names with slashes: `/dev/sda`, `/dev/nvme0n1`
  - Interface names with colons: `eth0:1`
  - GPU names with spaces: `NVIDIA GeForce GTX 1080 Ti`
  - Verifies proper table name cleaning and data insertion

### Data Format Compatibility
- ✅ All tests use data structures that match the format specified in `docs/data.json`
- ✅ Network, disk, and GPU data handled as arrays (multiple instances)
- ✅ Proper JSON structure with all required fields
- ✅ Correct data types (int, double, string, uint64_t equivalent)

## Test Results Summary
- **Total Tests**: 12 tests (11 ResourceStorage tests + 1 SimpleTest)
- **Pass Rate**: 100% (12/12 tests passed)
- **Execution Time**: ~1.6 seconds
- **Coverage**: All resource types, special characters, and error conditions covered
- **C++ Standard**: C++14 compatible

## Test Data Examples

### CPU Data
```json
{
  "usage_percent": 75.5,
  "load_avg_1m": 1.2,
  "load_avg_5m": 1.5,
  "load_avg_15m": 1.8,
  "core_count": 8,
  "core_allocated": 6,
  "temperature": 65.0,
  "voltage": 1.2,
  "current": 2.5,
  "power": 85.0
}
```

### Network Data (Array)
```json
[
  {
    "interface": "eth0",
    "rx_bytes": 1000000,
    "tx_bytes": 2000000,
    "rx_packets": 1000,
    "tx_packets": 1500,
    "rx_errors": 0,
    "tx_errors": 0
  },
  {
    "interface": "wlan0",
    "rx_bytes": 500000,
    "tx_bytes": 800000,
    "rx_packets": 600,
    "tx_packets": 800,
    "rx_errors": 1,
    "tx_errors": 0
  }
]
```

### GPU Data (Array)
```json
[
  {
    "index": 0,
    "name": "NVIDIA GeForce RTX 3080",
    "compute_usage": 85.5,
    "mem_usage": 70.0,
    "mem_used": 7000000000,
    "mem_total": 10000000000,
    "temperature": 75.0,
    "voltage": 1.1,
    "current": 15.0,
    "power": 320.0
  },
  {
    "index": 1,
    "name": "NVIDIA GeForce RTX 3090",
    "compute_usage": 90.0,
    "mem_usage": 80.0,
    "mem_used": 19000000000,
    "mem_total": 24000000000,
    "temperature": 80.0,
    "voltage": 1.2,
    "current": 18.0,
    "power": 350.0
  }
]
```

## TDengine Integration Verification

All tests successfully:
- ✅ Connect to TDengine (127.0.0.1, user: test, password: HZ715Net)
- ✅ Create the `resource` database
- ✅ Create super tables for each resource type
- ✅ Create child tables with proper naming (IP dots converted to underscores)
- ✅ Insert time-series data with current timestamps
- ✅ Handle multiple instances of network interfaces, disk devices, and GPUs
- ✅ Properly tag data with host IP addresses