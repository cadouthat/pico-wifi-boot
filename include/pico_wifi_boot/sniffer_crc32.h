#ifndef __PICO_WIFI_BOOT_SNIFFER_CRC32_H__
#define __PICO_WIFI_BOOT_SNIFFER_CRC32_H__

#include <stdint.h>

uint32_t sniffer_crc32(uint8_t* aligned_addr, uint32_t len);

#endif
