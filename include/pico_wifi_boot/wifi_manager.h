#ifndef __PICO_WIFI_BOOT_WIFI_MANAGER_H__
#define __PICO_WIFI_BOOT_WIFI_MANAGER_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void print_current_ipv4();

bool wifi_manager_init(bool enable_powersave);

void wifi_manager_connect_async();

bool wifi_manager_connect(int attempts);

bool wifi_manager_attempt_configure();

void wifi_manager_configure();

#ifdef __cplusplus
} // extern "C"
#endif

#endif
