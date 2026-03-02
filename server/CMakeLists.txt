cmake_minimum_required(VERSION 3.20)
project(msn_server LANGUAGES CXX)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(msn_server
  "${SRC_DIR}/main.cpp"
  "${SRC_DIR}/routes.cpp"
  "${SRC_DIR}/database.cpp"
)

target_include_directories(msn_server PRIVATE "${SRC_DIR}")

# Crow/Asio (si tu utilises ASIO_STANDALONE)
target_compile_definitions(msn_server PRIVATE
  ASIO_STANDALONE
  ASIO_NO_DEPRECATED
)

# Windows network libs (Crow/Asio)
if (WIN32)
  target_link_libraries(msn_server PRIVATE ws2_32 mswsock iphlpapi)
endif()

# OpenSSL
find_package(OpenSSL QUIET)
if (OpenSSL_FOUND AND TARGET OpenSSL::Crypto)
  target_link_libraries(msn_server PRIVATE OpenSSL::Crypto)
  if (TARGET OpenSSL::SSL)
    target_link_libraries(msn_server PRIVATE OpenSSL::SSL)
  endif()
else()
  target_link_libraries(msn_server PRIVATE crypto ssl)
endif()

# SQLite
find_package(SQLite3 QUIET)
if (SQLite3_FOUND AND TARGET SQLite::SQLite3)
  target_link_libraries(msn_server PRIVATE SQLite::SQLite3)
else()
  target_link_libraries(msn_server PRIVATE sqlite3)
endif()

# Warnings
if (MSVC)
  target_compile_options(msn_server PRIVATE /W4)
else()
  target_compile_options(msn_server PRIVATE -Wall -Wextra -Wpedantic)
endif()