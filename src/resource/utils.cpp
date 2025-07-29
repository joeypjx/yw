#include "utils.h"
#include "log_manager.h"
#include <cstdint>

namespace Utils {

std::string calculateHostIP(int box_id, int slot_id) {
    // 根据规则计算host_ip
    // 192.168.(box_id*2).5 slot_id 1
    // 192.168.(box_id*2).37 slot_id 2
    // 192.168.(box_id*2).69 slot_id 3
    // 192.168.(box_id*2).101 slot_id 4
    // 192.168.(box_id*2).133 slot_id 5
    // 192.168.(box_id*2).170 slot_id 6
    // 192.168.(box_id*2).180 slot_id 7
    // 192.168.(box_id*2+1).5 slot_id 8
    // 192.168.(box_id*2+1).37 slot_id 9
    // 192.168.(box_id*2+1).69 slot_id 10
    // 192.168.(box_id*2+1).101 slot_id 11
    // 192.168.(box_id*2+1).133 slot_id 12
    
    int network_id;
    int host_id;
    
    if (slot_id >= 1 && slot_id <= 7) {
        // slot_id 1-7: 使用 box_id*2
        network_id = box_id * 2;
        switch (slot_id) {
            case 1: host_id = 5; break;
            case 2: host_id = 37; break;
            case 3: host_id = 69; break;
            case 4: host_id = 101; break;
            case 5: host_id = 133; break;
            case 6: host_id = 170; break;
            case 7: host_id = 180; break;
            default: host_id = 5; break;
        }
    } else if (slot_id >= 8 && slot_id <= 12) {
        // slot_id 8-12: 使用 box_id*2+1
        network_id = box_id * 2 + 1;
        switch (slot_id) {
            case 8: host_id = 5; break;
            case 9: host_id = 37; break;
            case 10: host_id = 69; break;
            case 11: host_id = 101; break;
            case 12: host_id = 133; break;
            default: host_id = 5; break;
        }
    } else {
        // 无效的slot_id，返回默认值
        network_id = box_id * 2;
        host_id = 5;
        LogManager::getLogger()->warn("Invalid slot_id: {}, using default host_ip calculation", slot_id);
    }
    
    return "192.168." + std::to_string(network_id) + "." + std::to_string(host_id);
}

int ipmbaddrToSlotId(uint8_t ipmbaddr) {
    // 根据映射关系转换ipmbaddr为slotid
    switch (ipmbaddr) {
        case 0x7c: return 1;   // BOARD_S1
        case 0x7a: return 2;   // BOARD_S2
        case 0x38: return 3;   // BOARD_S3
        case 0x76: return 4;   // BOARD_S4
        case 0x34: return 5;   // BOARD_S5
        case 0x32: return 6;   // BOARD_S6
        case 0x70: return 7;   // BOARD_S7
        case 0x6e: return 8;   // BOARD_S8
        case 0x2c: return 9;   // BOARD_S9
        case 0x2a: return 10;  // BOARD_S10
        case 0x68: return 11;  // BOARD_S11
        case 0x26: return 12;  // BOARD_S12
        case 0x02: return 13;  // BOARD_S13 (D1)
        case 0x04: return 14;  // BOARD_S14 (D2)
        default:
            LogManager::getLogger()->warn("Unknown ipmbaddr: 0x{:02x}, returning -1", ipmbaddr);
            return -1;
    }
}

} // namespace Utils 