#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"

#include "pico_wifi_boot/ota_server.h"
#include "pico_wifi_boot/wifi_manager.h"

bool wifi_init() {
    if (cyw43_arch_init() != 0) {
        printf("cyw43 init failed\n");
        return false;
    }

    cyw43_arch_enable_sta_mode();

    if (!wifi_connect(1)) {
        return false;
    }

    return true;
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
    stdio_init_all();

    if (!wifi_init()) {
        while (1) tight_loop_contents();
    }

    if (!ota_init(OTA_PORT)) {
        while (1) tight_loop_contents();
    }

    while (true) {
        cyw43_arch_poll();
        blink_poll(5000);
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
    }
}
