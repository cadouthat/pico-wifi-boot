#ifndef __PICO_WIFI_BOOT_FLASH_H__
#define __PICO_WIFI_BOOT_FLASH_H__

#include <stdint.h>
#include <stdbool.h>

#define CONFIG_FLASH_SIZE (4 * 1024) // Sector aligned
#define CONFIG_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - CONFIG_FLASH_SIZE) // Sector aligned
#define CONFIG_MAGIC_CODE "CNF\n"
#define CONFIG_MAGIC_CODE_LEN 4
#define WIFI_CONFIG_SSID_SIZE 32
#define WIFI_CONFIG_PASS_SIZE 64

// Note: this needs to match the linker script offset to build user programs
#define BOOTLOADER_RESERVED_FLASH_SIZE (352 * 1024) // Sector aligned
#define USER_PROGRAM_OFFSET BOOTLOADER_RESERVED_FLASH_SIZE
#define USER_PROGRAM_MAX_SIZE (PICO_FLASH_SIZE_BYTES - BOOTLOADER_RESERVED_FLASH_SIZE - CONFIG_FLASH_SIZE)

#ifdef __cplusplus
extern "C" {
#endif

void write_sector(uint32_t sector_offset, uint8_t* data);

bool read_config_flash(char* ssid, char* pass);

void write_config_flash(char *ssid, char* pass);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
