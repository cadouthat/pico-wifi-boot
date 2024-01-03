#include "pico_wifi_boot/flash.h"

#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/multicore.h"

#include "pico_wifi_boot/sniffer_crc32.h"

void write_flash_sector(uint32_t sector_offset, uint8_t* data) {
    // If both cores are running, the other core must be locked out to prevent flash XIP access
    // Note: multicore_lockout_victim_init() must have been called on the other core in this case
    uint other_core_num = get_core_num() ? 0 : 1;
    bool core_lockout_available = multicore_lockout_victim_is_initialized(other_core_num);
    if (core_lockout_available) {
        multicore_lockout_start_blocking();
    }

    do {
        // Disable interrupts to avoid flash XIP access
        uint32_t saved = save_and_disable_interrupts();
        flash_range_erase(sector_offset, FLASH_SECTOR_SIZE);
        flash_range_program(sector_offset, data, FLASH_SECTOR_SIZE);
        restore_interrupts(saved);
    } while (memcmp(data, (uint8_t*)XIP_BASE + sector_offset, FLASH_SECTOR_SIZE) != 0);

    if (core_lockout_available) {
        multicore_lockout_end_blocking();
    }
}

bool read_wifi_config(char* ssid, char* pass) {
    uint8_t* read_from = (uint8_t*)XIP_BASE + CONFIG_FLASH_OFFSET;

    if (memcmp(read_from, CONFIG_MAGIC_CODE, CONFIG_MAGIC_CODE_LEN) != 0) {
        return false;
    }
    read_from += CONFIG_MAGIC_CODE_LEN;

    memcpy(ssid, read_from, WIFI_CONFIG_SSID_SIZE);
    read_from += WIFI_CONFIG_SSID_SIZE;

    memcpy(pass, read_from, WIFI_CONFIG_PASS_SIZE);
    read_from += WIFI_CONFIG_PASS_SIZE;

    return true;
}

bool read_flash_config_extra(void* extra, uint16_t size) {
    if (size > FLASH_CONFIG_EXTRA_MAX_SIZE) {
        return false;
    }

    uint8_t* read_from = (uint8_t*)XIP_BASE + CONFIG_FLASH_OFFSET;

    if (memcmp(read_from, CONFIG_MAGIC_CODE, CONFIG_MAGIC_CODE_LEN) != 0) {
        return false;
    }
    read_from += CONFIG_MAGIC_CODE_LEN;

    read_from += WIFI_CONFIG_SSID_SIZE;
    read_from += WIFI_CONFIG_PASS_SIZE;

    uint32_t stored_crc;
    memcpy(&stored_crc, read_from, sizeof(stored_crc));
    read_from += sizeof(stored_crc);

    if (stored_crc != sniffer_crc32(read_from, size)) {
        return false;
    }

    memcpy(extra, read_from, size);
    read_from += size;

    return true;
}

uint8_t *init_write_buffer(uint8_t* sector) {
    memcpy(sector, (uint8_t*)XIP_BASE + CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);

    if (memcmp(sector, CONFIG_MAGIC_CODE, CONFIG_MAGIC_CODE_LEN) != 0) {
        memset(sector, 0, sizeof(sector));
        memcpy(sector, CONFIG_MAGIC_CODE, CONFIG_MAGIC_CODE_LEN);
    }

    return sector + CONFIG_MAGIC_CODE_LEN;
}

bool write_wifi_config(char *ssid, char* pass) {
    // Get space on the heap to avoid large stack vars
    uint8_t* sector = malloc(FLASH_SECTOR_SIZE);
    if (!sector) {
        return false;
    }

    uint8_t *write_to = init_write_buffer(sector);

    memcpy(write_to, ssid, MIN(strlen(ssid) + 1, WIFI_CONFIG_SSID_SIZE));
    write_to += WIFI_CONFIG_SSID_SIZE;

    memcpy(write_to, pass, MIN(strlen(pass) + 1, WIFI_CONFIG_PASS_SIZE));
    write_to += WIFI_CONFIG_PASS_SIZE;

    write_flash_sector(CONFIG_FLASH_OFFSET, sector);

    free(sector);
    return true;
}

bool write_flash_config_extra(void *extra, uint16_t size) {
    if (size > FLASH_CONFIG_EXTRA_MAX_SIZE) {
        return false;
    }

    // Get space on the heap to avoid large stack vars
    uint8_t* sector = malloc(FLASH_SECTOR_SIZE);
    if (!sector) {
        return false;
    }

    uint8_t *write_to = init_write_buffer(sector);

    write_to += WIFI_CONFIG_SSID_SIZE;
    write_to += WIFI_CONFIG_PASS_SIZE;

    uint32_t crc32 = sniffer_crc32(extra, size);
    memcpy(write_to, &crc32, sizeof(crc32));
    write_to += sizeof(crc32);

    memcpy(write_to, extra, size);
    write_to += size;

    write_flash_sector(CONFIG_FLASH_OFFSET, sector);

    free(sector);
    return true;
}
