// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant

#pragma once

#include <system_error>
#include <string>

namespace slick {
namespace shm {

// Error codes for shared memory operations
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

// Error category for shared memory errors
class error_category_impl : public std::error_category {
public:
    const char* name() const noexcept override {
        return "slick-shm";
    }

    std::string message(int ev) const override {
        switch (static_cast<errc>(ev)) {
            case errc::success:
                return "success";
            case errc::already_exists:
                return "shared memory already exists";
            case errc::not_found:
                return "shared memory not found";
            case errc::permission_denied:
                return "permission denied";
            case errc::invalid_argument:
                return "invalid argument";
            case errc::size_mismatch:
                return "size mismatch";
            case errc::mapping_failed:
                return "memory mapping failed";
            case errc::invalid_size:
                return "invalid size (must be greater than zero)";
            case errc::invalid_name:
                return "invalid shared memory name";
            case errc::unknown_error:
            default:
                return "unknown error";
        }
    }
};

// Get the shared memory error category
inline const std::error_category& shm_category() noexcept {
    static error_category_impl instance;
    return instance;
}

// Create an error_code from an errc value
inline std::error_code make_error_code(errc e) noexcept {
    return std::error_code(static_cast<int>(e), shm_category());
}

// Exception class for shared memory errors
class shared_memory_error : public std::system_error {
public:
    explicit shared_memory_error(errc e)
        : std::system_error(make_error_code(e)) {}

    explicit shared_memory_error(errc e, const std::string& what_arg)
        : std::system_error(make_error_code(e), what_arg) {}

    explicit shared_memory_error(errc e, const char* what_arg)
        : std::system_error(make_error_code(e), what_arg) {}

    explicit shared_memory_error(std::error_code ec)
        : std::system_error(ec) {}

    explicit shared_memory_error(std::error_code ec, const std::string& what_arg)
        : std::system_error(ec, what_arg) {}

    explicit shared_memory_error(std::error_code ec, const char* what_arg)
        : std::system_error(ec, what_arg) {}
};

}  // namespace shm
}  // namespace slick

// Make errc compatible with std::error_code
namespace std {
template <>
struct is_error_code_enum<slick::shm::errc> : true_type {};
}  // namespace std
