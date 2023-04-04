cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)

project(MeagreMUD)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network)

set(meagremud_SRCS
    Console.cpp
    MainWindow.cpp
    main.cpp
    Settings.cpp
    SettingsDialog.cpp
    terminalwidget.cpp
)

add_executable(meagremud ${meagremud_SRCS})
target_link_libraries(meagremud Qt5::Widgets Qt5::Network)
