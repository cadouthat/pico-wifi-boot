#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

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

int main() {
    stdio_init_all();

    if (!wifi_init()) {
        return 1;
    }

    if (!ota_init(OTA_PORT)) {
        return 1;
    }

    while (true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(5000);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(5000);
    }
}
