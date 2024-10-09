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

# Add the utility libraries
add_subdirectory(../picolibs picolibs)

add_executable(${PROJECT_NAME} 
    remote.cpp
	remote_remote.cpp remote_backup.cpp remote_setup.cpp remote_menu.cpp
	remote_cleanup.cpp remote_config.cpp remote_test.cpp remote_log.cpp
	remotefile.cpp
	menu.cpp
	irprocessor.cpp
	irdevice.cpp
	command.cpp
	config.cpp
	backup.cpp
	${CMAKE_CURRENT_BINARY_DIR}/generated/web_files.cpp
	)

	target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})

pico_set_program_name(${PROJECT_NAME} "remote")
pico_set_program_version(${PROJECT_NAME} "0.1")

pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

target_link_libraries(${PROJECT_NAME} PUBLIC pico_stdlib 
    pico_cyw43_arch_lwip_threadsafe_background pico_lwip_mbedtls pico_mbedtls pico_lwip_mdns
    flash_filesystem tiny-json bgr_network bgr_ir_protocols bgr_util bgr_json)

pico_add_extra_outputs(${PROJECT_NAME})


find_package (Python3 REQUIRED COMPONENTS Interpreter)
set(WEB_RESOURCE_FILES
 	data/index.html data/webremote.js
 	data/backup.html data/backup.js
	data/config.html data/config.js
	data/setup.html data/setup.js
	data/setupbtn.html data/setupbtn.js
	data/menuedit.html
	data/test.html data/test.js
	data/log.html
	data/editprompt.html
	data/websocket.js
    data/webremote.css data/favicon.ico data/icons.json
	data/back.svg data/bottom.svg data/down.svg data/download.svg data/top.svg data/up.svg
    ${CERTS})
set(WEB_FILES_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(WEB_FILES "${WEB_FILES_DIR}/web_files.cpp")
set(WEB_FILES_TARGET "${PROJECT_NAME}_web_files")
add_custom_target(${WEB_FILES_TARGET} DEPENDS ${WEB_RESOURCE_FILES})
add_custom_command(
	OUTPUT ${WEB_FILES}
	DEPENDS ${WEB_RESOURCE_FILES} ${CMAKE_SOURCE_DIR}/create_web_files.py
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	COMMAND ${CMAKE_COMMAND} -E make_directory ${WEB_FILES_DIR} &&
			${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/create_web_files.py -o ${WEB_FILES} ${WEB_RESOURCE_FILES}
	VERBATIM)
add_dependencies(${PROJECT_NAME}
	${WEB_FILES_TARGET}
	)
