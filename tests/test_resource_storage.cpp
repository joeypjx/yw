#include <gtest/gtest.h>
#include "resource_storage.h"
#include <nlohmann/json.hpp>

class ResourceStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        storage = std::make_unique<ResourceStorage>("127.0.0.1", "test", "HZ715Net");
    }

    void TearDown() override {
        if (storage) {
            storage->disconnect();
        }
    }

    std::unique_ptr<ResourceStorage> storage;
};

TEST_F(ResourceStorageTest, ConnectionTest) {
    EXPECT_TRUE(storage->connect());
}

TEST_F(ResourceStorageTest, DatabaseCreationTest) {
    ASSERT_TRUE(storage->connect());
    EXPECT_TRUE(storage->createDatabase("resource"));
}

TEST_F(ResourceStorageTest, TableCreationTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase("resource"));
    EXPECT_TRUE(storage->createResourceTable());
}

TEST_F(ResourceStorageTest, InsertCpuDataTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase("resource"));
    ASSERT_TRUE(storage->createResourceTable());

    nlohmann::json cpuData = {
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
    };

    nlohmann::json resourceData = {
        {"cpu", cpuData}
    };

    EXPECT_TRUE(storage->insertResourceData("192.168.1.100", resourceData));
}

TEST_F(ResourceStorageTest, InsertMemoryDataTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase("resource"));
    ASSERT_TRUE(storage->createResourceTable());

    nlohmann::json memoryData = {
        {"total", 16777216000},
        {"used", 8388608000},
        {"free", 8388608000},
        {"usage_percent", 50.0}
    };

    nlohmann::json resourceData = {
        {"memory", memoryData}
    };

    EXPECT_TRUE(storage->insertResourceData("192.168.1.100", resourceData));
}

TEST_F(ResourceStorageTest, InsertCompleteResourceDataTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase("resource"));
    ASSERT_TRUE(storage->createResourceTable());

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
                {"device", "sda1"},
                {"mount_point", "/"},
                {"total", 1000000000000},
                {"used", 500000000000},
                {"free", 500000000000},
                {"usage_percent", 50.0}
            },
            {
                {"device", "sda2"},
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

    EXPECT_TRUE(storage->insertResourceData("192.168.1.100", resourceData));
}

TEST_F(ResourceStorageTest, InsertNetworkDataTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase("resource"));
    ASSERT_TRUE(storage->createResourceTable());

    nlohmann::json networkData = nlohmann::json::array({
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
    });

    nlohmann::json resourceData = {
        {"network", networkData}
    };

    EXPECT_TRUE(storage->insertResourceData("192.168.1.100", resourceData));
}

TEST_F(ResourceStorageTest, InsertDiskDataTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase("resource"));
    ASSERT_TRUE(storage->createResourceTable());

    nlohmann::json diskData = nlohmann::json::array({
        {
            {"device", "sda1"},
            {"mount_point", "/"},
            {"total", 1000000000000},
            {"used", 500000000000},
            {"free", 500000000000},
            {"usage_percent", 50.0}
        },
        {
            {"device", "sda2"},
            {"mount_point", "/home"},
            {"total", 2000000000000},
            {"used", 800000000000},
            {"free", 1200000000000},
            {"usage_percent", 40.0}
        }
    });

    nlohmann::json resourceData = {
        {"disk", diskData}
    };

    EXPECT_TRUE(storage->insertResourceData("192.168.1.100", resourceData));
}

TEST_F(ResourceStorageTest, InsertGpuDataTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase("resource"));
    ASSERT_TRUE(storage->createResourceTable());

    nlohmann::json gpuData = nlohmann::json::array({
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
    });

    nlohmann::json resourceData = {
        {"gpu", gpuData}
    };

    EXPECT_TRUE(storage->insertResourceData("192.168.1.100", resourceData));
}

TEST_F(ResourceStorageTest, InsertDataWithSpecialCharactersTest) {
    ASSERT_TRUE(storage->connect());
    ASSERT_TRUE(storage->createDatabase("resource"));
    ASSERT_TRUE(storage->createResourceTable());

    // Test with device names containing special characters like /dev/sda, /dev/nvme0n1
    nlohmann::json resourceData = {
        {"network", nlohmann::json::array({
            {
                {"interface", "eth0:1"},  // Interface with colon
                {"rx_bytes", 1000000},
                {"tx_bytes", 2000000},
                {"rx_packets", 1000},
                {"tx_packets", 1500},
                {"rx_errors", 0},
                {"tx_errors", 0}
            }
        })},
        {"disk", nlohmann::json::array({
            {
                {"device", "/dev/sda"},  // Device with slash
                {"mount_point", "/"},
                {"total", 1000000000000},
                {"used", 500000000000},
                {"free", 500000000000},
                {"usage_percent", 50.0}
            },
            {
                {"device", "/dev/nvme0n1"},  // Device with slash and numbers
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
                {"name", "NVIDIA GeForce GTX 1080 Ti"},  // GPU name with spaces
                {"compute_usage", 85.5},
                {"mem_usage", 70.0},
                {"mem_used", 7000000000},
                {"mem_total", 11000000000},
                {"temperature", 75.0},
                {"voltage", 1.1},
                {"current", 15.0},
                {"power", 250.0}
            }
        })}
    };

    EXPECT_TRUE(storage->insertResourceData("192.168.1.100", resourceData));
}

TEST_F(ResourceStorageTest, InvalidConnectionTest) {
    auto invalidStorage = std::make_unique<ResourceStorage>("invalid_host", "test", "HZ715Net");
    EXPECT_FALSE(invalidStorage->connect());
}