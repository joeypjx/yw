cmake_minimum_required(VERSION 3.10)
project(MyProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Find nlohmann/json
include(FetchContent)
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
    message(STATUS "Found TDengine: ${TAOS_LIBRARY}")
    set(TAOS_FOUND TRUE)
else()
    message(FATAL_ERROR "TDengine library not found. Please install TDengine client library.")
endif()

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME} ${TAOS_LIBRARY} nlohmann_json::nlohmann_json)
target_include_directories(${PROJECT_NAME} PRIVATE ${TAOS_INCLUDE_DIR})

# Fix library path on macOS
if(APPLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND install_name_tool -change /opt/taos/main/TDinternal/community/debug/build/lib/libtaos.dylib /usr/local/lib/libtaos.dylib $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Fixing TDengine library path"
    )
endif()

option(BUILD_TESTS "Build test programs" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()