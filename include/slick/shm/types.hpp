// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant

#pragma once

#include <cstddef>

namespace slick {
namespace shm {

// Access mode for shared memory
enum class access_mode {
    read_only,   // Read-only access
    read_write   // Read and write access
};

// Create modes for shared memory creation (internal use)
enum class create_mode {
    create_only,      // Create new shared memory, fail if it already exists
    open_or_create,   // Open existing or create new if doesn't exist
    open_always       // Always create (truncate if exists)
};

// Tag types for constructor overload resolution
struct create_only_t {
    explicit create_only_t() = default;
};

struct open_or_create_t {
    explicit open_or_create_t() = default;
};

struct open_always_t {
    explicit open_always_t() = default;
};

struct open_existing_t {
    explicit open_existing_t() = default;
};

// Tag instances
inline constexpr create_only_t create_only{};
inline constexpr open_or_create_t open_or_create{};
inline constexpr open_always_t open_always{};
inline constexpr open_existing_t open_existing{};

}  // namespace shm
}  // namespace slick
