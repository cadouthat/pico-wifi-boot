#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "pico_wifi_boot/ota_server.h"
#include "pico_wifi_boot/reboot.h"
#include "pico_wifi_boot/wifi_manager.h"

bool wifi_init() {
    if (cyw43_arch_init() != 0) {
        printf("cyw43 init failed\n");
        return false;
    }

    cyw43_arch_enable_sta_mode();

    // TODO: provide some way to force configuration mode
    while (!wifi_connect(3)) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(1000);
        wifi_configure();
    }

    print_current_ipv4();

    return true;
}

void blink_forever(int delay) {
    while (true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(delay);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(delay);
    }
}

int main() {
    if (!validate_bootloader_size()) {
        blink_forever(100);
    }

    gpio_init(BOOT_OVERRIDE_PIN);
    gpio_pull_up(BOOT_OVERRIDE_PIN);
    gpio_set_dir(BOOT_OVERRIDE_PIN, GPIO_IN);

    // Give the pull-up some time to settle out
    sleep_ms(1);

    if (!bootloader_requested()) {
        load_user_program();
        return 0;
    }

    stdio_init_all();

    if (!wifi_init()) {
        return 1;
    }

    if (!ota_init(OTA_PORT)) {
        return 1;
    }

    blink_forever(500);
}
