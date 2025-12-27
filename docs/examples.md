# Examples

This document provides practical examples of using the slick_shm library.

## Table of Contents

- [Basic Usage](#basic-usage)
- [Cross-Process Communication](#cross-process-communication)
- [Error Handling](#error-handling)
- [Synchronization](#synchronization)
- [Advanced Patterns](#advanced-patterns)

## Basic Usage

### Creating and Writing to Shared Memory

```cpp
#include <slick/shm/shared_memory.hpp>
#include <cstring>
#include <iostream>

int main() {
    using namespace slick::shm;

    // Create shared memory
    shared_memory shm("my_data", 1024, create_only);

    // Write data
    const char* message = "Hello, shared memory!";
    std::memcpy(shm.data(), message, std::strlen(message) + 1);

    std::cout << "Wrote: " << message << std::endl;

    return 0;
}
```

### Opening and Reading from Shared Memory

```cpp
#include <slick/shm/shared_memory.hpp>
#include <iostream>

int main() {
    using namespace slick::shm;

    // Open existing shared memory
    shared_memory shm("my_data", open_existing, access_mode::read_only);

    // Read data
    const char* message = static_cast<const char*>(shm.data());
    std::cout << "Read: " << message << std::endl;

    return 0;
}
```

## Cross-Process Communication

### Writer Process

```cpp
#include <slick/shm/shared_memory.hpp>
#include <cstring>
#include <iostream>

struct Message {
    int id;
    char text[256];
};

int main() {
    using namespace slick::shm;

    // Create shared memory
    shared_memory shm("ipc_channel", sizeof(Message), create_only);

    // Get typed pointer
    Message* msg = static_cast<Message*>(shm.data());

    // Write structured data
    msg->id = 42;
    std::strncpy(msg->text, "Message from writer", sizeof(msg->text));

    std::cout << "Message written. Run reader process..." << std::endl;
    std::cin.get();  // Wait for reader

    return 0;
}
```

### Reader Process

```cpp
#include <slick/shm/shared_memory.hpp>
#include <iostream>

struct Message {
    int id;
    char text[256];
};

int main() {
    using namespace slick::shm;

    // Open existing shared memory
    shared_memory shm("ipc_channel", open_existing, access_mode::read_only);

    // Get typed pointer
    const Message* msg = static_cast<const Message*>(shm.data());

    // Read structured data
    std::cout << "ID: " << msg->id << std::endl;
    std::cout << "Text: " << msg->text << std::endl;

    return 0;
}
```

## Error Handling

### Using Exceptions

```cpp
#include <slick/shm/shared_memory.hpp>
#include <iostream>

int main() {
    using namespace slick::shm;

    try {
        shared_memory shm("test", 1024, create_only);
        // Use shared memory...

    } catch (const shared_memory_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;

        // Handle specific errors
        if (e.code() == errc::already_exists) {
            std::cerr << "Shared memory already exists" << std::endl;
            // Try to remove and retry
            if (shared_memory::remove("test")) {
                std::cout << "Removed existing segment" << std::endl;
            }
        } else if (e.code() == errc::not_found) {
            std::cerr << "Shared memory not found" << std::endl;
        }

        return 1;
    }

    return 0;
}
```

### Using No-Throw Variants

```cpp
#include <slick/shm/shared_memory.hpp>
#include <iostream>

int main() {
    using namespace slick::shm;

    // No-throw constructor
    shared_memory shm("test", 1024, create_only,
                      access_mode::read_write, std::nothrow);

    // Check if successful
    if (!shm.is_valid()) {
        std::cerr << "Failed to create shared memory: "
                  << shm.last_error().message() << std::endl;
        return 1;
    }

    // Use shared memory...

    return 0;
}
```

## Synchronization

### Using std::atomic for Simple Synchronization

```cpp
#include <slick/shm/shared_memory.hpp>
#include <atomic>
#include <thread>
#include <iostream>

struct SharedData {
    std::atomic<bool> ready;
    std::atomic<int> value;
    char message[256];
};

// Writer
void writer_main() {
    using namespace slick::shm;

    shared_memory shm("sync_example", sizeof(SharedData), create_only);
    auto* data = static_cast<SharedData*>(shm.data());

    // Initialize atomics
    new (&data->ready) std::atomic<bool>(false);
    new (&data->value) std::atomic<int>(0);

    // Write data
    std::strcpy(data->message, "Hello from writer");
    data->value.store(42, std::memory_order_release);
    data->ready.store(true, std::memory_order_release);

    std::cout << "Writer: Data ready" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

// Reader
void reader_main() {
    using namespace slick::shm;

    std::this_thread::sleep_for(std::chrono::seconds(1));

    shared_memory shm("sync_example", open_existing);
    auto* data = static_cast<SharedData*>(shm.data());

    // Wait for data
    while (!data->ready.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Read data
    int value = data->value.load(std::memory_order_acquire);
    std::cout << "Reader: value=" << value << ", message=" << data->message << std::endl;
}
```

### Producer-Consumer Pattern

```cpp
#include <slick/shm/shared_memory.hpp>
#include <atomic>
#include <thread>

struct RingBuffer {
    static constexpr size_t SIZE = 1024;

    std::atomic<size_t> write_pos;
    std::atomic<size_t> read_pos;
    char data[SIZE];
};

class Producer {
public:
    explicit Producer(slick::shm::shared_memory_view view)
        : buffer_(static_cast<RingBuffer*>(view.data())) {}

    void write(const char* msg, size_t len) {
        size_t pos = buffer_->write_pos.load(std::memory_order_relaxed);
        std::memcpy(&buffer_->data[pos], msg, len);
        buffer_->write_pos.store(pos + len, std::memory_order_release);
    }

private:
    RingBuffer* buffer_;
};

class Consumer {
public:
    explicit Consumer(slick::shm::shared_memory_view view)
        : buffer_(static_cast<RingBuffer*>(view.data())) {}

    bool read(char* out, size_t max_len) {
        size_t read_pos = buffer_->read_pos.load(std::memory_order_acquire);
        size_t write_pos = buffer_->write_pos.load(std::memory_order_acquire);

        if (read_pos >= write_pos) {
            return false;  // No data available
        }

        size_t available = write_pos - read_pos;
        size_t to_read = std::min(available, max_len);

        std::memcpy(out, &buffer_->data[read_pos], to_read);
        buffer_->read_pos.store(read_pos + to_read, std::memory_order_release);

        return true;
    }

private:
    RingBuffer* buffer_;
};
```

## Advanced Patterns

### RAII Cleanup Helper

```cpp
#include <slick/shm/shared_memory.hpp>
#include <string>

class scoped_shared_memory {
public:
    scoped_shared_memory(const std::string& name, size_t size)
        : name_(name), shm_(name.c_str(), size, slick::shm::create_only) {}

    ~scoped_shared_memory() {
        shm_.close();
        slick::shm::shared_memory::remove(name_.c_str());
    }

    slick::shm::shared_memory& get() { return shm_; }

private:
    std::string name_;
    slick::shm::shared_memory shm_;
};

// Usage
int main() {
    scoped_shared_memory shm("auto_cleanup", 1024);
    // Use shm.get()...
    // Automatic cleanup on scope exit
}
```

### Type-Safe Wrapper

```cpp
#include <slick/shm/shared_memory.hpp>
#include <type_traits>

template<typename T>
class typed_shared_memory {
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable");

public:
    typed_shared_memory(const char* name, slick::shm::create_only_t)
        : shm_(name, sizeof(T), slick::shm::create_only) {
        // Placement new
        new (shm_.data()) T();
    }

    typed_shared_memory(const char* name, slick::shm::open_existing_t)
        : shm_(name, slick::shm::open_existing) {}

    T* operator->() { return static_cast<T*>(shm_.data()); }
    const T* operator->() const { return static_cast<const T*>(shm_.data()); }

    T& operator*() { return *static_cast<T*>(shm_.data()); }
    const T& operator*() const { return *static_cast<const T*>(shm_.data()); }

private:
    slick::shm::shared_memory shm_;
};

// Usage
struct Config {
    int version;
    double threshold;
    char name[64];
};

int main() {
    using namespace slick::shm;

    typed_shared_memory<Config> config("config", create_only);
    config->version = 1;
    config->threshold = 0.5;
}
```

### Shared Memory Pool

```cpp
#include <slick/shm/shared_memory.hpp>
#include <vector>
#include <memory>

class shared_memory_pool {
public:
    shared_memory_pool(const char* name_prefix, size_t block_size, size_t num_blocks)
        : block_size_(block_size) {
        for (size_t i = 0; i < num_blocks; ++i) {
            std::string name = std::string(name_prefix) + "_" + std::to_string(i);
            blocks_.emplace_back(name.c_str(), block_size, slick::shm::create_only);
        }
    }

    void* allocate() {
        if (next_block_ >= blocks_.size()) {
            return nullptr;  // Pool exhausted
        }
        return blocks_[next_block_++].data();
    }

private:
    size_t block_size_;
    size_t next_block_ = 0;
    std::vector<slick::shm::shared_memory> blocks_;
};
```

## Best Practices

1. **Always handle errors**: Use try-catch or check `is_valid()`
2. **Clean up**: Call `remove()` for portability (POSIX requires it)
3. **Synchronize access**: Use `std::atomic` or external synchronization
4. **Use views**: Pass `shared_memory_view` instead of `shared_memory&` to avoid ownership confusion
5. **Validate names**: Use cross-platform safe names (alphanumeric + underscore, ≤31 chars)
6. **Check sizes**: Be aware of platform-specific size limits
7. **Test on all platforms**: Behavior differs between Windows and POSIX
