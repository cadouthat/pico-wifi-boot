#ifndef __PICO_WIFI_BOOT_REBOOT_H__
#define __PICO_WIFI_BOOT_REBOOT_H__

#include <stdbool.h>

#ifndef BOOT_OVERRIDE_PIN
#define BOOT_OVERRIDE_PIN 15
#endif

#ifdef __cplusplus
extern "C" {
#endif

bool bootloader_requested();

void reboot();

void reboot_into_bootloader();

bool running_in_bootloader();

bool validate_bootloader_size();

void load_user_program();

#ifdef __cplusplus
} // extern "C"
#endif

#endif
