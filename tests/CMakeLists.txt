include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Find nlohmann/json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

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

if(TAOS_LIBRARY AND TAOS_INCLUDE_DIR)
    message(STATUS "Found TDengine for tests: ${TAOS_LIBRARY}")
    set(TAOS_FOUND TRUE)
else()
    message(FATAL_ERROR "TDengine library not found for tests. Please install TDengine client library.")
endif()

# Include source files for tests
file(GLOB_RECURSE TEST_SOURCES "*.cpp")
file(GLOB_RECURSE PROJECT_SOURCES "../src/*.cpp")

# Remove main.cpp from project sources to avoid multiple main functions
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

add_executable(tests ${TEST_SOURCES} ${PROJECT_SOURCES})
target_link_libraries(tests gtest_main ${TAOS_LIBRARY} nlohmann_json::nlohmann_json)
target_include_directories(tests PRIVATE ${TAOS_INCLUDE_DIR} ../include)

# Fix library path on macOS
if(APPLE)
    add_custom_command(TARGET tests POST_BUILD
        COMMAND install_name_tool -change /opt/taos/main/TDinternal/community/debug/build/lib/libtaos.dylib /usr/local/lib/libtaos.dylib $<TARGET_FILE:tests>
        COMMENT "Fixing TDengine library path in tests"
    )
endif()

add_test(NAME MyProjectTests COMMAND tests)