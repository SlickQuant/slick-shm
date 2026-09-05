# Platform-Specific Notes

This document describes platform-specific behavior and considerations for slick-shm.

## Table of Contents

- [Windows](#windows)
- [Linux](#linux)
- [macOS](#macos)
- [Name Requirements](#name-requirements)
- [Size Limits](#size-limits)
- [Permissions](#permissions)
- [Cleanup Behavior](#cleanup-behavior)

## Windows

### Implementation

- Uses `CreateFileMapping()` / `OpenFileMapping()` for creation/opening
- Uses `MapViewOfFile()` for mapping
- Backed by the system paging file

### Name Format

- Names are used as-is (no automatic prefix)
- User can optionally prefix with an object namespace (matched case-insensitively):
  - `Global\` for cross-session access (requires appropriate privileges)
  - `Local\` for session-local access (default if no prefix)
  - `Session\<id>\` for a specific terminal services session
- Invalid characters: `\ / : * ? " < > |` - the only backslash a name may contain
  is the separator of a leading namespace prefix
- Maximum length: 255 characters, prefix included

### Example Names

```cpp
// Local to current session
shared_memory shm("my_shm", 1024, create_only);

// Global (cross-session, requires privileges)
shared_memory shm("Global\\my_shm", 1024, create_only);

// Local (explicit)
shared_memory shm("Local\\my_shm", 1024, create_only);

// Specific session
shared_memory shm("Session\\1\\my_shm", 1024, create_only);
```

### Size Limits

- Maximum size: Limited by available address space
- 32-bit: ~2-3 GB practical limit
- 64-bit: Very large (limited by physical + virtual memory)

**Important**: Windows rounds shared memory sizes up to the system's page size (typically 4KB) or allocation granularity (typically 64KB). This means:
- Creating a 2048-byte region may actually allocate 4096 bytes
- The `size()` method returns the actual allocated size, not the requested size
- Both creator and opener will see the same rounded-up size

### Cleanup

- **Automatic**: Kernel automatically cleans up when last handle is closed
- `shared_memory::remove()` is a no-op (always returns true)
- Crash recovery: Automatic

### Permissions

- Controlled by ACLs (Access Control Lists)
- Default: Inherit from process
- Fine-grained control not exposed in this library (uses defaults)

## Linux

### Implementation

- Uses POSIX shared memory: `shm_open()` / `shm_unlink()`
- Uses `mmap()` for mapping
- Backed by tmpfs (typically `/dev/shm`)

### Name Format

- Names must start with `/`
- Library automatically prefixes with `/` if not present
- Cannot contain `/` except at the start
- Maximum length: Typically 255 characters (NAME_MAX)

### Example Names

```cpp
// Automatically prefixed with '/'
shared_memory shm("my_shm", 1024, create_only);  // Becomes "/my_shm"

// Explicit prefix
shared_memory shm("/my_shm", 1024, create_only);
```

### Size Limits

Bounded by the tmpfs mounted at `/dev/shm`, whose size caps the total of all
POSIX shared memory on the system:

- Default size: half of physical RAM
- Check available space: `df -h /dev/shm`
- Increase: `sudo mount -o remount,size=8G /dev/shm` (or an `/etc/fstab` entry)
- Docker defaults to only 64 MB - raise it with `docker run --shm-size=1g`
- Under cgroups, pages are charged to the process that first touches them

**Important**: `kernel.shmmax` and `kernel.shmall` do **not** apply. Those govern
System V shared memory (`shmget()`), a separate mechanism this library does not
use. Since Linux 4.14 their defaults are effectively unlimited anyway.

**Important**: `ftruncate()` extends the object sparsely, so requesting more than
`/dev/shm` can supply may succeed at creation and fail much later - a process
touching a page tmpfs cannot back receives `SIGBUS`. Size segments against
`df -h /dev/shm`.

### Cleanup

- **Manual**: Requires explicit `shm_unlink()`
- `shared_memory::remove()` calls `shm_unlink()`
- Crash recovery: Must manually clean up
- List existing: `ls /dev/shm`

### Permissions

- Default: 0666 (modified by umask)
- Actual permissions = 0666 & ~umask
- Example: umask=0022 → permissions=0644 (rw-r--r--)

## macOS

### Implementation

- Uses POSIX shared memory: `shm_open()` / `shm_unlink()`
- Uses `mmap()` for mapping
- May use different backing store than Linux

### Name Format

- Same as Linux (must start with `/`)
- Library automatically prefixes with `/` if not present
- Cannot contain `/` except at the start
- Maximum length: 31 characters (much shorter than Linux!)

### Example Names

```cpp
// Keep names short on macOS!
shared_memory shm("shm", 1024, create_only);  // Becomes "/shm"
```

### Size Limits

- No dedicated tunable: POSIX shared memory is backed by anonymous memory, so the
  practical ceiling is physical memory plus swap, and the process address space
- There is no `/dev/shm` on macOS; objects are not visible in the filesystem
- Sizes are fixed at creation in practice: `ftruncate()` fails with `EINVAL` once
  another process has the object open, so every opener sees the creator's size

**Important**: `kern.sysv.shmmax` and friends do **not** apply. Those govern
System V shared memory (`shmget()`), which this library does not use.

### Cleanup

- Same as Linux: manual cleanup required
- Use `shared_memory::remove()`

### Permissions

- Same as Linux: 0666 modified by umask

## Name Requirements

### Cross-Platform Safe Names

To ensure portability across all platforms:

1. **Length**: Keep names ≤ 31 characters (macOS limit)
2. **Characters**: Use only alphanumeric and underscore: `[a-zA-Z0-9_]`
3. **Start**: Start with a letter or underscore
4. **No prefix**: Let the library handle platform-specific prefixes

### Examples

```cpp
// Good - cross-platform safe
"my_shared_memory"
"shm_buffer_1"
"data_exchange"

// Avoid - may not work on all platforms
"my-shared-memory"          // Contains '-'
"very_long_shared_memory_name_that_exceeds_limit"  // Too long for macOS
"Global\\my_shm"            // Windows-specific
"/my_shm"                   // POSIX-specific (but library handles this)
```

## Size Limits

### Summary Table

| Platform | Backing Store | Typical Ceiling |
|----------|---------------|-----------------|
| Windows 64-bit | Paging file | TB range (address space) |
| Windows 32-bit | Paging file | ~2-3 GB (address space) |
| Linux | `/dev/shm` tmpfs | Half of RAM (64 MB under Docker) |
| macOS | Anonymous memory | Physical memory + swap |

### Recommendations

- **Small data** (< 1 MB): Works everywhere
- **Medium data** (1 MB - 100 MB): Check `df -h /dev/shm` on Linux; Docker's
  64 MB default is the limit most often hit first
- **Large data** (> 100 MB): Remount `/dev/shm` larger on Linux, or run
  containers with `--shm-size`

## Permissions

### Windows

- Uses process security context
- Default permissions typically sufficient
- ACLs not exposed in this library
- The file mapping is always created with `PAGE_READWRITE`; `access_mode::read_only`
  restricts the view this process maps, not the segment. Creating with
  `PAGE_READONLY` would bar every future process from ever mapping it for writing,
  which POSIX does not do.

### POSIX (Linux/macOS)

- Default: 0666 & ~umask
- Results in typically 0644 (rw-r--r--)
- Read-write creator, read-only others
- No fine-grained control in this library (uses defaults)

## Cleanup Behavior

### Windows

```cpp
shared_memory shm("test", 1024, create_only);
// ... use shared memory ...
// Cleanup is automatic when shm goes out of scope
// No need to call remove()
```

### POSIX (Linux/macOS)

```cpp
shared_memory shm("test", 1024, create_only);
// ... use shared memory ...
shm.close();  // Optional: explicit close

// IMPORTANT: Must remove to prevent leak
shared_memory::remove("test");
```

A construction that fails cleans up after itself: if the segment was created by
this call and a later step (sizing or mapping) fails, it is unlinked again
before the error is returned. Only successfully created segments need
`remove()`.

### Best Practice

For portable code, always call `remove()`:

```cpp
{
    shared_memory shm("test", 1024, create_only);
    // ... use shared memory ...
}  // Automatic cleanup

// Explicit remove (no-op on Windows, removes on POSIX)
shared_memory::remove("test");
```

## Known Issues and Limitations

### All Platforms

- No built-in synchronization (use `std::atomic` or external locks)
- No automatic size discovery on creation (must specify size)
- No resizing support

### Windows

- `remove()` is a no-op (not needed)
- Global namespace requires admin privileges in some scenarios

### POSIX

- Shared memory persists after crash (manual cleanup needed)
- Short name limit on macOS (31 chars)
- System limits may need adjustment for large allocations
- **`open_always` does not guarantee the requested size**: only Linux actually resizes an existing segment. macOS resizes one only when no other process has it open - otherwise `ftruncate` reports `EINVAL` and the library keeps the existing size rather than failing. Windows never resizes an existing section at all; `CreateFileMapping()` returns it as it stands and the requested size is ignored. So `size()` can be **smaller than requested** on macOS and Windows, and on Linux an existing segment can be **shrunk while other processes have it mapped**, which leaves them facing `SIGBUS` past the new end. Always drive your writes from `size()`, not from the size you passed in, and use `create_only` when the size has to be exact. See [Sizing an existing segment](api_reference.md#sizing-an-existing-segment) in the API reference.

## Debugging

### Windows

```powershell
# No direct way to list shared memory objects
# Use Process Explorer or similar tools
```

### Linux

```bash
# List shared memory segments
ls -lh /dev/shm

# Check the size limit (the /dev/shm mount, not kernel.shmmax)
df -h /dev/shm

# Remove orphaned segment
rm /dev/shm/my_shm
```

### macOS

```bash
# Similar to Linux but names are in a different location
# Use `ipcs -m` to list shared memory segments
ipcs -m

# Remove with ipcrm or shm_unlink
```

## Summary

- **Name portability**: Use short, alphanumeric names
- **Size limits**: Check system defaults for large allocations
- **Cleanup**: Always call `remove()` for portable code
- **Permissions**: Defaults are usually sufficient
- **Windows vs POSIX**: Main differences are in cleanup and naming
