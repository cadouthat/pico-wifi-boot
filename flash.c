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

bool read_config_flash(char* ssid, char* pass) {
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

void write_config_flash(char *ssid, char* pass) {
    uint8_t sector[FLASH_SECTOR_SIZE] = {0};
    uint8_t *write_to = sector;

    memcpy(write_to, CONFIG_MAGIC_CODE, CONFIG_MAGIC_CODE_LEN);
    write_to += CONFIG_MAGIC_CODE_LEN;

    memcpy(write_to, ssid, strlen(ssid));
    write_to += WIFI_CONFIG_SSID_SIZE;

    memcpy(write_to, pass, strlen(pass));
    write_to += WIFI_CONFIG_PASS_SIZE;

    write_sector(CONFIG_FLASH_OFFSET, sector);
}
