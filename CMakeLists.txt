cmake_minimum_required(VERSION 3.13)
project(AbyssalBalance C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
option(ABYSSAL_USE_PICO_SDK "Build against the Raspberry Pi Pico SDK" OFF)
if(ABYSSAL_USE_PICO_SDK)
    if(NOT DEFINED PICO_SDK_PATH)
        message(FATAL_ERROR "Set PICO_SDK_PATH when ABYSSAL_USE_PICO_SDK=ON")
    endif()
    include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
    pico_sdk_init()
endif()
set(ABYSSAL_SOURCES
    src/StateMachine.c
    drivers/rfid/RC522.c drivers/water/WaterLevel.c
    drivers/potentiometer/Potentiometer.c
    drivers/motor/L298N.c drivers/motor/Pump.c
    drivers/display/LCD1602.c drivers/buttons/Buttons.c
    drivers/uv/UV_LED.c drivers/camera/Camera.c drivers/camera/OCR.c)
add_library(abyssal_core ${ABYSSAL_SOURCES})
target_include_directories(abyssal_core PUBLIC include src)
add_executable(abyssal_balance src/main.c)
target_link_libraries(abyssal_balance PRIVATE abyssal_core)
if(ABYSSAL_USE_PICO_SDK)
    target_link_libraries(abyssal_core PUBLIC pico_stdlib hardware_adc hardware_i2c hardware_spi hardware_pwm)
    target_link_libraries(abyssal_balance PRIVATE pico_stdlib)
    pico_add_extra_outputs(abyssal_balance)
endif()
enable_testing()
add_subdirectory(tests)
