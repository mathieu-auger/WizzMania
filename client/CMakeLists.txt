# cmake_minimum_required(VERSION 3.16)
# project(chat_client VERSION 1.0.0 LANGUAGES CXX)

# # ============================================
# # QT SETTINGS
# # ============================================
# set(QT_PATH "C:/Program Files/msys64/mingw64")
# set(CMAKE_PREFIX_PATH ${QT_PATH})

# # Compiler settings
# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)

# # Windows specific settings
# if(WIN32)
#     add_compile_definitions(_WIN32_WINNT=0x0601)
#     add_compile_definitions(NOMINMAX)
#     add_compile_definitions(UNICODE _UNICODE)
# endif()

# # ============================================
# # FIND QT6
# # ============================================
# find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# # Qt auto tools
# set(CMAKE_AUTOMOC ON)
# set(CMAKE_AUTOUIC ON)
# set(CMAKE_AUTORCC ON)

# # ============================================
# # SOURCE FILES
# # ============================================
# set(CLIENT_SOURCES
#     src/main_client.cpp
#     src/Client.cpp
#     src/ChatWindow.cpp
#     ../common/SocketRAII.cpp  # Path to common code
# )

# # UI files (if using Qt Designer)
# file(GLOB UI_FILES "*.ui")
# file(GLOB RESOURCE_FILES "*.qrc")

# # Include directories
# include_directories(
#     include
#     ../common
# )

# # ============================================
# # CREATE CLIENT EXECUTABLE
# # ============================================
# add_executable(chat_client 
#     ${CLIENT_SOURCES}
#     ${UI_FILES}
#     ${RESOURCE_FILES}
# )

# # ============================================
# # LINK LIBRARIES
# # ============================================
# target_link_libraries(chat_client
#     Qt6::Core
#     Qt6::Widgets
#     Qt6::Network
# )

# if(WIN32)
#     target_link_libraries(chat_client ws2_32)
# endif()

# # ============================================
# # POST-BUILD: Copy Qt DLLs (Windows only)
# # ============================================
# if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
#     add_custom_command(TARGET chat_client POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         "${QT_PATH}/bin/Qt6Core.dll"
#         "${QT_PATH}/bin/Qt6Widgets.dll"
#         "${QT_PATH}/bin/Qt6Network.dll"
#         "$<TARGET_FILE_DIR:chat_client>"
#         COMMENT "Copying Qt DLLs..."
#     )
# endif()

# # ============================================
# # INSTALLATION (optional)
# # ============================================
# install(TARGETS chat_client
#     RUNTIME DESTINATION bin
# )

# if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
#     install(FILES
#         "${QT_PATH}/bin/Qt6Core.dll"
#         "${QT_PATH}/bin/Qt6Widgets.dll"
#         "${QT_PATH}/bin/Qt6Network.dll"
#         DESTINATION bin
#     )
# endif()

cmake_minimum_required(VERSION 3.20)
project(wizzmania_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ✅ Obligatoire si tu as Q_OBJECT (moc) et des fichiers .ui (uic)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ---- Qt ----
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# ---- Dossiers ----
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ---- Exécutable ----
add_executable(wizzmania_client
    "${SRC_DIR}/main.cpp"
    "${SRC_DIR}/MainWindow.cpp"
    "${INCLUDE_DIR}/MainWindow.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/MainWindow.ui"
)

# ---- Includes (pour éviter les ../../ dans le code) ----
# Adapte si tes dossiers sont différents, mais avec ta structure actuelle c'est ça.
target_include_directories(wizzmania_client PRIVATE
    "${INCLUDE_DIR}"                                      # client/include
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"                  # pour common/Message.hpp (src/common/Message.hpp)
    "${CMAKE_CURRENT_SOURCE_DIR}/../console_client/include" # pour HttpClient.hpp
)

# ---- Link Qt ----
target_link_libraries(wizzmania_client PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# ---- Déploiement Qt automatique (Windows) ----
if (WIN32)
    # Récupère le dossier bin de Qt via qmake
    get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_qmake}" DIRECTORY)
    set(_windeployqt "${_qt_bin_dir}/windeployqt.exe")

    add_custom_command(TARGET wizzmania_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Running windeployqt: ${_windeployqt}"
        COMMAND "${_windeployqt}"
                --no-translations
                --no-opengl-sw
                "$<TARGET_FILE:wizzmania_client>"
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:wizzmania_client>"
        COMMENT "Deploying Qt runtime..."
    )
endif()