cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(MeagreMUD VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
qt_standard_project_setup()

set(meagremud_SRCS
    Console.cpp
    MainWindow.cpp
    main.cpp
    Settings.cpp
    SettingsDialog.cpp
    terminalwidget.cpp
)

add_executable(meagremud ${meagremud_SRCS})
target_link_libraries(meagremud Qt6::Widgets Qt6::Network)
set_target_properties(meagremud PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON)
