#include <slick/shm/shared_memory_view.hpp>

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>

// Shared data structure
struct shared_data {
    std::atomic<int> counter;
    std::atomic<bool> done;
    char message[256];
};

void writer_thread(slick::shm::shared_memory_view view) {
    auto* data = static_cast<shared_data*>(view.data());

    std::cout << "[Writer] Starting..." << std::endl;

    for (int i = 0; i < 10; ++i) {
        data->counter.store(i, std::memory_order_release);

        std::string msg = "Message " + std::to_string(i);
        std::strncpy(data->message, msg.c_str(), sizeof(data->message) - 1);
        data->message[sizeof(data->message) - 1] = '\0';

        std::cout << "[Writer] Wrote: " << msg << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    data->done.store(true, std::memory_order_release);
    std::cout << "[Writer] Done." << std::endl;
}

void reader_thread(slick::shm::shared_memory_view view) {
    auto* data = static_cast<shared_data*>(view.data());

    std::cout << "[Reader] Starting..." << std::endl;

    int last_count = -1;
    while (!data->done.load(std::memory_order_acquire)) {
        int current = data->counter.load(std::memory_order_acquire);

        if (current != last_count) {
            std::cout << "[Reader] Read: counter=" << current
                      << ", message=\"" << data->message << "\"" << std::endl;
            last_count = current;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[Reader] Done." << std::endl;
}

int main() {
    using namespace slick::shm;

    const char* shm_name = "slick_shm_advanced";
    const std::size_t shm_size = sizeof(shared_data);

    try {
        // Clean up any existing shared memory
        shared_memory::remove(shm_name);

        // Create shared memory
        std::cout << "Creating shared memory for advanced synchronization example..." << std::endl;
        shared_memory shm(shm_name, shm_size, create_only);

        // Initialize the shared data
        auto* data = static_cast<shared_data*>(shm.data());
        new (data) shared_data();  // Placement new to initialize atomics
        data->counter.store(0, std::memory_order_relaxed);
        data->done.store(false, std::memory_order_relaxed);
        std::memset(data->message, 0, sizeof(data->message));

        std::cout << "Shared memory created and initialized." << std::endl;
        std::cout << "Starting writer and reader threads..." << std::endl;

        // Create a view for thread safety
        shared_memory_view view(shm);

        // Start threads
        std::thread writer(writer_thread, view);
        std::thread reader(reader_thread, view);

        // Wait for threads to complete
        writer.join();
        reader.join();

        std::cout << "\nExample completed successfully!" << std::endl;

        // Cleanup
        shared_memory::remove(shm_name);

    } catch (const shared_memory_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Error code: " << e.code().value() << std::endl;
        return 1;
    }

    return 0;
}
