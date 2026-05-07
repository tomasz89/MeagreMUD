add_library(meagrelib STATIC
    src/protocol/Protocol.h
    src/types/MudTypes.h
    src/db/Schema.h
    src/db/DatabaseManager.h
    src/db/DatabaseManager.cpp
    src/meagrelib.cpp
)

target_include_directories(meagrelib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(meagrelib
    PUBLIC  Qt6::Core Qt6::Network Qt6::Sql
)
