qt_add_executable(meagre-gui
    src/main.cpp
    src/window/MainWindow.h
    src/window/MainWindow.cpp
    src/window/AttentionPanel.h
    src/window/AttentionPanel.cpp
    src/window/TiledArea.h
    src/window/TiledArea.cpp
    src/connection/DaemonConnectionManager.h
    src/connection/DaemonConnectionManager.cpp
    src/pane/CharacterPane.h
    src/pane/CharacterPane.cpp
    src/pane/StatusLine.h
    src/pane/StatusLine.cpp
    src/terminal/TerminalWidget.h
    src/terminal/TerminalWidget.cpp
    src/input/InputWidget.h
    src/input/InputWidget.cpp
    src/dialogs/SettingsDialog.h
    src/dialogs/SettingsDialog.cpp
)

target_link_libraries(meagre-gui
    PRIVATE
        meagrelib
        Qt6::Core
        Qt6::Network
        Qt6::Widgets
)

target_include_directories(meagre-gui PRIVATE src)

set_target_properties(meagre-gui PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE    TRUE
)
