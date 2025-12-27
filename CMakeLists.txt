# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD weact_studio_rp2350b_core CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(nisem5cart C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(nisem5cart nisem5cart.c )

target_sources(nisem5cart PRIVATE nisem5cart.c vga16_graphics.c tms9918/vrEmuTms9918.c tms9918/vrEmuTms9918Util.c)

pico_set_program_name(nisem5cart "nisem5cart")
pico_set_program_version(nisem5cart "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(nisem5cart 1)
pico_enable_stdio_usb(nisem5cart 0)

pico_generate_pio_header(nisem5cart ${CMAKE_CURRENT_LIST_DIR}/hsync.pio)
pico_generate_pio_header(nisem5cart ${CMAKE_CURRENT_LIST_DIR}/vsync.pio)
pico_generate_pio_header(nisem5cart ${CMAKE_CURRENT_LIST_DIR}/rgb.pio)

# Add the standard library to the build
target_link_libraries(nisem5cart  
        hardware_flash hardware_vreg hardware_pio hardware_dma pico_multicore pico_stdlib)

# Add the standard include files to the build
target_include_directories(nisem5cart PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

#pico_set_binary_type(nisem5cart no_flash)

pico_add_extra_outputs(nisem5cart)

