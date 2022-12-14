#include "pico_wifi_boot/flash.h"

#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"

void write_sector(uint32_t sector_offset, uint8_t* data) {
    uint32_t saved = save_and_disable_interrupts();
    flash_range_erase(sector_offset, FLASH_SECTOR_SIZE);
    flash_range_program(sector_offset, data, FLASH_SECTOR_SIZE);
    restore_interrupts(saved);
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

bool read_extra_config(void* extra, uint16_t size) {
    if (size > EXTRA_CONFIG_MAX_SIZE) {
        return false;
    }

    uint8_t* read_from = (uint8_t*)XIP_BASE + CONFIG_FLASH_OFFSET;

    if (memcmp(read_from, CONFIG_MAGIC_CODE, CONFIG_MAGIC_CODE_LEN) != 0) {
        return false;
    }
    read_from += CONFIG_MAGIC_CODE_LEN;

    read_from += WIFI_CONFIG_SSID_SIZE;
    read_from += WIFI_CONFIG_PASS_SIZE;

    if (memcmp(read_from, &size, sizeof(size)) != 0) {
        return false;
    }
    read_from += sizeof(size);

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

void write_wifi_config(char *ssid, char* pass) {
    uint8_t sector[FLASH_SECTOR_SIZE];
    uint8_t *write_to = init_write_buffer(sector);

    memcpy(write_to, ssid, MIN(strlen(ssid) + 1, WIFI_CONFIG_SSID_SIZE));
    write_to += WIFI_CONFIG_SSID_SIZE;

    memcpy(write_to, pass, MIN(strlen(pass) + 1, WIFI_CONFIG_PASS_SIZE));
    write_to += WIFI_CONFIG_PASS_SIZE;

    write_sector(CONFIG_FLASH_OFFSET, sector);
}

bool write_extra_config(void *extra, uint16_t size) {
    if (size > EXTRA_CONFIG_MAX_SIZE) {
        return false;
    }

    uint8_t sector[FLASH_SECTOR_SIZE];
    uint8_t *write_to = init_write_buffer(sector);

    write_to += WIFI_CONFIG_SSID_SIZE;
    write_to += WIFI_CONFIG_PASS_SIZE;

    memcpy(write_to, &size, sizeof(size));
    write_to += sizeof(size);

    memcpy(write_to, extra, size);
    write_to += size;

    write_sector(CONFIG_FLASH_OFFSET, sector);
}
