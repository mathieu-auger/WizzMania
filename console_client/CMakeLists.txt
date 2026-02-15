cmake_minimum_required(VERSION 3.16)
project(wizzmania_console)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(wizzmania_console
    src/main_console.cpp
)

target_include_directories(wizzmania_console PRIVATE
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/common 
)

if(WIN32)
    target_link_libraries(wizzmania_console ws2_32)
endif()