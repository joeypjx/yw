#include <iostream>
#include "resource_storage.h"
#include "alarm_rule_storage.h"
#include "alarm_rule_engine.h"
#include "json.hpp"
#include <thread>
#include <chrono>

int main() {
    std::cout << "Resource Storage and Alarm Rule Storage Demo" << std::endl;
    
    // Create ResourceStorage instance
    ResourceStorage storage("127.0.0.1", "test", "HZ715Net");
    
    // Connect to TDengine
    if (!storage.connect()) {
        std::cerr << "Failed to connect to TDengine" << std::endl;
        return 1;
    }
    
    // Create database
    if (!storage.createDatabase("resource")) {
        std::cerr << "Failed to create database" << std::endl;
        return 1;
    }
    
    // Create tables
    if (!storage.createResourceTable()) {
        std::cerr << "Failed to create resource tables" << std::endl;
        return 1;
    }
    
    // Sample complete resource data matching docs/data.json format
    nlohmann::json resourceData = {
        {"cpu", {
            {"usage_percent", 75.5},
            {"load_avg_1m", 1.2},
            {"load_avg_5m", 1.5},
            {"load_avg_15m", 1.8},
            {"core_count", 8},
            {"core_allocated", 6},
            {"temperature", 65.0},
            {"voltage", 1.2},
            {"current", 2.5},
            {"power", 85.0}
        }},
        {"memory", {
            {"total", 16777216000},
            {"used", 8388608000},
            {"free", 8388608000},
            {"usage_percent", 50.0}
        }},
        {"network", nlohmann::json::array({
            {
                {"interface", "eth0"},
                {"rx_bytes", 1000000},
                {"tx_bytes", 2000000},
                {"rx_packets", 1000},
                {"tx_packets", 1500},
                {"rx_errors", 0},
                {"tx_errors", 0}
            },
            {
                {"interface", "wlan0"},
                {"rx_bytes", 500000},
                {"tx_bytes", 800000},
                {"rx_packets", 600},
                {"tx_packets", 800},
                {"rx_errors", 1},
                {"tx_errors", 0}
            }
        })},
        {"disk", nlohmann::json::array({
            {
                {"device", "/dev/sda1"},
                {"mount_point", "/"},
                {"total", 1000000000000},
                {"used", 500000000000},
                {"free", 500000000000},
                {"usage_percent", 50.0}
            },
            {
                {"device", "/dev/nvme0n1"},
                {"mount_point", "/home"},
                {"total", 2000000000000},
                {"used", 800000000000},
                {"free", 1200000000000},
                {"usage_percent", 40.0}
            }
        })},
        {"gpu", nlohmann::json::array({
            {
                {"index", 0},
                {"name", "NVIDIA GeForce RTX 3080"},
                {"compute_usage", 85.5},
                {"mem_usage", 70.0},
                {"mem_used", 7000000000},
                {"mem_total", 10000000000},
                {"temperature", 75.0},
                {"voltage", 1.1},
                {"current", 15.0},
                {"power", 320.0}
            },
            {
                {"index", 1},
                {"name", "NVIDIA GeForce RTX 3090"},
                {"compute_usage", 90.0},
                {"mem_usage", 80.0},
                {"mem_used", 19000000000},
                {"mem_total", 24000000000},
                {"temperature", 80.0},
                {"voltage", 1.2},
                {"current", 18.0},
                {"power", 350.0}
            }
        })}
    };
    
    // Insert data
    if (storage.insertResourceData("192.168.1.100", resourceData)) {
        std::cout << "Successfully inserted resource data" << std::endl;
    } else {
        std::cerr << "Failed to insert resource data" << std::endl;
        return 1;
    }
    
    std::cout << "\n=== Alarm Rule Storage Demo ===\n" << std::endl;
    
    // Create AlarmRuleStorage instance
    AlarmRuleStorage alarm_storage("127.0.0.1", 3306, "test", "HZ715Net", "alarm");
    
    // Connect to MySQL
    if (!alarm_storage.connect()) {
        std::cerr << "Failed to connect to MySQL" << std::endl;
        return 1;
    }
    
    // Create database and table
    if (!alarm_storage.createDatabase()) {
        std::cerr << "Failed to create alarm database" << std::endl;
        return 1;
    }
    
    if (!alarm_storage.createTable()) {
        std::cerr << "Failed to create alarm_rules table" << std::endl;
        return 1;
    }
    
    // Create sample alarm rules based on docs/alarm.md
    
    // Rule 1: High CPU or Memory usage on web cluster
    nlohmann::json web_cluster_expression = {
        {"and", {
            {
                {"stable", "cpu_metrics"},
                {"tag", "cluster"},
                {"operator", "=="},
                {"value", "web"}
            },
            {
                {"or", {
                    {
                        {"stable", "cpu_metrics"},
                        {"metric", "usage_percent"},
                        {"agg_func", "AVG"},
                        {"operator", ">"},
                        {"threshold", 90.0}
                    },
                    {
                        {"stable", "memory_metrics"},
                        {"metric", "usage_percent"},
                        {"agg_func", "AVG"},
                        {"operator", ">"},
                        {"threshold", 95.0}
                    }
                }}
            }
        }}
    };
    
    std::string rule_id1 = alarm_storage.insertAlarmRule(
        "HighCpuOrMemOnWeb",
        web_cluster_expression,
        "5m",
        "critical",
        "Web集群资源使用率过高",
        "节点 {{host_ip}} 资源使用率异常。CPU: {{usage_percent}}%, 内存: {{usage_percent}}%。"
    );
    
    // Rule 2: High disk usage
    nlohmann::json disk_expression = {
        {"stable", "disk_metrics"},
        {"metric", "usage_percent"},
        {"agg_func", "AVG"},
        {"operator", ">"},
        {"threshold", 85.0}
    };
    
    std::string rule_id2 = alarm_storage.insertAlarmRule(
        "HighDiskUsage",
        disk_expression,
        "10m",
        "warning",
        "磁盘使用率过高",
        "节点 {{host_ip}} 磁盘 {{device}} 使用率达到 {{usage_percent}}%"
    );
    
    if (!rule_id1.empty() && !rule_id2.empty()) {
        std::cout << "Successfully inserted alarm rules:" << std::endl;
        std::cout << "Rule 1 ID: " << rule_id1 << std::endl;
        std::cout << "Rule 2 ID: " << rule_id2 << std::endl;
        
        // Get all rules
        std::vector<AlarmRule> all_rules = alarm_storage.getAllAlarmRules();
        std::cout << "\nTotal alarm rules: " << all_rules.size() << std::endl;
        
        // Display first rule details
        if (!all_rules.empty()) {
            const auto& first_rule = all_rules[0];
            std::cout << "\nFirst rule details:" << std::endl;
            std::cout << "Alert Name: " << first_rule.alert_name << std::endl;
            std::cout << "Severity: " << first_rule.severity << std::endl;
            std::cout << "For Duration: " << first_rule.for_duration << std::endl;
            std::cout << "Summary: " << first_rule.summary << std::endl;
            std::cout << "Enabled: " << (first_rule.enabled ? "true" : "false") << std::endl;
        }
    } else {
        std::cerr << "Failed to insert alarm rules" << std::endl;
        return 1;
    }
    
    std::cout << "\n=== Alarm Rule Engine Demo ===\n" << std::endl;
    
    // Create shared pointers for alarm rule engine
    auto rule_storage_ptr = std::make_shared<AlarmRuleStorage>(alarm_storage);
    auto resource_storage_ptr = std::make_shared<ResourceStorage>(storage);
    
    // Create alarm rule engine
    AlarmRuleEngine engine(rule_storage_ptr, resource_storage_ptr);
    
    // Set up alarm event callback
    std::vector<AlarmEvent> received_events;
    engine.setAlarmEventCallback([&received_events](const AlarmEvent& event) {
        std::cout << "Received alarm event: " << event.fingerprint << " - " << event.status << std::endl;
        std::cout << "Event details: " << event.toJson() << std::endl;
        received_events.push_back(event);
    });
    
    // Set evaluation interval to 5 seconds for demo
    engine.setEvaluationInterval(std::chrono::seconds(5));
    
    // Start the engine
    std::cout << "Starting alarm rule engine..." << std::endl;
    if (!engine.start()) {
        std::cerr << "Failed to start alarm rule engine" << std::endl;
        return 1;
    }
    
    std::cout << "Alarm rule engine started. Running for 30 seconds..." << std::endl;
    
    // Let the engine run for 30 seconds
    std::this_thread::sleep_for(std::chrono::seconds(30));
    
    // Stop the engine
    std::cout << "Stopping alarm rule engine..." << std::endl;
    engine.stop();
    
    // Display results
    std::cout << "\nAlarm Engine Results:" << std::endl;
    std::cout << "Total alarm events generated: " << received_events.size() << std::endl;
    
    // Get current alarm instances
    std::vector<AlarmInstance> instances = engine.getCurrentAlarmInstances();
    std::cout << "Current alarm instances: " << instances.size() << std::endl;
    
    for (const auto& instance : instances) {
        std::cout << "  - " << instance.fingerprint << " (state: " << 
                     static_cast<int>(instance.state) << ", value: " << instance.value << ")" << std::endl;
    }
    
    return 0;
}