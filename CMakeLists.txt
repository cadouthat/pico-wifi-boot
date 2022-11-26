cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pico_wifi_boot_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_library(pico_wifi_boot
  src/flash.c
  src/ota_server.c
  src/reboot.c
  src/sniffer_crc32.c
  src/wifi_manager.c)

target_include_directories(pico_wifi_boot PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(pico_wifi_boot PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src )

target_link_libraries(pico_wifi_boot
  pico_cyw43_arch_lwip_threadsafe_background
  pico_stdlib
  hardware_flash
  hardware_sync
  cmsis_core
)

add_executable(bootloader
  src/bootloader.c
)

pico_enable_stdio_usb(bootloader 1)
pico_enable_stdio_uart(bootloader 1)

pico_add_extra_outputs(bootloader)

target_include_directories(bootloader PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src )

target_link_libraries(bootloader
  pico_wifi_boot
  pico_cyw43_arch_lwip_threadsafe_background
  pico_stdlib
)

function(wifi_boot_user_program_bin NAME)
  pico_set_linker_script(${NAME} ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/memmap_offset_flash.ld)
  pico_add_bin_output(${NAME})
endfunction()
