qt_add_executable(meagre-daemon
    src/main.cpp
    src/DaemonServer.h
    src/DaemonServer.cpp
    src/GuiConnection.h
    src/GuiConnection.cpp
    src/CharacterSession.h
    src/CharacterSession.cpp
    src/CharacterRegistry.h
    src/CharacterRegistry.cpp
)

target_link_libraries(meagre-daemon
    PRIVATE
        meagrelib
        Qt6::Core
        Qt6::Network
)

target_include_directories(meagre-daemon PRIVATE src)

# Daemon is a console application on all platforms
set_target_properties(meagre-daemon PROPERTIES
    WIN32_EXECUTABLE FALSE
)
