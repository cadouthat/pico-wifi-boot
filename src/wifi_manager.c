#include "pico_wifi_boot/wifi_manager.h"

#include <string.h>

#include "pico/stdio.h"

#include "cyw43.h"
#include "cyw43_config.h"
#include "cyw43_ll.h"
#include "lwip/netif.h"

#include "pico_wifi_boot/flash.h"

#define SERIAL_INPUT_TIMEOUT_US (30 * 1000 * 1000)
#define SERIAL_INPUT_END '\r'

// Forward-declare some functions we depend on from pico_cyw43_arch, since we do not know the required
// arch type to include pico/cyw43_arch.h
void cyw43_arch_enable_sta_mode(void);
int cyw43_arch_wifi_connect_timeout_ms(const char *ssid, const char *pw, uint32_t auth, uint32_t timeout);

void print_current_ipv4() {
    cyw43_thread_enter();

    printf("IPv4 address: %s\n", ipaddr_ntoa(netif_ip4_addr(netif_default)));

    cyw43_thread_exit();
}

bool wifi_connect(int attempts, bool enable_powersave) {
    char ssid[WIFI_CONFIG_SSID_SIZE + 1] = {0};
    char pass[WIFI_CONFIG_PASS_SIZE + 1] = {0};

    if (!read_wifi_config(ssid, pass) || !ssid[0]) {
        printf("WiFi is not configured\n");
        return false;
    }

    cyw43_arch_enable_sta_mode();

    if (!enable_powersave &&
        cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1)) != 0) {
        printf("cyw43_wifi_pm failed\n");
        return false;
    }

    for (int attempt = 0; attempt < attempts; attempt++) {
        printf("Connecting to %s\n", ssid);
        if (cyw43_arch_wifi_connect_timeout_ms(ssid, pass, CYW43_AUTH_WPA2_AES_PSK, 30000) == 0) {
            print_current_ipv4();
            return true;
        }
        printf("Failed to connect\n");
    }

    return false;
}

bool prompt(const char* message, char* buf, int buf_size) {
    printf("%s", message);

    int len = 0;
    int c;
    while (true) {
        c = getchar_timeout_us(SERIAL_INPUT_TIMEOUT_US);
        if (c == SERIAL_INPUT_END || c == PICO_ERROR_TIMEOUT) {
            break;
        }
        if (!c) {
            // Ignore NULL inputs
            continue;
        }
        putchar(c);
        if (len >= buf_size - 1) {
            break;
        }
        buf[len++] = c;
    }
    buf[len] = 0;
    printf("\n");

    if (c == PICO_ERROR_TIMEOUT) {
        printf("Input timeout\n");
        return false;
    }
    if (c != SERIAL_INPUT_END) {
        printf("Entry exceeds max length of %d characters\n", buf_size - 1);
        return false;
    }

    return true;
}

bool attempt_wifi_configure() {
    char ssid[WIFI_CONFIG_SSID_SIZE + 1] = {0};
    char pass[WIFI_CONFIG_PASS_SIZE + 1] = {0};
    
    if (!prompt("WiFi SSID: ", ssid, sizeof(ssid))) {
        return false;
    }
    if (!prompt("WiFi pass: ", pass, sizeof(pass))) {
        return false;
    }

    if (!write_wifi_config(ssid, pass)) {
        printf("Failed to write config to flash\n");
        return false;
    }

    return true;
}

void wifi_configure() {
    while (!attempt_wifi_configure()) {}
}
