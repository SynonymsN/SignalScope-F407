cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary.
#

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_PROJECT_NAME SignalScope_F407)

include("cmake/gcc-arm-none-eabi.cmake")

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

enable_language(C ASM)

add_executable(${CMAKE_PROJECT_NAME})

add_subdirectory(cmake/stm32cubemx)

# ---- LVGL v8.2 integration ----
# LVGL needs Core/Inc so lv_conf.h can be found by the library target.
set(LV_CONF_PATH "${CMAKE_SOURCE_DIR}/Core/Inc/lv_conf.h" CACHE STRING "" FORCE)
add_subdirectory(lvgl-release-v8.2)

# lv_conf.h uses STM32 HAL symbols for LV_TICK_CUSTOM.
target_include_directories(lvgl PUBLIC
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)
target_compile_definitions(lvgl PUBLIC USE_HAL_DRIVER STM32F407xx)

target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/lvgl-release-v8.2
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    lvgl
)
