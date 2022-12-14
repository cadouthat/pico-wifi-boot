#include "pico_wifi_boot/wifi_manager.h"

#include <stdio.h>

#include "pico/cyw43_arch.h"

#include "pico_wifi_boot/flash.h"

#define SERIAL_INPUT_END '\r'

void print_current_ipv4() {
    cyw43_arch_lwip_begin();

    printf("IPv4 address: %s\n", ipaddr_ntoa(netif_ip4_addr(netif_default)));

    cyw43_arch_lwip_end();
}

bool wifi_connect() {
    char ssid[WIFI_CONFIG_SSID_SIZE + 1] = {0};
    char pass[WIFI_CONFIG_PASS_SIZE + 1] = {0};

    if (!read_wifi_config(ssid, pass) || !ssid[0]) {
        printf("WiFi is not configured\n");
        return false;
    }

    printf("Connecting to %s\n", ssid);
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, pass, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Failed to connect\n");
        return false;
    }

    return true;
}

bool prompt(const char* message, char* buf, int buf_size) {
    printf("%s", message);

    int len = 0;
    char c;
    while (true) {
        c = getchar();
        if (!c || c == SERIAL_INPUT_END) {
            break;
        }
        putchar(c);
        if (len >= buf_size - 1) {
            break;
        }
        buf[len++] = c;
    }
    buf[len] = 0;
    printf("\n");

    if (!c) {
        printf("\nNULL input detected\n");
        return false;
    }
    if (c != SERIAL_INPUT_END) {
        printf("Entry exceeds max length of %d characters\n", buf_size - 1);
        return false;
    }

    return true;
}

void wifi_configure() {
    char ssid[WIFI_CONFIG_SSID_SIZE + 1] = {0};
    char pass[WIFI_CONFIG_PASS_SIZE + 1] = {0};
    
    while (!prompt("WiFi SSID: ", ssid, sizeof(ssid)));
    while (!prompt("WiFi pass: ", pass, sizeof(pass)));

    write_wifi_config(ssid, pass);
}
