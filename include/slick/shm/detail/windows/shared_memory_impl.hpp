// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant

#pragma once

#ifdef SLICK_SHM_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>
#include <system_error>

namespace slick {
namespace shm {
namespace detail {

// Helper to convert string to platform string type
#ifdef UNICODE
using platform_string = std::wstring;
inline std::wstring to_platform_string(const char* str) {
    if (!str) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
    if (size == 0) return L"";
    std::wstring result(size, L'\0');
    int written = MultiByteToWideChar(CP_UTF8, 0, str, -1, &result[0], size);
    if (written == 0) return L"";
    if (!result.empty() && result.back() == L'\0') {
        result.pop_back();
    }
    return result;
}
#else
using platform_string = std::string;
inline std::string to_platform_string(const char* str) {
    return str ? std::string(str) : std::string();
}
#endif

class platform_shared_memory {
public:
    platform_shared_memory() = default;

    ~platform_shared_memory() {
        close_impl();
    }

    // Non-copyable
    platform_shared_memory(const platform_shared_memory&) = delete;
    platform_shared_memory& operator=(const platform_shared_memory&) = delete;

    // Moveable
    platform_shared_memory(platform_shared_memory&& other) noexcept
        : file_mapping_handle_(other.file_mapping_handle_),
          mapped_view_(other.mapped_view_),
          name_(std::move(other.name_)),
          name_utf8_(std::move(other.name_utf8_)),
          size_(other.size_),
          mode_(other.mode_) {
        other.file_mapping_handle_ = INVALID_HANDLE_VALUE;
        other.mapped_view_ = nullptr;
        other.size_ = 0;
    }

    platform_shared_memory& operator=(platform_shared_memory&& other) noexcept {
        if (this != &other) {
            close_impl();

            file_mapping_handle_ = other.file_mapping_handle_;
            mapped_view_ = other.mapped_view_;
            name_ = std::move(other.name_);
            name_utf8_ = std::move(other.name_utf8_);
            size_ = other.size_;
            mode_ = other.mode_;

            other.file_mapping_handle_ = INVALID_HANDLE_VALUE;
            other.mapped_view_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    std::error_code create(const char* name, std::size_t size, create_mode mode, access_mode access) {
        if (!is_valid_name(name)) {
            return make_error_code(errc::invalid_name);
        }

        if (size == 0) {
            return make_error_code(errc::invalid_size);
        }

        name_ = to_platform_string(name);
        name_utf8_ = name;
        size_ = size;
        mode_ = access;

        DWORD protect = get_protection_flags(access);

        // Split 64-bit size into high and low 32-bit parts
        DWORD size_high = static_cast<DWORD>((size >> 32) & 0xFFFFFFFF);
        DWORD size_low = static_cast<DWORD>(size & 0xFFFFFFFF);

        if (mode == create_mode::create_only) {
            // Try to create, fail if exists
            file_mapping_handle_ = CreateFileMapping(
                INVALID_HANDLE_VALUE,  // Use paging file
                nullptr,               // Default security
                protect,
                size_high,
                size_low,
                name_.c_str()
            );

            if (file_mapping_handle_ == nullptr) {
                return get_last_error();
            }

            // Check if it already existed
            if (GetLastError() == ERROR_ALREADY_EXISTS) {
                CloseHandle(file_mapping_handle_);
                file_mapping_handle_ = INVALID_HANDLE_VALUE;
                return make_error_code(errc::already_exists);
            }
        } else {
            // open_or_create or open_always
            file_mapping_handle_ = CreateFileMapping(
                INVALID_HANDLE_VALUE,
                nullptr,
                protect,
                size_high,
                size_low,
                name_.c_str()
            );

            if (file_mapping_handle_ == nullptr) {
                return get_last_error();
            }
        }

        return map_impl();
    }

    std::error_code open(const char* name, access_mode access) {
        if (!is_valid_name(name)) {
            return make_error_code(errc::invalid_name);
        }

        name_ = to_platform_string(name);
        name_utf8_ = name;
        mode_ = access;

        DWORD desired_access = get_map_access(access);

        file_mapping_handle_ = OpenFileMapping(
            desired_access,
            FALSE,  // Don't inherit handle
            name_.c_str()
        );

        if (file_mapping_handle_ == nullptr) {
            DWORD err = GetLastError();
            if (err == ERROR_FILE_NOT_FOUND) {
                return make_error_code(errc::not_found);
            }
            return std::error_code(err, std::system_category());
        }

        // map_impl() will set the size for us
        return map_impl();
    }

    void unmap() noexcept {
        unmap_impl();
    }

    void close() noexcept {
        close_impl();
    }

    void* data() noexcept {
        return mapped_view_;
    }

    const void* data() const noexcept {
        return mapped_view_;
    }

    std::size_t size() const noexcept {
        return size_;
    }

    const char* name() const noexcept {
        return name_utf8_.c_str();
    }

    bool is_valid() const noexcept {
        return mapped_view_ != nullptr;
    }

    access_mode mode() const noexcept {
        return mode_;
    }

    static bool remove(const char* name) noexcept {
        // On Windows, shared memory is automatically cleaned up when
        // the last handle is closed. There's no explicit remove operation.
        // We return true to indicate "success" (no-op).
        (void)name;
        return true;
    }

    static bool exists(const char* name) noexcept {
        if (!is_valid_name(name)) {
            return false;
        }

        platform_string platform_name = to_platform_string(name);
        HANDLE h = OpenFileMapping(FILE_MAP_READ, FALSE, platform_name.c_str());
        if (h != nullptr) {
            CloseHandle(h);
            return true;
        }
        return false;
    }

private:
    HANDLE file_mapping_handle_ = INVALID_HANDLE_VALUE;
    void* mapped_view_ = nullptr;
    platform_string name_;          // std::wstring in UNICODE, std::string otherwise
    std::string name_utf8_;         // Always UTF-8 for name() accessor
    std::size_t size_ = 0;
    access_mode mode_ = access_mode::read_write;

    std::error_code map_impl() {
        if (file_mapping_handle_ == INVALID_HANDLE_VALUE) {
            return make_error_code(errc::mapping_failed);
        }

        DWORD access = get_map_access(mode_);

        mapped_view_ = MapViewOfFile(
            file_mapping_handle_,
            access,
            0,  // Offset high
            0,  // Offset low
            0   // Map entire file
        );

        if (mapped_view_ == nullptr) {
            return get_last_error();
        }

        // Query the actual size of the mapped region
        // Windows rounds up to page/allocation granularity
        MEMORY_BASIC_INFORMATION info;
        if (VirtualQuery(mapped_view_, &info, sizeof(info)) == 0) {
            UnmapViewOfFile(mapped_view_);
            mapped_view_ = nullptr;
            return get_last_error();
        }

        // Update size to reflect actual allocated size
        size_ = info.RegionSize;

        return {};
    }

    void unmap_impl() noexcept {
        if (mapped_view_ != nullptr) {
            UnmapViewOfFile(mapped_view_);
            mapped_view_ = nullptr;
        }
    }

    void close_impl() noexcept {
        unmap_impl();

        if (file_mapping_handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(file_mapping_handle_);
            file_mapping_handle_ = INVALID_HANDLE_VALUE;
        }

        size_ = 0;
    }

    static DWORD get_protection_flags(access_mode mode) {
        switch (mode) {
            case access_mode::read_only:
                return PAGE_READONLY;
            case access_mode::read_write:
                return PAGE_READWRITE;
            default:
                return PAGE_READWRITE;
        }
    }

    static DWORD get_map_access(access_mode mode) {
        switch (mode) {
            case access_mode::read_only:
                return FILE_MAP_READ;
            case access_mode::read_write:
                return FILE_MAP_ALL_ACCESS;
            default:
                return FILE_MAP_ALL_ACCESS;
        }
    }

    static std::error_code get_last_error() {
        DWORD err = GetLastError();
        return std::error_code(err, std::system_category());
    }
};

}  // namespace detail
}  // namespace shm
}  // namespace slick

#endif  // SLICK_SHM_WINDOWS
