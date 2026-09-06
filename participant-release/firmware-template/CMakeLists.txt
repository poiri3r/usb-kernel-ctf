cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(kapo_firmware C CXX ASM)

pico_sdk_init()

add_executable(kapo_firmware main.c)
set_target_properties(kapo_firmware PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(kapo_firmware PRIVATE pico_stdlib hardware_irq hardware_resets)
pico_enable_stdio_usb(kapo_firmware 0)
pico_enable_stdio_uart(kapo_firmware 0)
pico_add_extra_outputs(kapo_firmware)
