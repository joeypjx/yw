cmake_minimum_required(VERSION 3.10)
project(MyProject)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加调试信息
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

include_directories(include)
include_directories(include/resource)
include_directories(include/asio/asio/include)
include_directories(include/websocketpp)

# Use local nlohmann/json header file
# json.hpp is included in the include/ directory

# Use local websocketpp library
# websocketpp is header-only and included in the include/websocketpp directory

# Use local Asio library (standalone version)
# Asio headers are in include/asio/asio/include/

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
    PATHS /usr/local/lib /usr/lib /opt/homebrew/lib /opt/homebrew/Cellar/mysql-client/9.3.0/lib/
    DOC "MySQL client library"
    NO_DEFAULT_PATH
)

find_path(MYSQL_INCLUDE_DIR
    NAMES mysql.h
    PATHS /usr/local/include/mysql /usr/include/mysql /opt/homebrew/include/mysql /opt/homebrew/Cellar/mysql-client/9.3.0/include/mysql
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
    message(STATUS "Found TDengine: ${TAOS_LIBRARY}")
    set(TAOS_FOUND TRUE)
else()
    message(FATAL_ERROR "TDengine library not found. Please install TDengine client library.")
endif()

if(MYSQL_LIBRARY AND MYSQL_INCLUDE_DIR)
    message(STATUS "Found MySQL: ${MYSQL_LIBRARY}")
    set(MYSQL_FOUND TRUE)
else()
    message(FATAL_ERROR "MySQL client library not found. Please install MySQL client library.")
endif()

if(UUID_LIBRARY)
    message(STATUS "Found UUID: ${UUID_LIBRARY}")
    set(UUID_FOUND TRUE)
else()
    message(FATAL_ERROR "UUID library not found. Please install UUID library.")
endif()

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/resource/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h" "include/resource/*.h")

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# spdlog will automatically define SPDLOG_HEADER_ONLY when not using compiled lib

# Link libraries
target_link_libraries(${PROJECT_NAME} ${TAOS_LIBRARY} ${MYSQL_LIBRARY} ${UUID_LIBRARY} Threads::Threads)
target_include_directories(${PROJECT_NAME} PRIVATE ${TAOS_INCLUDE_DIR} ${MYSQL_INCLUDE_DIR})

# Define ASIO_STANDALONE for websocketpp to use standalone Asio
target_compile_definitions(${PROJECT_NAME} PRIVATE ASIO_STANDALONE)



# Fix library path on macOS
if(APPLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND install_name_tool -change /opt/taos/main/TDinternal/community/debug/build/lib/libtaos.dylib /usr/local/lib/libtaos.dylib $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Fixing TDengine library path"
    )
endif()

