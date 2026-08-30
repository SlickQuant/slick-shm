// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant

#pragma once

#include <cstddef>
#include <cstring>
#include <memory>
#include "../error.hpp"
#include "../types.hpp"

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

// Address-stable storage for the name handed out by shared_memory::name().
//
// shared_memory_view borrows that pointer, so the characters must stay put when
// the owning shared_memory is moved - only the owning pointer moves, which keeps
// views built from the source valid. A std::string cannot promise that: small
// names live inside the string object itself, so moving the owner would relocate
// them and leave every view pointing into the moved-from object.
class stable_name {
public:
    stable_name() = default;

    stable_name(stable_name&&) noexcept = default;
    stable_name& operator=(stable_name&&) noexcept = default;

    stable_name(const stable_name&) = delete;
    stable_name& operator=(const stable_name&) = delete;

    stable_name& operator=(const char* name) {
        if (name == nullptr) {
            buffer_.reset();
            return *this;
        }
        std::size_t len = std::strlen(name);
        std::unique_ptr<char[]> buffer(new char[len + 1]);
        std::memcpy(buffer.get(), name, len + 1);
        buffer_ = std::move(buffer);
        return *this;
    }

    const char* c_str() const noexcept {
        return buffer_ ? buffer_.get() : "";
    }

private:
    std::unique_ptr<char[]> buffer_;
};

#ifdef SLICK_SHM_WINDOWS

// Case-insensitive match of a lower-case ASCII prefix. Object namespace prefixes
// are ASCII and are matched case-insensitively by the kernel.
inline bool starts_with_ci(const char* name, std::size_t len,
                           const char* lower_prefix, std::size_t prefix_len) {
    if (len < prefix_len) {
        return false;
    }
    for (std::size_t i = 0; i < prefix_len; ++i) {
        char c = name[i];
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
        if (c != lower_prefix[i]) {
            return false;
        }
    }
    return true;
}

// Length of the leading object-namespace prefix ("Global\", "Local\" or
// "Session\<id>\"), including its trailing backslash. Returns 0 when the name
// carries no such prefix.
inline std::size_t namespace_prefix_length(const char* name, std::size_t len) {
    if (starts_with_ci(name, len, "global\\", 7)) {
        return 7;
    }
    if (starts_with_ci(name, len, "local\\", 6)) {
        return 6;
    }
    if (starts_with_ci(name, len, "session\\", 8)) {
        // Session\<decimal session id>\<object name>
        std::size_t i = 8;
        while (i < len && name[i] >= '0' && name[i] <= '9') {
            ++i;
        }
        if (i > 8 && i < len && name[i] == '\\') {
            return i + 1;
        }
    }
    return 0;
}

#endif  // SLICK_SHM_WINDOWS

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
    // Windows: an optional leading object-namespace prefix is allowed, so the
    // only legal backslash is the separator inside that prefix.
    std::size_t start = namespace_prefix_length(name, len);
    if (start == len) {
        return false;  // prefix with no object name after it
    }

    // Invalid characters: \ / : * ? " < > |
    for (std::size_t i = start; i < len; ++i) {
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
