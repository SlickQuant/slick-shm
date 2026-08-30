// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant

#pragma once

#include "shared_memory.hpp"

namespace slick {
namespace shm {

/**
 * @brief Non-owning view into shared memory
 *
 * This class provides a lightweight, copyable view into shared memory without
 * managing its lifetime. Useful for passing shared memory references around
 * without transferring ownership.
 *
 * @note The view owns nothing, name included: name() hands back a pointer into
 *       storage owned by the shared_memory object (or the caller-provided
 *       buffer) it was built from, so that source must outlive the view.
 *       Moving the source is safe - shared_memory holds its name in
 *       address-stable storage, so views stay valid across a move of it, a
 *       std::vector<shared_memory> reallocation included.
 *
 * @note Copying a view is allocation-free: copies are plain pointer copies.
 *
 * Thread safety: Individual shared_memory_view objects are not thread-safe.
 */
class shared_memory_view {
public:
    /**
     * @brief Default constructor - creates an invalid view
     */
    shared_memory_view() noexcept
        : data_(nullptr), size_(0), name_(""), mode_(access_mode::read_write) {}

    /**
     * @brief Construct view from a shared_memory object
     * @param shm The shared_memory object to view
     */
    explicit shared_memory_view(const shared_memory& shm) noexcept
        : data_(const_cast<void*>(shm.data())),
          size_(shm.size()),
          name_(shm.name()),
          mode_(shm.mode()) {}

    /**
     * @brief Deleted - a view cannot be built from a temporary
     *
     * Every member of the view would dangle the moment the full expression
     * ends, so this is rejected at compile time rather than at runtime.
     */
    explicit shared_memory_view(shared_memory&&) = delete;

    /**
     * @brief Construct view from raw parameters
     * @param data Pointer to shared memory data
     * @param size Size in bytes
     * @param name Name of the shared memory
     * @param mode Access mode
     */
    shared_memory_view(void* data, std::size_t size, const char* name,
                       access_mode mode = access_mode::read_write) noexcept
        : data_(data), size_(size), name_(name ? name : ""), mode_(mode) {}

    // Copyable - cheap, allocation-free (plain pointer members)
    shared_memory_view(const shared_memory_view&) noexcept = default;
    shared_memory_view& operator=(const shared_memory_view&) noexcept = default;

    // Moveable
    shared_memory_view(shared_memory_view&&) noexcept = default;
    shared_memory_view& operator=(shared_memory_view&&) noexcept = default;

    /**
     * @brief Get pointer to shared memory data
     * @return Pointer to the memory, or nullptr if invalid
     */
    void* data() noexcept {
        return data_;
    }

    /**
     * @brief Get const pointer to shared memory data
     * @return Const pointer to the memory, or nullptr if invalid
     */
    const void* data() const noexcept {
        return data_;
    }

    /**
     * @brief Get the size of the shared memory in bytes
     * @return Size in bytes, or 0 if invalid
     */
    std::size_t size() const noexcept {
        return size_;
    }

    /**
     * @brief Get the name of the shared memory
     * @return Borrowed pointer to the name, or "" if the view carries none. It
     *         is owned by the source the view was built from and stays valid
     *         for as long as that source lives, across moves of it.
     */
    const char* name() const noexcept {
        return name_;
    }

    /**
     * @brief Check if the view is valid
     * @return true if valid (non-null data), false otherwise
     */
    bool is_valid() const noexcept {
        return data_ != nullptr;
    }

    /**
     * @brief Get the access mode
     * @return Access mode (read_only or read_write)
     */
    access_mode mode() const noexcept {
        return mode_;
    }

private:
    void* data_;
    std::size_t size_;
    const char* name_;  // Non-owning; must outlive the view
    access_mode mode_;
};

}  // namespace shm
}  // namespace slick
