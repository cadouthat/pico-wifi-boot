#include "pico_wifi_boot/reboot.h"

#include "RP2040.h"
#include "hardware/watchdog.h"
#include "hardware/structs/scb.h"
#include "pico/stdlib.h"

#include "pico_wifi_boot/flash.h"

extern char __flash_binary_end;

#define BOOT_WATCHDOG_MAGIC 0x307A6EB0

bool bootloader_requested() {
    if (gpio_get(BOOT_OVERRIDE_PIN) == 0 ||
        (watchdog_hw->scratch[0] == BOOT_WATCHDOG_MAGIC &&
            watchdog_hw->scratch[1] == ~BOOT_WATCHDOG_MAGIC)) {
        watchdog_hw->scratch[0] = 0;
        watchdog_hw->scratch[1] = 0;
        return true;
    }

    return false;
}

void reboot() {
    watchdog_reboot(0, 0, 0);
    while (1) { tight_loop_contents(); }
}

void reboot_into_bootloader() {
    watchdog_hw->scratch[0] = BOOT_WATCHDOG_MAGIC;
    watchdog_hw->scratch[1] = ~BOOT_WATCHDOG_MAGIC;
    reboot();
}

bool running_in_bootloader() {
    return (uint32_t)&__flash_binary_end <= XIP_BASE + USER_PROGRAM_OFFSET;
}

bool validate_bootloader_size() {
    return (uint32_t)&__flash_binary_end - XIP_BASE <= USER_PROGRAM_OFFSET;
}

void load_user_program() {
    uint32_t* vt = (uint32_t*)(XIP_BASE + USER_PROGRAM_OFFSET);
    uint32_t stack_end = vt[0];
    uint32_t reset_handler = vt[1];

    // Disable interrupts and clear pending
    NVIC->ICER[0] = 0xFFFFFFFF;
    NVIC->ICPR[0] = 0xFFFFFFFF;

    // Point the VTOR register to the user program vector table
    scb_hw->vtor = (uint32_t)vt;

    // Initialize the stack pointer and jump to reset handler
    asm volatile(
        "msr msp, %0\n\t"
        "bx %1\n\t"
        ::"r"(stack_end), "r"(reset_handler));
}
