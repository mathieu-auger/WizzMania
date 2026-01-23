# cmake_minimum_required(VERSION 3.21)

# project(WizzMania VERSION 0.1.0 LANGUAGES CXX)

# set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)
# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# # ---- Options (anticipation) ----
# option(BUILD_HTTP_SERVER "Build the Crow HTTP server" ON)
# option(BUILD_QT_CLIENT   "Build the Qt6 client GUI" OFF)
# option(BUILD_QT_SERVER   "Build the Qt6 server GUI/TCP" OFF)

# # ---- Warnings ----
# add_library(project_warnings INTERFACE)
# if (MSVC)
#   target_compile_options(project_warnings INTERFACE /W4 /permissive-)
# else()
#   target_compile_options(project_warnings INTERFACE -Wall -Wextra -Wpedantic)
# endif()

# # ---- Threads (Crow) ----
# find_package(Threads REQUIRED)

# # ---- Common (votre Protocol.* existe déjà) ----
# # add_library(common STATIC
# #   src/common/Protocol.cpp
# #   src/common/Protocol.hpp
# # )

# # target_include_directories(common
# #   PUBLIC
# #     ${CMAKE_CURRENT_SOURCE_DIR}/src/common
# # )

# # target_link_libraries(common
# #   PUBLIC
# #     project_warnings
# #     Threads::Threads
# # )

# # ============================================================
# # HTTP SERVER (Crow) - basé sur VOS chemins réels
# # ============================================================
# if (BUILD_HTTP_SERVER)

#   add_executable(http_server
#     src/common/http_server/main.cpp
#     src/common/http_server/routes.hpp
#     src/common/http_server/routes.cpp
#     # src/common/http_server/storage.hpp
#     # src/common/http_server/storage.cpp
#   )

#   target_include_directories(http_server
#     PRIVATE
#       ${CMAKE_CURRENT_SOURCE_DIR}/external/crow/include
#       ${CMAKE_CURRENT_SOURCE_DIR}/src/common/http_server
#   )

#   target_link_libraries(http_server
#     PRIVATE
#       common
#   )

# endif()

# # ============================================================
# # Qt6 (anticipation) - activé uniquement si demandé
# # ============================================================
# if (BUILD_QT_CLIENT OR BUILD_QT_SERVER)

#   set(CMAKE_AUTOMOC ON)
#   set(CMAKE_AUTOUIC ON)
#   set(CMAKE_AUTORCC ON)

#   # Recommandé MSYS2 MinGW64 :
#   # -DQt6_DIR="C:/msys64/mingw64/lib/cmake/Qt6"
#   if (NOT DEFINED Qt6_DIR AND DEFINED QT_ROOT)
#     list(PREPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
#   endif()

#   find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

#   add_library(qt_common INTERFACE)
#   target_link_libraries(qt_common INTERFACE Qt6::Core Qt6::Network common)

#   if (BUILD_QT_CLIENT)
#     file(GLOB_RECURSE QT_CLIENT_SOURCES CONFIGURE_DEPENDS
#       client/*.cpp client/*.hpp client/*.h client/*.ui
#     )
#     add_executable(qt_client ${QT_CLIENT_SOURCES})
#     target_include_directories(qt_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/client)
#     target_link_libraries(qt_client PRIVATE qt_common Qt6::Widgets)
#     qt_finalize_executable(qt_client)
#   endif()

#   if (BUILD_QT_SERVER)
#     file(GLOB_RECURSE QT_SERVER_SOURCES CONFIGURE_DEPENDS
#       server/*.cpp server/*.hpp server/*.h server/*.ui
#     )
#     add_executable(qt_server ${QT_SERVER_SOURCES})
#     target_include_directories(qt_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/server)
#     target_link_libraries(qt_server PRIVATE qt_common Qt6::Widgets)
#     qt_finalize_executable(qt_server)
#   endif()

# endif()


cmake_minimum_required(VERSION 3.16)

project(wizz LANGUAGES CXX)

add_subdirectory(server)