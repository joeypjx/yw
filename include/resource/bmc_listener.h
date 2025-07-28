#ifndef BMC_LISTENER_H
#define BMC_LISTENER_H

#include <cstdint>
#include <string>
#include <functional>

typedef struct __attribute__((packed))
{
    uint8_t fanseq;
    uint8_t fanmode;
    uint32_t fanspeed;
} UdpFanInfo;

typedef struct __attribute__((packed))
{
    uint8_t sensorseq;
    uint8_t sensortype;
    uint8_t sensorname[6];
    uint8_t sensorvalue_L;
    uint8_t sensorvalue_H;
    uint8_t sensoralmtype;
    uint8_t sensorresv;
} UdpSensorInfo;

typedef struct __attribute__((packed))
{
    uint8_t ipmbaddr;
    uint16_t moduletype;
    uint16_t bmccompany;
    uint8_t bmcversion[8];
    uint8_t sensornum;
    UdpSensorInfo sensor[5];
    uint8_t resv[2];
} UdpBoardInfo;

typedef struct __attribute__((packed))
{
    uint16_t head;
    uint16_t msglenth;
    uint16_t seqnum;
    uint16_t msgtype;
    uint32_t timestamp;
    uint8_t recv[4];
    uint8_t boxname;
    uint8_t boxid;
    UdpFanInfo fan[2];
    UdpBoardInfo board[14];
    uint16_t tail;
} UdpInfo;

// C-style interface for backward compatibility
int bmc_listener_init(const char* group_ip, uint16_t port);
void bmc_listener_start();
void bmc_listener_stop();
void bmc_listener_set_callback(const std::function<void(const std::string&)>& callback);
void bmc_listener_cleanup();

#endif // BMC_LISTENER_H