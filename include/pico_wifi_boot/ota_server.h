#ifndef __PICO_WIFI_BOOT_OTA_SERVER_H__
#define __PICO_WIFI_BOOT_OTA_SERVER_H__

#include <stdint.h>

#include "lwip/tcp.h"

#ifndef OTA_PORT
#define OTA_PORT 2222
#endif

struct tcp_pcb* ota_init(uint16_t port);

#endif
