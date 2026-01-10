// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant

#pragma once

#include "types.hpp"
#include "error.hpp"
#include "detail/platform.hpp"

#ifdef SLICK_SHM_WINDOWS
#include "detail/windows/shared_memory_impl.hpp"
#elif defined(SLICK_SHM_POSIX)
#include "detail/posix/shared_memory_impl.hpp"
#endif

#include <utility>
#include <new>

namespace slick {
namespace shm {

/**
 * @brief RAII wrapper for cross-platform shared memory
 *
 * This class provides a simple interface for creating and accessing shared memory
 * across Windows, Linux, and macOS. It automatically manages the lifetime of the
 * shared memory mapping and handles platform-specific details.
 *
 * Thread safety: Individual shared_memory objects are not thread-safe.
 * Multiple threads can use different shared_memory objects to access the same
 * shared memory segment.
 */
class shared_memory {
public:
    /**
     * @brief Default constructor - creates an invalid shared memory object
     */
    shared_memory() = default;

    // ========================================================================
    // Creating new shared memory - Throwing variants
    // ========================================================================

    /**
     * @brief Create new shared memory (create_only mode)
     * @param name Name of the shared memory
     * @param size Size in bytes
     * @param tag create_only tag
     * @param mode Access mode (default: read_write)
     * @throws shared_memory_error if creation fails
     */
    shared_memory(const char* name, std::size_t size, create_only_t tag,
                  access_mode mode = access_mode::read_write)
        : impl_(), last_error_() {
        (void)tag;
        last_error_ = impl_.create(name, size, create_mode::create_only, mode);
        if (last_error_) {
            throw shared_memory_error(last_error_);
        }
    }

    /**
     * @brief Create or open shared memory (open_or_create mode)
     * @param name Name of the shared memory
     * @param size Size in bytes (used only if creating)
     * @param tag open_or_create tag
     * @param mode Access mode (default: read_write)
     * @throws shared_memory_error if operation fails
     */
    shared_memory(const char* name, std::size_t size, open_or_create_t tag,
                  access_mode mode = access_mode::read_write)
        : impl_(), last_error_() {
        (void)tag;
        last_error_ = impl_.create(name, size, create_mode::open_or_create, mode);
        if (last_error_) {
            throw shared_memory_error(last_error_);
        }
    }

    /**
     * @brief Create shared memory, truncate if exists (open_always mode)
     * @param name Name of the shared memory
     * @param size Size in bytes
     * @param tag open_always tag
     * @param mode Access mode (default: read_write)
     * @throws shared_memory_error if operation fails
     */
    shared_memory(const char* name, std::size_t size, open_always_t tag,
                  access_mode mode = access_mode::read_write)
        : impl_(), last_error_() {
        (void)tag;
        last_error_ = impl_.create(name, size, create_mode::open_always, mode);
        if (last_error_) {
            throw shared_memory_error(last_error_);
        }
    }

    /**
     * @brief Open existing shared memory
     * @param name Name of the shared memory
     * @param tag open_existing tag
     * @param mode Access mode (default: read_write)
     * @throws shared_memory_error if shared memory doesn't exist or open fails
     */
    shared_memory(const char* name, open_existing_t tag,
                  access_mode mode = access_mode::read_write)
        : impl_(), last_error_() {
        (void)tag;
        last_error_ = impl_.open(name, mode);
        if (last_error_) {
            throw shared_memory_error(last_error_);
        }
    }

    // ========================================================================
    // Creating new shared memory - Non-throwing variants
    // ========================================================================

    /**
     * @brief Create new shared memory (create_only mode) - no-throw version
     * @param name Name of the shared memory
     * @param size Size in bytes
     * @param tag create_only tag
     * @param mode Access mode
     * @param nt std::nothrow tag
     * @note Check is_valid() and last_error() after construction
     */
    shared_memory(const char* name, std::size_t size, create_only_t tag,
                  access_mode mode, const std::nothrow_t& nt) noexcept
        : impl_(), last_error_() {
        (void)tag;
        (void)nt;
        last_error_ = impl_.create(name, size, create_mode::create_only, mode);
    }

    /**
     * @brief Create or open shared memory (open_or_create mode) - no-throw version
     * @param name Name of the shared memory
     * @param size Size in bytes
     * @param tag open_or_create tag
     * @param mode Access mode
     * @param nt std::nothrow tag
     * @note Check is_valid() and last_error() after construction
     */
    shared_memory(const char* name, std::size_t size, open_or_create_t tag,
                  access_mode mode, const std::nothrow_t& nt) noexcept
        : impl_(), last_error_() {
        (void)tag;
        (void)nt;
        last_error_ = impl_.create(name, size, create_mode::open_or_create, mode);
    }

    /**
     * @brief Create shared memory, truncate if exists (open_always mode) - no-throw version
     * @param name Name of the shared memory
     * @param size Size in bytes
     * @param tag open_always tag
     * @param mode Access mode
     * @param nt std::nothrow tag
     * @note Check is_valid() and last_error() after construction
     */
    shared_memory(const char* name, std::size_t size, open_always_t tag,
                  access_mode mode, const std::nothrow_t& nt) noexcept
        : impl_(), last_error_() {
        (void)tag;
        (void)nt;
        last_error_ = impl_.create(name, size, create_mode::open_always, mode);
    }

    /**
     * @brief Open existing shared memory - no-throw version
     * @param name Name of the shared memory
     * @param tag open_existing tag
     * @param mode Access mode
     * @param nt std::nothrow tag
     * @note Check is_valid() and last_error() after construction
     */
    shared_memory(const char* name, open_existing_t tag, access_mode mode,
                  const std::nothrow_t& nt) noexcept
        : impl_(), last_error_() {
        (void)tag;
        (void)nt;
        last_error_ = impl_.open(name, mode);
    }

    /**
     * @brief Destructor - automatically unmaps and closes shared memory
     */
    ~shared_memory() = default;

    /**
     * @brief Deleted copy constructor
     */
    shared_memory(const shared_memory&) = delete;

    /**
     * @brief Deleted copy assignment
     */
    shared_memory& operator=(const shared_memory&) = delete;

    /**
     * @brief Move constructor
     */
    shared_memory(shared_memory&& other) noexcept = default;

    /**
     * @brief Move assignment
     */
    shared_memory& operator=(shared_memory&& other) noexcept = default;

    // ========================================================================
    // Accessors
    // ========================================================================

    /**
     * @brief Get pointer to shared memory data
     * @return Pointer to the mapped memory, or nullptr if invalid
     */
    void* data() noexcept {
        return impl_.data();
    }

    /**
     * @brief Get const pointer to shared memory data
     * @return Const pointer to the mapped memory, or nullptr if invalid
     */
    const void* data() const noexcept {
        return impl_.data();
    }

    /**
     * @brief Get the size of the shared memory in bytes
     * @return Size in bytes, or 0 if invalid
     */
    std::size_t size() const noexcept {
        return impl_.size();
    }

    /**
     * @brief Get the name of the shared memory
     * @return Name string, or empty if invalid
     */
    const char* name() const noexcept {
        return impl_.name();
    }

    /**
     * @brief Check if the shared memory is valid (mapped)
     * @return true if valid and mapped, false otherwise
     */
    bool is_valid() const noexcept {
        return impl_.is_valid();
    }

    /**
     * @brief Get the last error code (for no-throw constructors)
     * @return Error code from last operation
     */
    std::error_code last_error() const noexcept {
        return last_error_;
    }

    /**
     * @brief Get the access mode
     * @return Access mode (read_only or read_write)
     */
    access_mode mode() const noexcept {
        return impl_.mode();
    }

    // ========================================================================
    // Manual control
    // ========================================================================

    /**
     * @brief Manually unmap the shared memory
     * @note The shared memory handle remains open
     */
    void unmap() noexcept {
        impl_.unmap();
    }

    /**
     * @brief Manually close and unmap the shared memory
     * @note This is called automatically by the destructor
     */
    void close() noexcept {
        impl_.close();
    }

    // ========================================================================
    // Static utilities
    // ========================================================================

    /**
     * @brief Remove/unlink a shared memory segment by name
     * @param name Name of the shared memory to remove
     * @return true if removed successfully, false otherwise
     * @note On Windows, this is a no-op (returns true) as cleanup is automatic
     * @note On POSIX, this calls shm_unlink()
     */
    static bool remove(const char* name) noexcept {
        return detail::platform_shared_memory::remove(name);
    }

    /**
     * @brief Check if a shared memory segment with the given name exists
     * @param name Name of the shared memory
     * @return true if exists, false otherwise
     */
    static bool exists(const char* name) noexcept {
        return detail::platform_shared_memory::exists(name);
    }

private:
    detail::platform_shared_memory impl_;
    std::error_code last_error_;
};

}  // namespace shm
}  // namespace slick
