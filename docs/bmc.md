static std:: string group_ip = "224.100.200.15";
static uint16_t group_port = 5715; // 组播端口

/*和风扇小板通信
1：风扇序号
2：告警类型及工作模式，各占 4bit。高 4 位
表示告警类型，低 4 位表示工作模式。0,zidong ;1,shoudong
3：转速。高 1 位表示转速单位，后 7 位表示转速数值。
转速单位为 0 时表示档位，档位定义：低速:1，中速:2，高速:3；
转速单位为1  时表示占空比，此时转速数值为转速占空比，取值 1-100。
*/

typedef struct __attribute__((packed))
{
  uint8_t fanseq;//序号
  uint8_t fanmode;//告警及工作模式，高4位告警类型，低4位工作模式
  uint32_t fanspeed;//转速
}UdpFanInfo;

typedef struct __attribute__((packed))
{
  uint8_t sensorseq;//传感器序号
  uint8_t sensortype;//传感器类型
  uint8_t sensorname[6];//传感器名称
  uint8_t sensorvalue_L;//数值
  uint8_t sensorvalue_H;//数值
  uint8_t sensoralmtype;//告警类型
  uint8_t sensorresv;//预留

}UdpSensorInfo;


typedef struct __attribute__((packed))
{
uint8_t ipmbaddr;//槽位地址，槽位号
uint16_t moduletype;//模块设备号
uint16_t bmccompany;//厂商编号
uint8_t bmcversion[8];//BMC软件版本
uint8_t sensornum;//不超过5个
UdpSensorInfo sensor[5];
uint8_t resv[2];//0
}UdpBoardInfo;


typedef struct __attribute__((packed))
{
uint16_t head;//0xA55A
uint16_t msglenth;//报文长度
uint16_t seqnum;//1-65535循环
uint16_t msgtype;//表征报文种类，信号处理取0x0002
uint32_t timestamp;//时间戳
uint8_t recv[4];
uint8_t boxname;//固定1
uint8_t boxid;
UdpFanInfo fan[2];
UdpBoardInfo board[14];//电源1/2，负载1-12
uint16_t tail;//0xA55A
}UdpInfo;

ipmbaddr -> slotid
#define BOARD_S1 0x7c               //slotid 1   
#define BOARD_S2 0x7a
#define BOARD_S3 0x38
#define BOARD_S4 0x76
#define BOARD_S5 0x34
#define BOARD_S6 0x32
#define BOARD_S7 0x70
#define BOARD_S8 0x6e
#define BOARD_S9 0x2c
#define BOARD_S10 0x2a
#define BOARD_S11 0x68
#define BOARD_S12 0x26
#define BOARD_S13 0x2               //D1 slotid 13
#define BOARD_S14 0x4               //D2 slotid 14
