#include <slick/shm/shared_memory.hpp>

#include <iostream>
#include <cstring>

int main() {
    using namespace slick::shm;

    const char* shm_name = "slick_shm_example";

    try {
        // Open existing shared memory
        std::cout << "Opening existing shared memory '" << shm_name << "'..." << std::endl;

        shared_memory shm(shm_name, open_existing, access_mode::read_only);

        std::cout << "Shared memory opened successfully!" << std::endl;
        std::cout << "  Name: " << shm.name() << std::endl;
        std::cout << "  Size: " << shm.size() << " bytes" << std::endl;
        std::cout << "  Address: " << shm.data() << std::endl;

        // Read the data
        const char* data = static_cast<const char*>(shm.data());
        std::cout << "\nMessage from shared memory: \"" << data << "\"" << std::endl;

        std::cout << "\nPress Enter to exit..." << std::endl;
        std::cin.get();

    } catch (const shared_memory_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Error code: " << e.code().value() << std::endl;

        if (e.code() == errc::not_found) {
            std::cerr << "\nShared memory not found. Make sure 'basic_writer' is running first." << std::endl;
        }

        return 1;
    }

    return 0;
}
