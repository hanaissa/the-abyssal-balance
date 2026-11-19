cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico CACHE STRING "Pico board for the motor bench test")
include(../../pico_sdk_import.cmake)
project(AbyssalPumpBench C CXX ASM)
pico_sdk_init()

add_executable(Pump_Bench
    bench.c
    ../../drivers/motor/Pump.c
    ../../drivers/motor/L298N.c
)
target_compile_features(Pump_Bench PRIVATE c_std_11)
target_include_directories(Pump_Bench PRIVATE ../../include ../../drivers/motor)
target_link_libraries(Pump_Bench PRIVATE pico_stdlib hardware_clocks hardware_pwm)
pico_enable_stdio_usb(Pump_Bench 1)
pico_enable_stdio_uart(Pump_Bench 0)
pico_add_extra_outputs(Pump_Bench)
