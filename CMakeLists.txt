cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(PICO_BOARD pico2 CACHE STRING "Board type")
set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib/FreeRTOS-Kernel)

include(pico_sdk_import.cmake)
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake)

project(dsp_examples C CXX ASM)
pico_sdk_init()

include(FetchContent)

# ── CMSIS-DSP (ARM optimized, ใช้ DSP SIMD instructions ของ Cortex-M33) ───────
FetchContent_Declare(
    CMSISDSP
    GIT_REPOSITORY https://github.com/ARM-software/CMSIS-DSP.git
    GIT_TAG        v1.15.0
)
FetchContent_MakeAvailable(CMSISDSP)
target_link_libraries(CMSISDSP PUBLIC cmsis_core)
target_compile_options(CMSISDSP PRIVATE -Os -fno-if-conversion)

add_executable(dsp_examples
    src/main.c
)

target_include_directories(dsp_examples PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(dsp_examples
    pico_stdlib
    pico_float
    FreeRTOS-Kernel-Heap4
    CMSISDSP
    cmsis_core
)

target_link_options(${PROJECT_NAME} PRIVATE
    -Wl,--print-memory-usage
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
)


pico_enable_stdio_uart(dsp_examples 0)
pico_enable_stdio_usb(dsp_examples 1)

pico_add_extra_outputs(dsp_examples)
