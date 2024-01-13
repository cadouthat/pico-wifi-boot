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

    // TODO: provide some way to force configuration mode
    while (!wifi_connect(/*attempts=*/ 3, /*enable_powersave=*/ false)) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(1000);
        wifi_configure();
    }

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

void blink_poll(int delay) {
    static bool is_on = false;
    static uint32_t next_toggle_ms = 0;

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if (now_ms >= next_toggle_ms) {
        next_toggle_ms = now_ms + delay;

        is_on = !is_on;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, is_on);
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
        while (1) tight_loop_contents();
    }

    if (!ota_init(OTA_PORT)) {
        while (1) tight_loop_contents();
    }

    while (1) {
        cyw43_arch_poll();
        blink_poll(500);
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(500));
    }
}
