#                   *****  remote  *****

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)

set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)

# Pull in Raspberry Pi Pico SDK (must be before project)
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

if (${USE-HTTPS} STREQUAL "true")
	message("Building https version")
	add_definitions(-DUSE_HTTPS)
	set(CERTS data/newcert.pem data/newkey.pem)
else()
	message("Building http version")
endif()

# Add executable. Default name is the project name, version 0.1
project(remote C CXX ASM)

# Add libraries
add_subdirectory(${PICO_SDK_PATH}/../pico-filesystem pfs)
add_subdirectory(${PICO_SDK_PATH}/../tiny-json tiny-json)

add_executable(${PROJECT_NAME} 
    remote.cpp
	remotefile.cpp
	jsonstring.cpp
	)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/dhcpserver)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/build/generated)

pico_set_program_name(${PROJECT_NAME} "remote")
pico_set_program_version(${PROJECT_NAME} "0.1")

pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

target_link_libraries(${PROJECT_NAME} PUBLIC pico_stdlib 
    pico_cyw43_arch_lwip_threadsafe_background pico_lwip_mbedtls pico_mbedtls pico_lwip_mdns
    flash_filesystem tiny-json)

pico_add_extra_outputs(${PROJECT_NAME})

