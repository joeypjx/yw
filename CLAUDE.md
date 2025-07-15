# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This is a modern C++ project using CMake with C++17 standard. The build system uses CMake's FetchContent to automatically download and build dependencies.

### Common Commands

```bash
# Build the project
mkdir build
cd build
cmake ..
make

# Run the executable
./MyProject

# Run tests
make test

# Clean build
rm -rf build
```

### Testing

The project uses Google Test (GoogleTest) framework version 1.12.1, fetched automatically via CMake's FetchContent. Tests are located in the `tests/` directory with their own CMakeLists.txt configuration.

## Project Architecture

### Directory Structure

- `src/` - Main source files (.cpp)
- `include/` - Public header files (.h)
- `tests/` - Test files with Google Test framework
- `build/` - Build output directory (generated)
- `cmake/` - CMake modules (currently empty)
- `docs/` - Documentation (currently empty)
- `examples/` - Example code (currently empty)
- `lib/` - Third-party libraries (currently empty)

### CMake Configuration

The main CMakeLists.txt:
- Sets C++17 standard as required
- Includes headers from `include/` directory
- Recursively finds all `.cpp` files in `src/` and `.h` files in `include/`
- Creates a single executable target named `MyProject`
- Optional test building with `BUILD_TESTS` flag (enabled by default)

The tests/CMakeLists.txt:
- Uses FetchContent to download GoogleTest from GitHub
- Links against `gtest_main` library
- Integrates with CTest for `make test` command

### Current Implementation

The project contains:
- **TDengine Resource Storage Module** (`src/resource_storage.cpp`, `include/resource_storage.h`)
  - Connects to TDengine database (127.0.0.1, user: test, password: HZ715Net)
  - Creates `resource` database and time-series tables for CPU, Memory, Network, Disk, GPU metrics
  - Provides interface to store JSON resource data matching `@docs/data.json` format
  - Automatic table creation with proper naming (IP addresses with dots converted to underscores)
  
- **Main Application** (`src/main.cpp`)
  - Demonstrates ResourceStorage usage with sample data
  - Connects to TDengine, creates database/tables, inserts resource data
  
- **Comprehensive Test Suite** (`tests/test_resource_storage.cpp`)
  - Tests database connection, table creation, data insertion
  - Covers all resource types (CPU, Memory, Network, Disk, GPU)
  - Includes error handling and edge cases

### TDengine Integration

The ResourceStorage class provides the following interface:
- `connect()` - Connect to TDengine database
- `createDatabase(dbName)` - Create database (typically "resource")
- `createResourceTable()` - Create super tables for all resource types
- `insertResourceData(hostIp, jsonData)` - Insert JSON resource data

The module automatically handles:
- Time-series table creation with proper tags
- JSON parsing and data validation
- SQL generation and execution
- Error handling and logging
- Special character cleaning for table names (/, -, ., :, spaces converted to _)

### Dependencies

- TDengine client library (libtaos)
- nlohmann/json for JSON handling
- Google Test for testing

### Known Issues

- TDengine library on macOS has incorrect install_name, fixed with post-build install_name_tool
- Library path automatically corrected during build process