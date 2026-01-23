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

cmake_minimum_required(VERSION 3.16)
project(wizzmania_client VERSION 1.0.0 LANGUAGES CXX)

set(QT_PATH "C:/Program Files/msys64/mingw64")
set(CMAKE_PREFIX_PATH ${QT_PATH})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(${PROJECT_NAME}
    main.cpp
    MainWindow.cpp
    MainWindow.hpp
    MainWindow.ui
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES 
        WIN32_EXECUTABLE TRUE
    )
endif()

target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
)

if(WIN32)
    # 1. Основні Qt DLL
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_PATH}/bin/Qt6Core.dll"
        "${QT_PATH}/bin/Qt6Gui.dll"
        "${QT_PATH}/bin/Qt6Widgets.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying Qt DLLs..."
    )
    
    # 2. Плагіни платформи
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms"
    )
    
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_PATH}/share/qt6/plugins/platforms/qwindows.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms/"
        COMMENT "Copying Qt platform plugin..."
    )
endif()

message(STATUS "Client configured with Qt ${Qt6_VERSION}")