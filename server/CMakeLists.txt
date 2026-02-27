cmake_minimum_required(VERSION 3.20)

# Supprime les warnings liés à l'évolution de find_package
cmake_policy(SET CMP0144 NEW) # utilise <PackageName>_ROOT (ex: Boost_ROOT)
cmake_policy(SET CMP0167 NEW) # FindBoost retiré -> on privilégie Boost CONFIG

project(msn_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Sources ---
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(msn_server
    "${SRC_DIR}/main.cpp"
    "${SRC_DIR}/routes.cpp"
    "${SRC_DIR}/routes.hpp"
    "${SRC_DIR}/database.hpp"
    "${SRC_DIR}/database.cpp"
)

# --- Includes ---
# crow_all.h + asio.hpp sont dans server/
target_include_directories(msn_server PRIVATE
    "${SRC_DIR}"
)

# --- Asio standalone (Crow 1.3.0 crow_all.h inclut <asio.hpp>) ---

target_compile_definitions(msn_server PRIVATE
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED
)

# find_package(Boost CONFIG QUIET)
# if (Boost_FOUND AND DEFINED Boost_INCLUDE_DIRS)
#     target_include_directories(msn_server PRIVATE ${Boost_INCLUDE_DIRS})
# endif()

# --- Windows network library ---
if (WIN32)
    target_link_libraries(msn_server PRIVATE ws2_32 mswsock iphlpapi)
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



# --- Warnings (optionnel) ---
if (MSVC)
    target_compile_options(msn_server PRIVATE /W4)
else()
    target_compile_options(msn_server PRIVATE -Wall -Wextra -Wpedantic)
endif()
