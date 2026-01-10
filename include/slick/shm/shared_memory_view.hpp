// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant

#pragma once

#include "shared_memory.hpp"
#include <string>

namespace slick {
namespace shm {

// Forward declaration
class shared_memory;

/**
 * @brief Non-owning view into shared memory
 *
 * This class provides a lightweight, copyable view into shared memory without
 * managing its lifetime. Useful for passing shared memory references around
 * without transferring ownership.
 *
 * Thread safety: Individual shared_memory_view objects are not thread-safe.
 */
class shared_memory_view {
public:
    /**
     * @brief Default constructor - creates an invalid view
     */
    shared_memory_view() noexcept
        : data_(nullptr), size_(0), name_(), mode_(access_mode::read_write) {}

    /**
     * @brief Construct view from a shared_memory object
     * @param shm The shared_memory object to view
     */
    explicit shared_memory_view(const shared_memory& shm) noexcept;

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

    // Copyable
    shared_memory_view(const shared_memory_view&) = default;
    shared_memory_view& operator=(const shared_memory_view&) = default;

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
     * @return Name string, or empty if invalid
     */
    const char* name() const noexcept {
        return name_.c_str();
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
    std::string name_;
    access_mode mode_;
};

// Implementation of constructor from shared_memory
// This is defined here after shared_memory_view is fully defined
inline shared_memory_view::shared_memory_view(const shared_memory& shm) noexcept
    : data_(const_cast<void*>(shm.data())),
      size_(shm.size()),
      name_(shm.name()),
      mode_(shm.mode()) {}

}  // namespace shm
}  // namespace slick
