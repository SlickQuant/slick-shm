# slick_shm

A modern C++17 header-only, cross-platform shared memory library.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Header-only](https://img.shields.io/badge/header--only-yes-brightgreen.svg)](#installation)
[![CI](https://github.com/SlickQuant/slick_shm/actions/workflows/ci.yml/badge.svg)](https://github.com/SlickQuant/slick_shm/actions/workflows/ci.yml)
[![GitHub release](https://img.shields.io/github/v/release/SlickQuant/slick_shm)](https://github.com/SlickQuant/slick_shm/releases)

## Features

- **Header-only**: No compilation required, just include and use
- **Cross-platform**: Works on Windows, Linux, and macOS
- **Modern C++17**: Uses modern C++ features and idioms
- **RAII**: Automatic resource management
- **Hybrid error handling**: Both exception and no-throw variants
- **Type-safe**: Clean, type-safe API
- **Well-tested**: Comprehensive test suite with Catch2
- **Well-documented**: Extensive API documentation and examples

## Platform Support

| Platform | Implementation | Status |
|----------|---------------|---------|
| Windows  | `CreateFileMapping` / `MapViewOfFile` | ✅ Supported |
| Linux    | POSIX `shm_open` / `mmap` | ✅ Supported |
| macOS    | POSIX `shm_open` / `mmap` | ✅ Supported |

## Quick Start

### Installation

#### vcpkg

Install using [vcpkg](https://github.com/microsoft/vcpkg):

```bash
# Install from vcpkg
vcpkg install slick-shm

# Or add to vcpkg.json manifest
{
  "dependencies": ["slick-shm"]
}
```

Then in your CMakeLists.txt:

```cmake
find_package(slick_shm CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE slick::shm)
```

#### Header-Only (Manual)

Simply copy the `include/slick` directory to your project and include it:

```cpp
#include <slick/shm/shared_memory.hpp>
```

#### CMake FetchContent

```cmake
include(FetchContent)

# Disable slick_shm tests and examples
set(SLICK_SHM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SLICK_SHM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SLICK_SHM_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    slick_shm
    GIT_REPOSITORY https://github.com/SlickQuant/slick_shm.git
    GIT_TAG v0.1.0  # See https://github.com/SlickQuant/slick_shm/releases for latest version
)
FetchContent_MakeAvailable(slick::slick_shm)

target_link_libraries(your_target PRIVATE slick::slick_shm)
```

#### CMake Integration

```cmake
# Option 1: Add as subdirectory
add_subdirectory(path/to/slick_shm)
target_link_libraries(your_target PRIVATE slick::shm)

# Option 2: Install and find_package
find_package(slick_shm REQUIRED)
target_link_libraries(your_target PRIVATE slick::shm)
```

### Basic Usage

#### Writer Process

```cpp
#include <slick/shm/shared_memory.hpp>
#include <cstring>

int main() {
    using namespace slick::shm;

    // Create shared memory
    shared_memory shm("my_shm", 1024, create_only);

    // Write data
    const char* message = "Hello, World!";
    std::memcpy(shm.data(), message, std::strlen(message) + 1);

    return 0;
}
```

#### Reader Process

```cpp
#include <slick/shm/shared_memory.hpp>
#include <iostream>

int main() {
    using namespace slick::shm;

    // Open existing shared memory
    shared_memory shm("my_shm", open_existing, access_mode::read_only);

    // Read data
    const char* message = static_cast<const char*>(shm.data());
    std::cout << message << std::endl;

    // Cleanup (important on POSIX systems)
    shared_memory::remove("my_shm");

    return 0;
}
```

## Building

### Requirements

- C++17 compatible compiler
  - GCC 7+ or Clang 5+ (Linux/macOS)
  - MSVC 2017+ or MinGW (Windows)
- CMake 3.14+ (for building examples and tests)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/SlickQuant/slick_shm.git
cd slick_shm

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build .

# Run tests
ctest --output-on-failure

# Install (optional)
cmake --install . --prefix /usr/local
```

### CMake Options

- `SLICK_SHM_BUILD_EXAMPLES` (default: ON) - Build example programs
- `SLICK_SHM_BUILD_TESTS` (default: ON) - Build unit tests
- `SLICK_SHM_INSTALL` (default: ON) - Generate install target

## Documentation

- [API Reference](docs/api_reference.md) - Complete API documentation
- [Platform Notes](docs/platform_notes.md) - Platform-specific behavior and limitations
- [Examples](docs/examples.md) - Detailed usage examples and patterns

## API Overview

### Core Classes

#### `shared_memory`

The main RAII wrapper for shared memory.

```cpp
// Create new shared memory
shared_memory shm("name", 1024, create_only);

// Open existing shared memory
shared_memory shm("name", open_existing);

// Create or open
shared_memory shm("name", 1024, open_or_create);

// Access data
void* ptr = shm.data();
size_t size = shm.size();

// Check validity
if (shm.is_valid()) { /* ... */ }

// Static utilities
shared_memory::remove("name");
bool exists = shared_memory::exists("name");
```

#### `shared_memory_view`

A non-owning view into shared memory.

```cpp
shared_memory shm("name", 1024, create_only);
shared_memory_view view(shm);  // Lightweight, copyable
```

### Error Handling

#### Exception-Based

```cpp
try {
    shared_memory shm("test", 1024, create_only);
} catch (const shared_memory_error& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    if (e.code() == errc::already_exists) {
        // Handle specific error
    }
}
```

#### No-Throw Variant

```cpp
shared_memory shm("test", 1024, create_only,
                  access_mode::read_write, std::nothrow);

if (!shm.is_valid()) {
    std::cerr << "Error: " << shm.last_error().message() << std::endl;
}
```

## Examples

The [examples](examples/) directory contains:

- [basic_writer.cpp](examples/basic_writer.cpp) - Simple writer example
- [basic_reader.cpp](examples/basic_reader.cpp) - Simple reader example
- [advanced_sync.cpp](examples/advanced_sync.cpp) - Synchronization with `std::atomic`

Build and run:

```bash
cd build
./examples/basic_writer    # In one terminal
./examples/basic_reader    # In another terminal
```

## Testing

The library includes a comprehensive test suite using Catch2:

```bash
cd build
ctest --output-on-failure
```

Tests cover:
- Create/open operations
- Error handling (exceptions and no-throw)
- Move semantics
- Cross-process communication
- Platform-specific behavior

## Platform-Specific Notes

### Windows

- Uses system paging file for backing
- Names used as-is (can prefix with `Global\` or `Local\`)
- Automatic cleanup when last handle closes

### Linux/macOS (POSIX)

- Uses `shm_open()` / `mmap()` backed by tmpfs
- Names automatically prefixed with `/` (handled internally)
- **Important**: Must call `shared_memory::remove()` to clean up
- **Name length limits**:
  - macOS: 31 characters maximum (including the `/` prefix)
  - Linux: 255 characters maximum
  - Recommendation: Keep names ≤30 characters for cross-platform compatibility

See [platform_notes.md](docs/platform_notes.md) for detailed information.

## Best Practices

1. **Name portability**: Use short (≤30 chars), alphanumeric names for cross-platform compatibility
2. **Always cleanup**: Call `remove()` for portable code
3. **Synchronize access**: Use `std::atomic` or external synchronization
4. **Handle errors**: Always check for errors (exceptions or `is_valid()`)
5. **Use views**: Pass `shared_memory_view` to avoid ownership confusion

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## Author

Slick Quant

## Acknowledgments

- Inspired by Boost.Interprocess
- Uses Catch2 for testing
