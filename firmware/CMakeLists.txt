cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(kapo_rp2040 C CXX ASM)

pico_sdk_init()

add_executable(kapo_rp2040
    main.c
    kapo_protocol.c
)

set_target_properties(kapo_rp2040 PROPERTIES LINKER_LANGUAGE C)

target_link_libraries(kapo_rp2040 PRIVATE
    pico_stdlib
    hardware_irq
    hardware_resets
)

pico_enable_stdio_usb(kapo_rp2040 0)
pico_enable_stdio_uart(kapo_rp2040 0)
pico_add_extra_outputs(kapo_rp2040)
