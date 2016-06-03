cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(MeagreMUD)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Widgets REQUIRED)
add_executable(MeagreMUD main.cpp mainwindow.cpp mainwindow.h configdialog.h configdialog.cpp configpages.h configpages.cpp)
target_link_libraries(MeagreMUD Qt5::Widgets)
