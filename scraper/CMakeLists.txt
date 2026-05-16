qt_add_executable(meagre-scrape
    src/main.cpp
    src/BtrieveReader.h
    src/BtrieveReader.cpp
    src/NativeDbImporter.h
    src/NativeDbImporter.cpp
    src/WorldKnowledgeWriter.h
    src/WorldKnowledgeWriter.cpp
)

target_link_libraries(meagre-scrape
    PRIVATE
        meagrelib
        Qt6::Core
        Qt6::Sql
)

target_include_directories(meagre-scrape PRIVATE src)

set_target_properties(meagre-scrape PROPERTIES
    WIN32_EXECUTABLE FALSE
)
