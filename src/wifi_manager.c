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
int cyw43_arch_wifi_connect_async(const char *ssid, const char *pw, uint32_t auth);

bool wifi_manager_config_stale = false;

void print_current_ipv4() {
    cyw43_thread_enter();

    printf("IPv4 address: %s\n", ipaddr_ntoa(netif_ip4_addr(netif_default)));

    cyw43_thread_exit();
}

bool wifi_manager_init(bool enable_powersave) {
    cyw43_arch_enable_sta_mode();

    if (!enable_powersave &&
        cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1)) != 0) {
        printf("cyw43_wifi_pm failed\n");
        return false;
    }

    return true;
}

void wifi_manager_connect_async() {
    static uint32_t wait_until_ms = 0;
    static bool is_connecting = false;

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if (wait_until_ms > now_ms) {
        return;
    }

    int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (status == CYW43_LINK_UP && !wifi_manager_config_stale) {
        if (is_connecting) {
            is_connecting = false;
            print_current_ipv4();
        }
        wait_until_ms = now_ms + 1000;
        return;
    }

    if (status < 0 && is_connecting) {
        printf("Failed to connect (%d)\n", status);
        is_connecting = false;
        wait_until_ms = now_ms + 5000;
        return;
    }

    if (!is_connecting || wifi_manager_config_stale) {
        char ssid[WIFI_CONFIG_SSID_SIZE + 1] = {0};
        char pass[WIFI_CONFIG_PASS_SIZE + 1] = {0};
        if (!read_wifi_config(ssid, pass) || !ssid[0]) {
            printf("WiFi is not configured\n");
            wait_until_ms = now_ms + 5000;
            return;
        }

        if (cyw43_arch_wifi_connect_async(ssid, pass, CYW43_AUTH_WPA2_AES_PSK) != 0) {
            printf("cyw43_arch_wifi_connect_async failed\n");
            wait_until_ms = now_ms + 5000;
            return;
        }

        printf("Connecting to %s\n", ssid);
        is_connecting = true;
        wifi_manager_config_stale = false;
    }
}

bool wifi_manager_connect(int attempts) {
    char ssid[WIFI_CONFIG_SSID_SIZE + 1] = {0};
    char pass[WIFI_CONFIG_PASS_SIZE + 1] = {0};

    if (!read_wifi_config(ssid, pass) || !ssid[0]) {
        printf("WiFi is not configured\n");
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

bool wifi_manager_is_connected() {
    return cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP;
}

bool wifi_manager_attempt_configure() {
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

    wifi_manager_config_stale = true;

    return true;
}

void wifi_manager_configure() {
    while (!wifi_manager_attempt_configure()) {}
}
