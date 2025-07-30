#ifndef UTILS_H
#define UTILS_H

#include <string>

namespace Utils {

/**
 * 根据box_id和slot_id计算host_ip
 * 
 * @param box_id 机箱号
 * @param slot_id 槽位号
 * @return 计算出的host_ip字符串
 * 
 * 计算规则：
 * - slot_id 1-7: 192.168.(box_id*2).X
 * - slot_id 8-12: 192.168.(box_id*2+1).X
 * 
 * 具体映射：
 * - slot_id 1 -> 192.168.(box_id*2).5
 * - slot_id 2 -> 192.168.(box_id*2).37
 * - slot_id 3 -> 192.168.(box_id*2).69
 * - slot_id 4 -> 192.168.(box_id*2).101
 * - slot_id 5 -> 192.168.(box_id*2).133
 * - slot_id 6 -> 192.168.(box_id*2).170
 * - slot_id 7 -> 192.168.(box_id*2).180
 * - slot_id 8 -> 192.168.(box_id*2+1).5
 * - slot_id 9 -> 192.168.(box_id*2+1).37
 * - slot_id 10 -> 192.168.(box_id*2+1).69
 * - slot_id 11 -> 192.168.(box_id*2+1).101
 * - slot_id 12 -> 192.168.(box_id*2+1).133
 */
std::string calculateHostIP(int box_id, int slot_id);

/**
 * 将十六进制的ipmbaddr转换为slotid号
 * 
 * @param ipmbaddr 十六进制的IPMB地址
 * @return 对应的slotid号，如果未找到匹配则返回-1
 * 
 * 映射关系：
 * - 0x7c -> slotid 1
 * - 0x7a -> slotid 2
 * - 0x38 -> slotid 3
 * - 0x76 -> slotid 4
 * - 0x34 -> slotid 5
 * - 0x32 -> slotid 6
 * - 0x70 -> slotid 7
 * - 0x6e -> slotid 8
 * - 0x2c -> slotid 9
 * - 0x2a -> slotid 10
 * - 0x68 -> slotid 11
 * - 0x26 -> slotid 12
 * - 0x02 -> slotid 13 (D1)
 * - 0x04 -> slotid 14 (D2)
 */
uint8_t ipmbaddrToSlotId(uint8_t ipmbaddr);

} // namespace Utils

#endif // UTILS_H 