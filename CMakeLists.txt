cmake_minimum_required(VERSION 3.22)

include(cmake/MeagreMUDVersion.cmake)

project(MeagreMUD
    VERSION ${MEAGREMUD_VERSION}
    DESCRIPTION "Open source MegaMUD clone"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Network Sql Svg Widgets)
qt_standard_project_setup()

add_subdirectory(meagrelib)
add_subdirectory(daemon)
add_subdirectory(gui)
add_subdirectory(scraper)
