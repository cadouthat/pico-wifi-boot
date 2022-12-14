#ifndef __PICO_WIFI_BOOT_FLASH_H__
#define __PICO_WIFI_BOOT_FLASH_H__

#include <stdint.h>
#include <stdbool.h>

#include "hardware/flash.h"

// Config flash size is one sector
#define CONFIG_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE) // Sector aligned
#define CONFIG_MAGIC_CODE "CNF\n"
#define CONFIG_MAGIC_CODE_LEN 4
#define WIFI_CONFIG_SSID_SIZE 32
#define WIFI_CONFIG_PASS_SIZE 64
#define EXTRA_CONFIG_MAX_SIZE (FLASH_SECTOR_SIZE - CONFIG_MAGIC_CODE_LEN - WIFI_CONFIG_SSID_SIZE - WIFI_CONFIG_SSID_SIZE - sizeof(uint16_t))

// Note: this needs to match the linker script offset to build user programs
#define BOOTLOADER_RESERVED_FLASH_SIZE (352 * 1024) // Sector aligned
#define USER_PROGRAM_OFFSET BOOTLOADER_RESERVED_FLASH_SIZE
#define USER_PROGRAM_MAX_SIZE (PICO_FLASH_SIZE_BYTES - BOOTLOADER_RESERVED_FLASH_SIZE - FLASH_SECTOR_SIZE)

#ifdef __cplusplus
extern "C" {
#endif

void write_sector(uint32_t sector_offset, uint8_t* data);

bool read_wifi_config(char* ssid, char* pass);
bool read_extra_config(void *extra, uint16_t size);

void write_wifi_config(char *ssid, char* pass);
bool write_extra_config(void *extra, uint16_t size);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
