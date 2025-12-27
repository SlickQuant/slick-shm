#include <slick/shm/shared_memory.hpp>
#include <cstring>
#include <iostream>

// Helper executable for cross-process tests
// Usage: test_process_writer <shm_name>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: test_process_writer <shm_name>" << std::endl;
        return 1;
    }

    const char* shm_name = argv[1];

    try {
        using namespace slick::shm;

        // Create shared memory
        shared_memory shm(shm_name, 1024, create_only);

        // Write test data
        const char* test_data = "Cross-process test data from writer";
        std::memcpy(shm.data(), test_data, std::strlen(test_data) + 1);

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Writer error: " << e.what() << std::endl;
        return 1;
    }
}
