# slick-shm API Reference

## Table of Contents

- [Core Classes](#core-classes)
  - [shared_memory](#shared_memory)
  - [shared_memory_view](#shared_memory_view)
- [Types and Enums](#types-and-enums)
- [Error Handling](#error-handling)

## Core Classes

### shared_memory

The main RAII wrapper for cross-platform shared memory.

#### Constructors

**Throwing Variants:**

```cpp
// Create new shared memory (fail if exists)
shared_memory(const char* name, std::size_t size, create_only_t,
              access_mode mode = access_mode::read_write);

// Create or open shared memory
shared_memory(const char* name, std::size_t size, open_or_create_t,
              access_mode mode = access_mode::read_write);

// Create, resize if exists (see "Sizing an existing segment" below - the
// requested size is not guaranteed to be applied on every platform)
shared_memory(const char* name, std::size_t size, open_always_t,
              access_mode mode = access_mode::read_write);

// Open existing shared memory
shared_memory(const char* name, open_existing_t,
              access_mode mode = access_mode::read_write);
```

**Non-Throwing Variants (with std::nothrow):**

All constructors above have corresponding no-throw variants that take `std::nothrow` as the last parameter. Check `is_valid()` and `last_error()` after construction.

```cpp
shared_memory shm(name, size, create_only, access_mode::read_write, std::nothrow);
if (!shm.is_valid()) {
    std::cerr << "Error: " << shm.last_error().message() << std::endl;
}
```

#### Member Functions

##### data()

```cpp
void* data() noexcept;
const void* data() const noexcept;
```

Returns a pointer to the mapped shared memory. Returns `nullptr` if invalid.

##### size()

```cpp
std::size_t size() const noexcept;
```

Returns the size of the shared memory in bytes. Returns 0 if invalid.

##### name()

```cpp
const char* name() const noexcept;
```

Returns the name of the shared memory segment.

##### is_valid()

```cpp
bool is_valid() const noexcept;
```

Returns `true` if the shared memory is valid and mapped, `false` otherwise.

##### last_error()

```cpp
std::error_code last_error() const noexcept;
```

Returns the last error code. Useful for no-throw constructors.

##### mode()

```cpp
access_mode mode() const noexcept;
```

Returns the access mode (read_only or read_write).

##### is_creator()

```cpp
bool is_creator() const noexcept;
```

Returns `true` if this object created the shared memory segment, `false` if it opened an existing one.

**Use Cases:**
- Determine cleanup responsibilities
- Track ownership in multi-process applications
- Conditional behavior based on creator status

**Behavior:**
- `create_only`: Always returns `true` (only succeeds if creating new)
- `open_existing`: Always returns `false` (only succeeds if opening existing)
- `open_or_create`: Returns `true` if created new, `false` if opened existing
- `open_always`: Returns `true` if created new, `false` if opened existing
- After move operations: The flag is transferred to the moved-to object
- Invalid/default-constructed objects: Returns `false`

**Platform Details:**
- **Windows**: Creator status is determined by checking if `GetLastError()` returned `ERROR_ALREADY_EXISTS` after `CreateFileMapping()`
- **POSIX**: Creator status is tracked by the internal `owns_shm_` flag, set based on whether `shm_open()` was called with `O_CREAT | O_EXCL`

**Example:**
```cpp
shared_memory shm(name, 1024, open_or_create);
if (shm.is_creator()) {
    std::cout << "Created new shared memory" << std::endl;
    // Initialize data structures
} else {
    std::cout << "Opened existing shared memory" << std::endl;
    // Use existing data
}
```

##### unmap()

```cpp
void unmap() noexcept;
```

Manually unmap the shared memory. The handle remains open.

##### close()

```cpp
void close() noexcept;
```

Manually close and unmap the shared memory. Called automatically by destructor.

#### Static Functions

##### remove()

```cpp
static bool remove(const char* name) noexcept;
```

Remove/unlink a shared memory segment by name.

- **Windows**: No-op (returns true) as cleanup is automatic
- **POSIX**: Calls `shm_unlink()`

##### exists()

```cpp
static bool exists(const char* name) noexcept;
```

Check if a shared memory segment with the given name exists.

#### Example

```cpp
using namespace slick::shm;

// Create shared memory
shared_memory shm("my_shm", 1024, create_only);

// Write data
std::memcpy(shm.data(), "Hello, World!", 14);

// Open in another process
shared_memory reader("my_shm", open_existing, access_mode::read_only);
std::cout << static_cast<const char*>(reader.data()) << std::endl;

// Cleanup
shared_memory::remove("my_shm");
```

### shared_memory_view

A non-owning, lightweight view into shared memory.

> **Lifetime**: The view is non-owning in every respect. In particular `name()`
> returns a borrowed pointer to the segment name stored by the `shared_memory`
> object (or the caller-provided buffer) the view was built from — the source
> must outlive any view that references it.
>
> Moving the source is safe - `shared_memory` stores its name in address-stable
> storage, so a view survives a move or move-assignment of its source, including
> a `std::vector<shared_memory>` reallocation. Constructing a view from a
> temporary `shared_memory` is rejected at compile time.
>
> **Copy cost**: Copying a `shared_memory_view` is allocation-free (plain
> pointer/word copies), so views are cheap to pass by value, including into
> worker threads.

#### Constructors

```cpp
// Default constructor
shared_memory_view() noexcept;

// Construct from shared_memory
explicit shared_memory_view(const shared_memory& shm) noexcept;

// Construct from raw parameters
shared_memory_view(void* data, std::size_t size, const char* name,
                   access_mode mode = access_mode::read_write) noexcept;
```

#### Member Functions

Same as `shared_memory` for accessors:
- `data()`
- `size()`
- `name()`
- `is_valid()`
- `mode()`

#### Example

```cpp
shared_memory shm("test", 256, create_only);
shared_memory_view view(shm);

// Pass view to functions without transferring ownership
process_data(view);
```

## Types and Enums

### access_mode

```cpp
enum class access_mode {
    read_only,   // Read-only access
    read_write   // Read and write access
};
```

`access_mode` describes the calling process's own mapping, not the segment. A
segment created with `access_mode::read_only` gives *this* process a read-only
mapping; another process is still free to open the same segment read-write. This
is the behavior on every platform - on Windows the underlying section is always
created read-write so that a read-only creator cannot lock the segment against
future writers.

### create_mode

```cpp
enum class create_mode {
    create_only,      // Create new, fail if exists
    open_or_create,   // Open existing or create new
    open_always       // Create, or resize an existing segment where possible
};
```

#### Sizing an existing segment

`size` is a request, not a guarantee. It is applied exactly only when the call
creates the segment. When `open_or_create` or `open_always` finds an existing
segment, what happens to its size is platform-dependent:

| Platform | `open_always` against an existing segment of a different size |
|----------|--------------------------------------------------------------|
| Linux    | Resized to the requested size - it both grows and shrinks, even while other processes have it mapped, and those processes get `SIGBUS` past the new end |
| macOS    | Resized only when no other process has it open; otherwise the existing size is kept (`ftruncate` reports `EINVAL`) |
| Windows  | Never resized - the existing section is returned unchanged |

`open_or_create` never attempts a resize on any platform.

The consequence worth guarding against: **`size()` can come back smaller than
you asked for.** Always size your work from `size()` rather than from the value
you passed in.

```cpp
shared_memory shm("my_shm", 16384, open_always);

if (shm.size() < 16384) {
    // An existing, smaller segment this platform would not resize.
    // Writing 16384 bytes here would run off the end of the mapping.
}
```

Use `create_only` when the size has to be exact - it fails outright rather than
handing back a segment somebody else sized.

### Tag Types

```cpp
inline constexpr create_only_t create_only{};
inline constexpr open_or_create_t open_or_create{};
inline constexpr open_always_t open_always{};
inline constexpr open_existing_t open_existing{};
```

Use these tags for constructor overload resolution.

## Error Handling

### errc Enum

```cpp
enum class errc {
    success = 0,
    already_exists,
    not_found,
    permission_denied,
    invalid_argument,
    size_mismatch,
    mapping_failed,
    invalid_size,
    invalid_name,
    unknown_error
};
```

### shared_memory_error Exception

```cpp
class shared_memory_error : public std::system_error {
    // Thrown by throwing constructors on error
};
```

#### Example

```cpp
try {
    shared_memory shm("test", 1024, create_only);
} catch (const shared_memory_error& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    std::cerr << "Code: " << e.code().value() << std::endl;
}
```

### Error Category

```cpp
const std::error_category& shm_category() noexcept;
std::error_code make_error_code(errc e) noexcept;
```

## Thread Safety

- **Individual objects**: Not thread-safe. Do not access the same `shared_memory` object from multiple threads without synchronization.
- **Shared memory segment**: Multiple threads can use different `shared_memory` objects to access the same segment.
- **Data access**: User is responsible for synchronizing access to the shared data (e.g., using `std::atomic`, mutexes in shared memory, etc.).

## Platform Notes

See [platform_notes.md](platform_notes.md) for platform-specific behavior and limitations.
