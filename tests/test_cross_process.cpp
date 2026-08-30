#include <catch2/catch_test_macros.hpp>
#include <slick/shm/shared_memory.hpp>
#include "test_helpers.hpp"

#include <string>
#include <cstdlib>
#include <thread>

using namespace slick::shm;
using namespace slick_shm_test;

TEST_CASE("Cross-process communication", "[cross-process]") {
    std::string name = unique_name("test_cross_proc");
    shm_cleanup cleanup{name};

    SECTION("Writer then reader") {
        // Create shared memory in this process and keep it alive
        shared_memory shm(name.c_str(), 1024, create_only);
        const char* test_data = "Cross-process test data";
        std::memcpy(shm.data(), test_data, std::strlen(test_data) + 1);

        // Try to find the reader executable
        std::string reader_cmd;

        #ifdef _WIN32
            // On Windows with CMake, executables might be in Debug/Release subdirs
            const char* configs[] = {"", "Debug\\", "Release\\", ".\\", ".\\Debug\\", ".\\Release\\"};
            bool found = false;
            for (const char* config : configs) {
                std::string test_path = std::string(config) + "test_process_reader.exe";
                HANDLE h = CreateFileA(test_path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (h != INVALID_HANDLE_VALUE) {
                    CloseHandle(h);
                    reader_cmd = test_path + " " + name;
                    found = true;
                    break;
                }
            }
            if (!found) {
                reader_cmd = "test_process_reader.exe " + name;
            }
        #else
            reader_cmd = "./test_process_reader " + name;
        #endif

        // Run reader process
        int reader_result = std::system(reader_cmd.c_str());

        REQUIRE(reader_result == 0);
    }
}
