#!/bin/bash

# 告警规则测试运行脚本
# 用于运行告警规则相关的测试用例

set -e

echo "=== 告警规则测试运行脚本 ==="
echo

# 检查构建目录
if [ ! -d "build" ]; then
    echo "创建构建目录..."
    mkdir build
fi

cd build

echo "1. 配置CMake..."
cmake ..

echo
echo "2. 构建项目和测试..."
make

echo
echo "3. 运行所有测试..."
make test

echo
echo "4. 运行特定的告警规则测试..."
echo "运行HTTP服务器告警规则测试:"
./tests --gtest_filter="HttpServerAlarmRulesTest.*"

echo
echo "运行指标名称解析测试:"
./tests --gtest_filter="MetricNameParsingTest.*"

echo
echo "运行告警规则存储测试:"
./tests --gtest_filter="AlarmRuleStorageTest.*"

echo
echo "运行告警规则引擎测试:"
./tests --gtest_filter="AlarmRuleEngineTest.*"

echo
echo "运行告警管理器测试:"
./tests --gtest_filter="AlarmManagerTest.*"

echo
echo "=== 测试完成 ==="