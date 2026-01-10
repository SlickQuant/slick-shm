// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant

#pragma once

#ifdef SLICK_SHM_POSIX

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

#include <string>
#include <system_error>

namespace slick {
namespace shm {
namespace detail {

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
        : shm_fd_(other.shm_fd_),
          mapped_addr_(other.mapped_addr_),
          name_(std::move(other.name_)),
          original_name_(std::move(other.original_name_)),
          size_(other.size_),
          mode_(other.mode_),
          owns_shm_(other.owns_shm_) {
        other.shm_fd_ = -1;
        other.mapped_addr_ = nullptr;
        other.size_ = 0;
        other.owns_shm_ = false;
    }

    platform_shared_memory& operator=(platform_shared_memory&& other) noexcept {
        if (this != &other) {
            close_impl();

            shm_fd_ = other.shm_fd_;
            mapped_addr_ = other.mapped_addr_;
            name_ = std::move(other.name_);
            original_name_ = std::move(other.original_name_);
            size_ = other.size_;
            mode_ = other.mode_;
            owns_shm_ = other.owns_shm_;

            other.shm_fd_ = -1;
            other.mapped_addr_ = nullptr;
            other.size_ = 0;
            other.owns_shm_ = false;
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

        original_name_ = name;  // Store original name for accessor
        name_ = format_name(name);  // Store formatted name for POSIX API
        size_ = size;
        mode_ = access;

        int flags = get_open_flags(mode, access);
        int perms = 0666;  // Default permissions (modified by umask)

        if (mode == create_mode::create_only) {
            // Create only, fail if exists
            flags |= O_EXCL;
            shm_fd_ = shm_open(name_.c_str(), flags, perms);

            if (shm_fd_ == -1) {
                if (errno == EEXIST) {
                    return make_error_code(errc::already_exists);
                }
                return get_errno_error();
            }
            owns_shm_ = true;
        } else {
            // open_or_create or open_always
            shm_fd_ = shm_open(name_.c_str(), flags, perms);

            if (shm_fd_ == -1) {
                return get_errno_error();
            }
            owns_shm_ = true;

            // Check if the segment already existed by checking its size
            struct stat sb;
            if (fstat(shm_fd_, &sb) == -1) {
                std::error_code ec = get_errno_error();
                ::close(shm_fd_);
                shm_fd_ = -1;
                if (owns_shm_) {
                    shm_unlink(name_.c_str());
                    owns_shm_ = false;
                }
                return ec;
            }

            if (sb.st_size == 0 || mode == create_mode::open_always) {
                // New segment (size is 0) or open_always mode - set the size
                if (ftruncate(shm_fd_, static_cast<off_t>(size_)) == -1) {
                    std::error_code ec = get_errno_error();
                    ::close(shm_fd_);
                    shm_fd_ = -1;
                    if (owns_shm_) {
                        shm_unlink(name_.c_str());
                        owns_shm_ = false;
                    }
                    return ec;
                }

                // Query the actual allocated size after truncate (OS may round up)
                if (fstat(shm_fd_, &sb) == -1) {
                    std::error_code ec = get_errno_error();
                    ::close(shm_fd_);
                    shm_fd_ = -1;
                    if (owns_shm_) {
                        shm_unlink(name_.c_str());
                        owns_shm_ = false;
                    }
                    return ec;
                }
            }
            // For existing segments in open_or_create mode, use existing size
            size_ = static_cast<std::size_t>(sb.st_size);
            return map_impl();
        }

        // For create_only mode, set the size using ftruncate
        if (ftruncate(shm_fd_, static_cast<off_t>(size_)) == -1) {
            std::error_code ec = get_errno_error();
            ::close(shm_fd_);
            shm_fd_ = -1;
            if (owns_shm_) {
                shm_unlink(name_.c_str());
                owns_shm_ = false;
            }
            return ec;
        }

        // Query the actual allocated size (OS may round up)
        struct stat sb;
        if (fstat(shm_fd_, &sb) == -1) {
            std::error_code ec = get_errno_error();
            ::close(shm_fd_);
            shm_fd_ = -1;
            if (owns_shm_) {
                shm_unlink(name_.c_str());
                owns_shm_ = false;
            }
            return ec;
        }

        size_ = static_cast<std::size_t>(sb.st_size);

        return map_impl();
    }

    std::error_code open(const char* name, access_mode access) {
        if (!is_valid_name(name)) {
            return make_error_code(errc::invalid_name);
        }

        original_name_ = name;  // Store original name for accessor
        name_ = format_name(name);  // Store formatted name for POSIX API
        mode_ = access;
        owns_shm_ = false;  // We didn't create it, so don't unlink it

        int flags = (access == access_mode::read_only) ? O_RDONLY : O_RDWR;

        shm_fd_ = shm_open(name_.c_str(), flags, 0);

        if (shm_fd_ == -1) {
            if (errno == ENOENT) {
                return make_error_code(errc::not_found);
            }
            return get_errno_error();
        }

        // Get the size using fstat
        struct stat sb;
        if (fstat(shm_fd_, &sb) == -1) {
            std::error_code ec = get_errno_error();
            ::close(shm_fd_);
            shm_fd_ = -1;
            return ec;
        }

        size_ = static_cast<std::size_t>(sb.st_size);

        return map_impl();
    }

    void unmap() noexcept {
        unmap_impl();
    }

    void close() noexcept {
        close_impl();
    }

    void* data() noexcept {
        return mapped_addr_;
    }

    const void* data() const noexcept {
        return mapped_addr_;
    }

    std::size_t size() const noexcept {
        return size_;
    }

    const char* name() const noexcept {
        return original_name_.c_str();
    }

    bool is_valid() const noexcept {
        return mapped_addr_ != nullptr && mapped_addr_ != MAP_FAILED;
    }

    access_mode mode() const noexcept {
        return mode_;
    }

    static bool remove(const char* name) noexcept {
        if (!is_valid_name(name)) {
            return false;
        }

        std::string formatted_name = format_name(name);
        return shm_unlink(formatted_name.c_str()) == 0;
    }

    static bool exists(const char* name) noexcept {
        if (!is_valid_name(name)) {
            return false;
        }

        std::string formatted_name = format_name(name);
        int fd = shm_open(formatted_name.c_str(), O_RDONLY, 0);
        if (fd != -1) {
            ::close(fd);
            return true;
        }
        return false;
    }

private:
    int shm_fd_ = -1;
    void* mapped_addr_ = nullptr;
    std::string name_;  // Formatted name with "/" prefix for POSIX API
    std::string original_name_;  // Original name without prefix for public accessor
    std::size_t size_ = 0;
    access_mode mode_ = access_mode::read_write;
    bool owns_shm_ = false;  // Track if we created it (for unlinking)

    std::error_code map_impl() {
        if (shm_fd_ == -1) {
            return make_error_code(errc::mapping_failed);
        }

        int prot = get_mmap_prot(mode_);

        mapped_addr_ = mmap(
            nullptr,           // Let kernel choose address
            size_,
            prot,
            MAP_SHARED,        // Share with other processes
            shm_fd_,
            0                  // Offset
        );

        if (mapped_addr_ == MAP_FAILED) {
            mapped_addr_ = nullptr;
            return get_errno_error();
        }

        return {};
    }

    void unmap_impl() noexcept {
        if (mapped_addr_ != nullptr && mapped_addr_ != MAP_FAILED) {
            munmap(mapped_addr_, size_);
            mapped_addr_ = nullptr;
        }
    }

    void close_impl() noexcept {
        unmap_impl();

        if (shm_fd_ != -1) {
            ::close(shm_fd_);
            shm_fd_ = -1;
        }

        // Only unlink if we created it
        // Note: In typical usage, you might not want to unlink here
        // because other processes might still be using it.
        // We leave unlinking to the user via the remove() static method.

        size_ = 0;
    }

    static std::string format_name(const char* name) {
        // POSIX requires name to start with '/'
        if (name[0] == '/') {
            return std::string(name);
        }
        return std::string("/") + name;
    }

    static int get_open_flags(create_mode mode, access_mode access) {
        int flags = O_CREAT;

        if (access == access_mode::read_only) {
            flags |= O_RDONLY;
        } else {
            flags |= O_RDWR;
        }

        return flags;
    }

    static int get_mmap_prot(access_mode mode) {
        switch (mode) {
            case access_mode::read_only:
                return PROT_READ;
            case access_mode::read_write:
                return PROT_READ | PROT_WRITE;
            default:
                return PROT_READ | PROT_WRITE;
        }
    }

    static std::error_code get_errno_error() {
        return std::error_code(errno, std::system_category());
    }
};

}  // namespace detail
}  // namespace shm
}  // namespace slick

#endif  // SLICK_SHM_POSIX
