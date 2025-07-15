# MyProject

A C++ 14 project with TDengine resource monitoring capabilities.

## Prerequisites

- TDengine client library (see INSTALL_DEPENDENCIES.md)
- CMake 3.10 or higher
- C++14 compatible compiler

## Build Instructions

```bash
# Install TDengine client library first (see INSTALL_DEPENDENCIES.md)
mkdir build
cd build
cmake ..
make
```

## Run Tests

```bash
cd build
make test
```

## Directory Structure

```
├── src/          # Source files
├── include/      # Header files
├── tests/        # Test files
├── docs/         # Documentation
├── build/        # Build output (generated)
├── examples/     # Example code
├── lib/          # Third-party libraries
├── cmake/        # CMake modules
├── CMakeLists.txt
└── .gitignore
```