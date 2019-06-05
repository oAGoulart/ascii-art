cmake_minimum_required(VERSION 3.6)
project(ascii-art VERSION 0.1.0.0 LANGUAGES C)

add_executable(main src/main.c)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
	target_link_libraries(main Ws2_32.lib)
endif()
