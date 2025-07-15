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

The project currently contains:
- Basic "Hello, World!" application in `src/main.cpp`
- Sample test in `tests/test_main.cpp`
- No custom headers or library code yet

This is a foundation ready for expansion with proper separation of source, headers, and tests following modern C++ project conventions.