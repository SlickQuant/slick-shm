// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant

#pragma once

#include <slick/shm/shared_memory.hpp>

#include <chrono>
#include <string>

namespace slick_shm_test {

// Builds a segment name that is unique per run. Deliberately short: macOS caps
// shared memory names at 31 characters, including the leading '/'.
inline std::string unique_name(const char* prefix) {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return std::string(prefix) + std::to_string(millis % 100000000);
}

// Removes the named segment when the scope ends. A no-op on Windows, where the
// kernel reclaims the segment once the last handle closes.
struct shm_cleanup {
    std::string name;
    ~shm_cleanup() {
        slick::shm::shared_memory::remove(name.c_str());
    }
};

}  // namespace slick_shm_test
