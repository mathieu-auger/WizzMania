cmake_minimum_required(VERSION 3.20)

project(msn_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(msn_server
    main.cpp
    routes.cpp
    database.cpp
)

# crow_all.h + asio.hpp sont dans /server
target_include_directories(msn_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ===== Windows network libs (Crow/Asio) =====
if (WIN32)
    target_link_libraries(msn_server PRIVATE ws2_32 mswsock)
endif()

# ===== OpenSSL (SHA256) =====
find_package(OpenSSL QUIET)
if (OpenSSL_FOUND AND TARGET OpenSSL::Crypto)
    target_link_libraries(msn_server PRIVATE OpenSSL::Crypto)
    if (TARGET OpenSSL::SSL)
        target_link_libraries(msn_server PRIVATE OpenSSL::SSL)
    endif()
else()
    # fallback MSYS2/MinGW
    target_link_libraries(msn_server PRIVATE crypto ssl)
endif()

# ===== SQLite3 =====
find_package(SQLite3 QUIET)
if (SQLite3_FOUND AND TARGET SQLite::SQLite3)
    target_link_libraries(msn_server PRIVATE SQLite::SQLite3)
else()
    # fallback MSYS2/MinGW
    target_link_libraries(msn_server PRIVATE sqlite3)
endif()

# ===== warnings =====
if (MSVC)
    target_compile_options(msn_server PRIVATE /W4)
else()
    target_compile_options(msn_server PRIVATE -Wall -Wextra -Wpedantic)
endif()
