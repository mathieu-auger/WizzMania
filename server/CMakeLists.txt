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
)

# --- Includes ---
# crow_all.h + asio.hpp sont dans server/
target_include_directories(msn_server PRIVATE
    "${SRC_DIR}"
)

# --- Asio standalone (Crow 1.3.0 crow_all.h inclut <asio.hpp>) ---
# Important : aide la compilation à choisir Asio standalone.
target_compile_definitions(msn_server PRIVATE
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED
)

# Si tu as des erreurs de zlib (compression), décommente :
# target_compile_definitions(msn_server PRIVATE CROW_DISABLE_COMPRESSION)

# --- Boost (OPTIONNEL ici) ---
# Ton crow_all.h actuel utilise Asio standalone, donc Boost n'est pas requis
# pour un /ping minimal. On le garde en option sans bloquer la config.
find_package(Boost CONFIG QUIET)
if (Boost_FOUND AND DEFINED Boost_INCLUDE_DIRS)
    target_include_directories(msn_server PRIVATE ${Boost_INCLUDE_DIRS})
endif()

# --- Windows network library ---
if (WIN32)
    target_link_libraries(msn_server PRIVATE ws2_32 mswsock iphlpapi)
endif()



# --- Warnings (optionnel) ---
if (MSVC)
    target_compile_options(msn_server PRIVATE /W4)
else()
    target_compile_options(msn_server PRIVATE -Wall -Wextra -Wpedantic)
endif()
