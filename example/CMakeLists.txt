cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico_w)

include(../pico_sdk_import.cmake)

add_subdirectory(.. pico-wifi-boot)

project(pico_wifi_boot_blink_project C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_library(lwipopts_provider INTERFACE)
target_include_directories(lwipopts_provider INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

add_executable(main
  src/main.c
)

pico_enable_stdio_usb(main 1)

target_include_directories(main PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include )

target_link_libraries(main
  pico_cyw43_arch_lwip_poll
  pico_stdlib
  pico_time
  pico_wifi_boot
)

wifi_boot_user_program_bin(main)
