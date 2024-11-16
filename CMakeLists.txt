#                   *****  remote  *****

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)

set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)

#set(PICO_LWIP_PATH ${PICO_SDK_PATH}/../lwip)

# Pull in Raspberry Pi Pico SDK (must be before project)
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

# Add executable. Default name is the project name, version 0.1
project(remote C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add libraries
add_subdirectory(${PICO_SDK_PATH}/../pico-filesystem pfs)
add_subdirectory(${PICO_SDK_PATH}/../tiny-json tiny-json)

# Add the utility libraries
add_subdirectory(../picolibs picolibs)

add_executable(${PROJECT_NAME} 
    remote.cpp
	remote_remote.cpp remote_backup.cpp remote_setup.cpp remote_menu.cpp
	remote_cleanup.cpp remote_config.cpp remote_test.cpp remote_log.cpp
	remote_watchdog.cpp
	remotefile.cpp
	menu.cpp
	irprocessor.cpp
	irdevice.cpp
	command.cpp
	config.cpp
	backup.cpp
	)

pico_set_program_name(${PROJECT_NAME} "remote")
pico_set_program_version(${PROJECT_NAME} "0.1")

pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

target_link_libraries(${PROJECT_NAME} PUBLIC pico_stdlib 
    pico_cyw43_arch_lwip_threadsafe_background
    flash_filesystem tiny-json
	bgr_webserver bgr_ir_protocols bgr_util bgr_json
	hardware_watchdog)

pico_add_extra_outputs(${PROJECT_NAME})

set(WEB_RESOURCE_FILES
 	data/index.html data/webremote.js
 	data/backup.html data/backup.js
	data/config.html data/config.js
	data/setup.html data/setup.js
	data/setupbtn.html data/setupbtn.js
	data/menuedit.html
	data/test.html data/test.js
	data/log.html data/log.js
	data/editprompt.html
	data/webremote.css data/favicon.ico data/icons.json
	data/back.svg data/bottom.svg data/down.svg data/download.svg data/top.svg data/up.svg
	data/navigator.js)

web_files(FILES ${WEB_RESOURCE_FILES} WEBSOCKET)
