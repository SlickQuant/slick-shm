# Changelog

## [v0.1.4] - 2026-01-30

### Added
- Add `is_creator()` method to `shared_memory` class to determine if the object created the segment or opened existing
  - Returns `true` if the object created the shared memory, `false` if it opened existing
  - Useful for conditional initialization and cleanup responsibilities
  - Works with all creation modes: `create_only`, `open_existing`, `open_or_create`, `open_always`
  - Creator status is preserved during move operations
  - Windows: Tracks via `GetLastError() != ERROR_ALREADY_EXISTS` after `CreateFileMapping()`
  - POSIX: Returns existing `owns_shm_` flag value
- Add comprehensive test suite for `is_creator()` functionality (10 new test cases in `test_is_creator.cpp`)
  - Tests all creation modes
  - Tests move constructor and move assignment semantics
  - Tests no-throw constructors
  - Tests invalid objects
- Add detailed API documentation for `is_creator()` in `docs/api_reference.md` with usage examples
- Add feature to README features list

## [v0.1.3] - 2026-01-29

### Fixed
- Add missing `<type_traits>` include in `error.hpp` to fix compilation with strict compilers
- Fix Windows handle validation in `create()`, `open()`, and `map_impl()` to properly check for both NULL and INVALID_HANDLE_VALUE
- Add missing `error.hpp` and `types.hpp` includes in `platform.hpp` to resolve dependency issues
- Add documentation comment to POSIX `format_name()` clarifying preconditions
- Fix Windows UTF-8 to UTF-16 name conversion buffer sizing in shared memory mapping
- Fix POSIX create/open_or_create/open_always logic to avoid unlinking existing segments and to allow read-only open_or_create
- Reject POSIX shared memory name of '/' and add tests for read-only create/open_or_create and slash-only name validation
- Fix unsafe atomic initialization in `advanced_sync` example (removed placement new on std::atomic in shared memory)
- Fix memory ordering in `advanced_sync` example initialization (use release semantics instead of relaxed)
- Fix potential cleanup race condition in `advanced_sync` example (explicit close before remove)

### Improved
- Add comprehensive alignment documentation for `data()` method (Windows 64KB, POSIX 4KB guarantees)
- Enhance `size()` method documentation about platform-specific size rounding behavior
- Add no-resize policy documentation with rationale and alternatives
- Improve error messages in `basic_writer` and `basic_reader` examples to include operation context and shared memory names

### Documentation
- Document Windows 64KB allocation granularity and POSIX 4KB page alignment guarantees
- Explain that actual allocated size may exceed requested size due to page rounding
- Clarify why resizing is not supported (Windows CreateFileMapping limitation)
- Add examples of better error handling patterns with contextual error messages

## [v0.1.2] - 2026-01-10

 - Rename repository from slick_shm to slick-shm (hyphenated naming follows recommended convention)
 - Update documentation and build references to use new repository name

## [v0.1.1] - 2025-12-26

 - Fix Linux build enum multiple definition error
 - Add CHANGELOG.md
 - Add version number cmake message

## [v0.1.0] - 2025-12-25

- Initial release
- Windows, Linux, and macOS support
- RAII-based shared memory management
