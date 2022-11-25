#ifndef __PICO_WIFI_BOOT_REBOOT_H__
#define __PICO_WIFI_BOOT_REBOOT_H__

#include <stdbool.h>

#ifndef BOOT_OVERRIDE_PIN
#define BOOT_OVERRIDE_PIN 15
#endif

bool bootloader_requested();

void reboot();

void reboot_into_bootloader();

bool running_in_bootloader();

bool validate_bootloader_size();

void load_user_program();

#endif
