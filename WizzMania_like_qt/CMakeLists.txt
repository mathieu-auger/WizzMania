cmake_minimum_required(VERSION 3.21)

project(msn_like_qt
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt : automoc/autouic/autorcc pour .ui / Q_OBJECT / ressources
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ---- Trouver Qt6, sinon Qt5 ----
find_package(Qt6 COMPONENTS Core Widgets Network QUIET)

if (Qt6_FOUND)
  set(QT_VERSION_MAJOR 6)
  set(QT_LIBS Qt6::Core Qt6::Widgets Qt6::Network)
else()
  find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network)
  set(QT_VERSION_MAJOR 5)
  set(QT_LIBS Qt5::Core Qt5::Widgets Qt5::Network)
endif()

# ---- Options warnings ----
add_library(project_warnings INTERFACE)
if (MSVC)
  target_compile_options(project_warnings INTERFACE /W4 /permissive-)
else()
  target_compile_options(project_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# ---- Bibliothèque common (protocole / utilitaires partagés) ----
add_library(common
  src/common/Protocol.cpp
  src/common/Protocol.hpp
)

target_link_libraries(common
  PUBLIC ${QT_LIBS} project_warnings
)

target_include_directories(common
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ---- Client (GUI Qt) ----
add_executable(client
  src/client/main.cpp
  src/client/MainWindow.hpp
  src/client/MainWindow.cpp
  src/client/MainWindow.ui
)

target_link_libraries(client
  PRIVATE common
)

# ---- Serveur (GUI Qt) ----
add_executable(server
  src/server/main.cpp
  src/server/MainWindow.hpp
  src/server/MainWindow.cpp
  src/server/MainWindow.ui
)

target_link_libraries(server
  PRIVATE common
)

# ---- Propriétés utiles ----
# (Sur macOS, ça aide à avoir un bundle correct)
set_target_properties(client server PROPERTIES
  MACOSX_BUNDLE TRUE
)

# ---- Déploiement Qt (Qt6) optionnel ----
# Pour Qt6, Qt fournit qt_finalize_executable, utile sur certains générateurs.
if (QT_VERSION_MAJOR EQUAL 6)
  qt_finalize_executable(client)
  qt_finalize_executable(server)
endif()
