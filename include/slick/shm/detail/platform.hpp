// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant

#pragma once

#include <cstddef>
#include <cstring>

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define SLICK_SHM_WINDOWS
#elif defined(__unix__) || defined(__APPLE__)
    #define SLICK_SHM_POSIX
    #if defined(__linux__)
        #define SLICK_SHM_LINUX
    #elif defined(__APPLE__)
        #define SLICK_SHM_MACOS
    #endif
#else
    #error "Unsupported platform"
#endif

namespace slick {
namespace shm {
namespace detail {

// Platform constants
constexpr std::size_t MAX_NAME_LENGTH = 255;

// Name validation
inline bool is_valid_name(const char* name) {
    if (!name || !*name) {
        return false;
    }

    std::size_t len = std::strlen(name);
    if (len == 0 || len > MAX_NAME_LENGTH) {
        return false;
    }

#ifdef SLICK_SHM_WINDOWS
    // Windows: check for invalid characters
    // Invalid characters: \ / : * ? " < > |
    for (std::size_t i = 0; i < len; ++i) {
        char c = name[i];
        if (c == '\\' || c == '/' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            return false;
        }
    }
#else
    // POSIX: name can contain '/' only at the start
    if (len == 1 && name[0] == '/') {
        return false;
    }
    for (std::size_t i = 1; i < len; ++i) {
        if (name[i] == '/') {
            return false;
        }
    }
#endif

    return true;
}

}  // namespace detail
}  // namespace shm
}  // namespace slick
