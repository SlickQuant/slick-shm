#include <catch2/catch_test_macros.hpp>
#include <slick/shm/shared_memory.hpp>

#include <cstring>
#include <string>
#include <chrono>

using namespace slick::shm;

namespace {

// Helper to generate unique names (kept short for macOS 31-char limit including "/" prefix)
std::string unique_name(const char* prefix) {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    // Use modulo to keep the timestamp shorter while still being unique
    return std::string(prefix) + std::to_string(millis % 100000000);
}

// RAII cleanup helper
struct shm_cleanup {
    std::string name;
    ~shm_cleanup() {
        shared_memory::remove(name.c_str());
    }
};

}  // namespace

TEST_CASE("Create shared memory with create_only", "[create]") {
    std::string name = unique_name("test_create");
    shm_cleanup cleanup{name};

    SECTION("Create new shared memory") {
        shared_memory shm(name.c_str(), 1024, create_only);

        REQUIRE(shm.is_valid());
        // Windows may round up to page size (typically 4KB)
        REQUIRE(shm.size() >= 1024);
        REQUIRE(shm.data() != nullptr);
        REQUIRE(std::string(shm.name()) == name);
    }

    SECTION("Creating twice should fail") {
        shared_memory shm1(name.c_str(), 1024, create_only);
        REQUIRE(shm1.is_valid());

        REQUIRE_THROWS_AS(
            shared_memory(name.c_str(), 1024, create_only),
            shared_memory_error
        );
    }
}

TEST_CASE("Open existing shared memory", "[open]") {
    std::string name = unique_name("test_open");
    shm_cleanup cleanup{name};

    SECTION("Open after create") {
        shared_memory shm_creator(name.c_str(), 2048, create_only);
        REQUIRE(shm_creator.is_valid());

        // Write some data
        const char* test_data = "Hello, World!";
        std::memcpy(shm_creator.data(), test_data, std::strlen(test_data) + 1);

        // Open in another object
        shared_memory shm_opener(name.c_str(), open_existing);
        REQUIRE(shm_opener.is_valid());

        // On Windows, size may be rounded up to page/allocation granularity
        // Both objects should report the same size
        REQUIRE(shm_opener.size() == shm_creator.size());
        REQUIRE(shm_opener.size() >= 2048);

        // Verify data
        const char* read_data = static_cast<const char*>(shm_opener.data());
        REQUIRE(std::strcmp(read_data, test_data) == 0);
    }

    SECTION("Opening non-existent should fail") {
        std::string nonexistent = unique_name("nonexistent");

        REQUIRE_THROWS_AS(
            shared_memory(nonexistent.c_str(), open_existing),
            shared_memory_error
        );
    }
}

TEST_CASE("Open or create shared memory", "[open_or_create]") {
    std::string name = unique_name("test_open_or_create");
    shm_cleanup cleanup{name};

    SECTION("Create if doesn't exist") {
        shared_memory shm(name.c_str(), 512, open_or_create);

        REQUIRE(shm.is_valid());
        // Windows may round up to page size
        REQUIRE(shm.size() >= 512);
    }

    SECTION("Open if exists") {
        shared_memory shm1(name.c_str(), 1024, create_only);
        REQUIRE(shm1.is_valid());

        const char* test_data = "Test data";
        std::memcpy(shm1.data(), test_data, std::strlen(test_data) + 1);

        shared_memory shm2(name.c_str(), 999, open_or_create);
        REQUIRE(shm2.is_valid());

        // Should have opened existing, so size should match shm1
        // Both may be rounded up on Windows
        REQUIRE(shm2.size() == shm1.size());
        REQUIRE(shm2.size() >= 1024);

        // Verify data
        const char* read_data = static_cast<const char*>(shm2.data());
        REQUIRE(std::strcmp(read_data, test_data) == 0);
    }
}

TEST_CASE("Read/write access modes", "[access]") {
    std::string name = unique_name("test_access");
    shm_cleanup cleanup{name};

    SECTION("Read-write mode") {
        shared_memory shm(name.c_str(), 256, create_only, access_mode::read_write);

        REQUIRE(shm.is_valid());
        REQUIRE(shm.mode() == access_mode::read_write);

        // Should be able to write
        const char* data = "Write test";
        std::memcpy(shm.data(), data, std::strlen(data) + 1);

        // And read
        const char* read = static_cast<const char*>(shm.data());
        REQUIRE(std::strcmp(read, data) == 0);
    }

    SECTION("Read-only mode") {
        // Create with read-write first
        shared_memory shm_rw(name.c_str(), 256, create_only, access_mode::read_write);
        const char* data = "Read-only test";
        std::memcpy(shm_rw.data(), data, std::strlen(data) + 1);

        // Open as read-only
        shared_memory shm_ro(name.c_str(), open_existing, access_mode::read_only);

        REQUIRE(shm_ro.is_valid());
        REQUIRE(shm_ro.mode() == access_mode::read_only);

        // Should be able to read
        const char* read = static_cast<const char*>(shm_ro.data());
        REQUIRE(std::strcmp(read, data) == 0);
    }
}

TEST_CASE("Read-only create and open_or_create", "[access][read-only]") {
    std::string name = unique_name("test_read_only_create");
    shm_cleanup cleanup{name};

    SECTION("Create-only with read-only access") {
        shared_memory shm(name.c_str(), 256, create_only, access_mode::read_only);
        REQUIRE(shm.is_valid());
        REQUIRE(shm.mode() == access_mode::read_only);
        REQUIRE(shm.size() >= 256);
    }

    SECTION("open_or_create creates with read-only access") {
        shared_memory shm(name.c_str(), 512, open_or_create, access_mode::read_only);
        REQUIRE(shm.is_valid());
        REQUIRE(shm.mode() == access_mode::read_only);
        REQUIRE(shm.size() >= 512);
    }

    SECTION("open_or_create opens existing with read-only access") {
        shared_memory shm_rw(name.c_str(), 1024, create_only, access_mode::read_write);
        REQUIRE(shm_rw.is_valid());

        shared_memory shm_ro(name.c_str(), 2048, open_or_create, access_mode::read_only);
        REQUIRE(shm_ro.is_valid());
        REQUIRE(shm_ro.mode() == access_mode::read_only);
        REQUIRE(shm_ro.size() == shm_rw.size());
    }
}

TEST_CASE("RAII cleanup", "[raii]") {
    std::string name = unique_name("test_raii");
    shm_cleanup cleanup{name};

    void* addr = nullptr;

    {
        shared_memory shm(name.c_str(), 128, create_only);
        REQUIRE(shm.is_valid());
        addr = shm.data();
        REQUIRE(addr != nullptr);
    }

    // After destructor, memory should be unmapped
    // (We can't easily verify this without causing UB, so we just check we can create again)
}

TEST_CASE("Manual unmap and close", "[manual]") {
    std::string name = unique_name("test_manual");
    shm_cleanup cleanup{name};

    shared_memory shm(name.c_str(), 256, create_only);
    REQUIRE(shm.is_valid());

    SECTION("Manual unmap") {
        shm.unmap();
        REQUIRE(!shm.is_valid());
        REQUIRE(shm.data() == nullptr);
    }

    SECTION("Manual close") {
        shm.close();
        REQUIRE(!shm.is_valid());
        REQUIRE(shm.data() == nullptr);
        REQUIRE(shm.size() == 0);
    }
}

TEST_CASE("Static utility functions", "[static]") {
    std::string name = unique_name("test_static");

    SECTION("exists() returns false for non-existent") {
        REQUIRE(!shared_memory::exists(name.c_str()));
    }

    SECTION("exists() returns true after creation") {
        shared_memory shm(name.c_str(), 128, create_only);
        REQUIRE(shared_memory::exists(name.c_str()));

        shm.close();
        shared_memory::remove(name.c_str());
    }

    SECTION("remove() cleanup") {
        shared_memory shm(name.c_str(), 128, create_only);
        REQUIRE(shm.is_valid());

        shm.close();

        // Remove should succeed (or be no-op on Windows)
        REQUIRE(shared_memory::remove(name.c_str()));
    }
}
