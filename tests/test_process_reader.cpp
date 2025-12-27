#include <slick/shm/shared_memory.hpp>
#include <cstring>
#include <iostream>

// Helper executable for cross-process tests
// Usage: test_process_reader <shm_name>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: test_process_reader <shm_name>" << std::endl;
        return 1;
    }

    const char* shm_name = argv[1];

    try {
        using namespace slick::shm;

        // Open existing shared memory
        shared_memory shm(shm_name, open_existing, access_mode::read_only);

        // Read and verify test data
        const char* expected = "Cross-process test data";
        const char* actual = static_cast<const char*>(shm.data());

        if (std::strcmp(actual, expected) != 0) {
            std::cerr << "Data mismatch!" << std::endl;
            std::cerr << "Expected: " << expected << std::endl;
            std::cerr << "Actual: " << actual << std::endl;
            return 1;
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Reader error: " << e.what() << std::endl;
        return 1;
    }
}
