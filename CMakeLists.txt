cmake_minimum_required(VERSION 3.6)
project(ascii-art VERSION 0.1.0.0 LANGUAGES C)

add_executable(main src/main.c)
add_executable(rand src/rand.c)
add_executable(memory src/memory.c)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
	target_link_libraries(main Ws2_32.lib)
	target_link_libraries(rand Advapi32.lib)
	target_link_libraries(memory Kernel32.lib Ws2_32.lib)
endif()
