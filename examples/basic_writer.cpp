#include <slick/shm/shared_memory.hpp>

#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

int main() {
    using namespace slick::shm;

    const char* shm_name = "slick_shm_example";
    const std::size_t shm_size = 1024;

    try {
        // Create shared memory
        std::cout << "Creating shared memory '" << shm_name << "' with size "
                  << shm_size << " bytes..." << std::endl;

        shared_memory shm(shm_name, shm_size, create_only);

        std::cout << "Shared memory created successfully!" << std::endl;
        std::cout << "  Name: " << shm.name() << std::endl;
        std::cout << "  Size: " << shm.size() << " bytes" << std::endl;
        std::cout << "  Address: " << shm.data() << std::endl;

        // Write some data
        const char* message = "Hello from slick_shm! This is a test message.";
        std::memcpy(shm.data(), message, std::strlen(message) + 1);

        std::cout << "\nWrote message to shared memory: \"" << message << "\"" << std::endl;

        std::cout << "\nShared memory is now accessible to other processes." << std::endl;
        std::cout << "Run 'basic_reader' in another terminal to read the data." << std::endl;
        std::cout << "\nPress Enter to cleanup and exit..." << std::endl;

        std::cin.get();

        std::cout << "Cleaning up..." << std::endl;

    } catch (const shared_memory_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Error code: " << e.code().value() << std::endl;

        // Try to cleanup if it already exists
        if (e.code() == errc::already_exists) {
            std::cerr << "\nShared memory already exists. Removing it..." << std::endl;
            if (shared_memory::remove(shm_name)) {
                std::cerr << "Removed successfully. Please run again." << std::endl;
            }
        }

        return 1;
    }

    return 0;
}
