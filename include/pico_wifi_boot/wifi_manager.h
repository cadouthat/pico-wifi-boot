#ifndef __PICO_WIFI_BOOT_WIFI_MANAGER_H__
#define __PICO_WIFI_BOOT_WIFI_MANAGER_H__

#include <stdbool.h>

void print_current_ipv4();

bool wifi_connect();

void wifi_configure();

#endif
