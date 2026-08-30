# Changelog

## [v0.1.6] - 2026-08-30

### Changed
- `shared_memory_view` is now allocation-free to copy. It borrows the name pointer
  instead of holding its own `std::string`, so `sizeof(shared_memory_view)` drops from
  56 to 32 bytes and its copy constructor and copy assignment become `noexcept`. Passing
  a view by value no longer allocates - which it previously did for any name longer than
  the standard library's small-string capacity (15 characters on MSVC).
- To make that borrow sound, `shared_memory` now keeps the name behind `name()` in
  address-stable storage rather than a `std::string`. Moving a `shared_memory` transfers
  a pointer and leaves the characters where they are, so views built from it stay valid
  across a move, a move-assignment, or a `std::vector<shared_memory>` reallocation. As a
  side effect `sizeof(shared_memory)` drops from 112 to 88 bytes.
- Constructing a `shared_memory_view` from a temporary `shared_memory` is now a
  compile-time error rather than an immediately dangling view.
- `access_mode` now means the same thing on every platform: it restricts the calling
  process's mapping, not the segment. Windows used to pass `PAGE_READONLY` to
  `CreateFileMapping()` when a segment was created `read_only`, which made the section
  read-only for *every* process forever - no later opener could map it for writing,
  while the same code on POSIX left the segment writable for others. Windows now always
  creates the section `PAGE_READWRITE` and restricts only its own view, matching POSIX.

### Fixed
- POSIX `create()` no longer leaks the segment when `mmap()` fails on a newly created
  one. The `fstat`/`ftruncate` error paths unlinked the segment, but the final
  `map_impl()` call did not, and `close()` never unlinks - so a failed mapping (an
  unsatisfiable size, for instance) left the segment behind in `/dev/shm` until it was
  removed by hand. The mapping failure is now routed through the same cleanup, which
  unlinks only when this call created the segment.
- POSIX `open()` now releases the descriptor when `mmap()` fails, instead of holding it
  until the object is destroyed. Matches the `fstat()` error path just above it, and
  matters for a no-throw construction whose failed object stays alive.
- Windows name validation no longer rejects the documented `Global\` / `Local\` object
  namespace prefixes. `is_valid_name()` rejected every name containing a backslash, so
  `shared_memory("Global\my_shm", 1024, create_only)` failed with `errc::invalid_name`
  even though the README and platform notes advertise and demonstrate exactly that form.
  A leading `Global\`, `Local\` or `Session\<id>\` prefix (matched case-insensitively, as
  the kernel does) is now accepted; a backslash anywhere past the prefix, or a prefix
  with no object name after it, is still invalid.

### Documentation
- Document that `size` is a request, not a guarantee, once the segment already exists.
  `open_always` resizes on Linux, resizes on macOS only when no other process holds the
  segment open, and never resizes on Windows - so `size()` can come back **smaller than
  requested**, and on Linux an existing segment can be shrunk while other processes have
  it mapped. Adds a per-platform table and the `size()` check idiom to the API reference,
  rewrites the macOS-only note in the platform notes to cover all three platforms, and
  adds a best-practice line to the README. A test now pins the per-platform outcome.

## [v0.1.5] - 2026-08-19

### Fixed
- The installed package config now `find_dependency(Threads)` before loading the
  export set. The exported `slick::shm` target lists `Threads::Threads` in its link
  interface on Apple and Unix, but the config never ensured that target existed, so
  a consumer that had not already called `find_package(Threads)` itself failed at
  generate time with `The link interface of target "slick::shm" contains:
  Threads::Threads but the target was not found`. The failure surfaced transitively
  and well away from its cause - `slick-queue` depends on `slick-shm`, so projects
  that only ever asked for `slick-queue`, or for `slick-logger` above it, were the
  ones that broke.

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

### Fixed
- Fix `open_always` mode on POSIX/macOS when another process has the segment open
  - Previously would fail with `EINVAL` when attempting to truncate a shared memory segment that's open by another process
  - Now gracefully preserves existing size instead of failing
  - Documents limitation in platform notes
- Shorten test names to comply with macOS 31-character name limit for shared memory segments

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
