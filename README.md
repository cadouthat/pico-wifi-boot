# pico-wifi-boot
L2 bootloader for the Raspberry Pi Pico W which provides the ability to flash user programs wirelessly over TCP.
WiFi credentials are stored in flash, and the provided library allows user programs to connect to WiFi using the same credentials.

An example user program is [provided here](example/).

## Building
These targets require users to provide a CMake library named `lwipopts_provider` which exposes the `lwipopts.h` header for lwIP configuration.

The [bootloader](/CMakeLists.txt) is a typical Pico SDK executable, buildable with CMake (with the lwIP caveat above).

User programs (binaries intended to be used with this bootloader) must be built using the provided `wifi_boot_user_program_bin` CMake function ([see example](example/CMakeLists.txt)).
This uses customized linker settings to work with the offset where the binary will be loaded in flash.

## Flashing
1. The `bootloader` binary should be flashed onto the Pico using normal methods. This binary also contains the L1 bootloader from the SDK
1. Reboot while holding GPIO 15 low, which will prevent the bootloader from jumping into uninitialized user program space
1. The bootloader will enter programming mode, and will prompt for WiFi credentials via serial. After successful configuration, the credentials are stored in flash
1. The bootloader will wait for a user program to be uploaded (using the [upload tool](upload_tool/)), and will automatically reboot into the user program
1. Once loaded, user programs may utilize the provided [OTA server](include/pico_wifi_boot/ota_server.h) to enable rebooting into the bootloader wirelessly
