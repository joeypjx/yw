#include "alarm_system.h"
#include <iostream>

int main(int argc, char* argv[]) {
    // 解析命令行参数
    AlarmSystemConfig config;
    
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--help") {
            std::cout << "告警系统使用说明：\n"
                      << "  --help          显示此帮助信息\n";
            return 0;
        }
    }
    
    // 运行告警系统
    return runAlarmSystem(config);
}