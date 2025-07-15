# Use local nlohmann/json header file
# json.hpp is included in the parent include/ directory

# Find TDengine library
find_library(TAOS_LIBRARY 
    NAMES taos
    PATHS /usr/local/lib /usr/lib /opt/homebrew/lib
    DOC "TDengine library"
    NO_DEFAULT_PATH
)

find_path(TAOS_INCLUDE_DIR
    NAMES taos.h
    PATHS /usr/local/include /usr/include /opt/homebrew/include
    DOC "TDengine include directory"
    NO_DEFAULT_PATH
)

# Find MySQL library
find_library(MYSQL_LIBRARY 
    NAMES mysqlclient
    PATHS /usr/local/lib /usr/lib /opt/homebrew/lib
    DOC "MySQL client library"
    NO_DEFAULT_PATH
)

find_path(MYSQL_INCLUDE_DIR
    NAMES mysql.h
    PATHS /usr/local/include/mysql /usr/include/mysql /opt/homebrew/include/mysql
    DOC "MySQL include directory"
    NO_DEFAULT_PATH
)

# Find UUID library (for alarm manager)
find_library(UUID_LIBRARY
    NAMES uuid
    PATHS /usr/local/lib /usr/lib /opt/homebrew/lib
    DOC "UUID library"
    NO_DEFAULT_PATH
)

if(TAOS_LIBRARY AND TAOS_INCLUDE_DIR)
    message(STATUS "Found TDengine for tests: ${TAOS_LIBRARY}")
    set(TAOS_FOUND TRUE)
else()
    message(FATAL_ERROR "TDengine library not found for tests. Please install TDengine client library.")
endif()

if(MYSQL_LIBRARY AND MYSQL_INCLUDE_DIR)
    message(STATUS "Found MySQL for tests: ${MYSQL_LIBRARY}")
    set(MYSQL_FOUND TRUE)
else()
    message(FATAL_ERROR "MySQL client library not found for tests. Please install MySQL client library.")
endif()

if(UUID_LIBRARY)
    message(STATUS "Found UUID for tests: ${UUID_LIBRARY}")
    set(UUID_FOUND TRUE)
else()
    message(FATAL_ERROR "UUID library not found for tests. Please install UUID library.")
endif()

# Include source files for tests
file(GLOB_RECURSE TEST_SOURCES "*.cpp")
file(GLOB_RECURSE PROJECT_SOURCES "../src/*.cpp")

# Remove main.cpp from project sources to avoid multiple main functions
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

add_executable(tests ${TEST_SOURCES} ${PROJECT_SOURCES})
target_link_libraries(tests gtest_main ${TAOS_LIBRARY} ${MYSQL_LIBRARY} ${UUID_LIBRARY})
target_include_directories(tests PRIVATE ${TAOS_INCLUDE_DIR} ${MYSQL_INCLUDE_DIR} ../include)

# Fix library path on macOS
if(APPLE)
    add_custom_command(TARGET tests POST_BUILD
        COMMAND install_name_tool -change /opt/taos/main/TDinternal/community/debug/build/lib/libtaos.dylib /usr/local/lib/libtaos.dylib $<TARGET_FILE:tests>
        COMMENT "Fixing TDengine library path in tests"
    )
endif()

add_test(NAME MyProjectTests COMMAND tests)