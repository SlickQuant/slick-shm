#include <catch2/catch_test_macros.hpp>
#include <slick/shm/shared_memory.hpp>

#include <string>
#include <chrono>
#include <cstring>

using namespace slick::shm;

namespace {

std::string unique_name(const char* prefix) {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    // Use modulo to keep the timestamp shorter while still being unique (macOS has 31-char limit)
    return std::string(prefix) + std::to_string(millis % 100000000);
}

struct shm_cleanup {
    std::string name;
    ~shm_cleanup() {
        shared_memory::remove(name.c_str());
    }
};

}  // namespace

TEST_CASE("Move constructor transfers ownership", "[move]") {
    std::string name = unique_name("test_move_ctor");
    shm_cleanup cleanup{name};

    shared_memory shm1(name.c_str(), 512, create_only);
    REQUIRE(shm1.is_valid());

    void* original_addr = shm1.data();
    std::size_t original_size = shm1.size();
    std::string original_name = shm1.name();

    // Write test data
    const char* test_data = "Move test";
    std::memcpy(shm1.data(), test_data, std::strlen(test_data) + 1);

    // Move construct
    shared_memory shm2(std::move(shm1));

    SECTION("Moved-to object is valid") {
        REQUIRE(shm2.is_valid());
        REQUIRE(shm2.data() == original_addr);
        REQUIRE(shm2.size() == original_size);
        REQUIRE(std::string(shm2.name()) == original_name);

        // Verify data is intact
        const char* read_data = static_cast<const char*>(shm2.data());
        REQUIRE(std::strcmp(read_data, test_data) == 0);
    }

    SECTION("Moved-from object is invalid") {
        REQUIRE(!shm1.is_valid());
        REQUIRE(shm1.data() == nullptr);
        REQUIRE(shm1.size() == 0);
    }
}

TEST_CASE("Move assignment transfers ownership", "[move]") {
    std::string name1 = unique_name("test_move_assign1");
    std::string name2 = unique_name("test_move_assign2");
    shm_cleanup cleanup1{name1};
    shm_cleanup cleanup2{name2};

    shared_memory shm1(name1.c_str(), 256, create_only);
    shared_memory shm2(name2.c_str(), 512, create_only);

    REQUIRE(shm1.is_valid());
    REQUIRE(shm2.is_valid());

    void* addr1 = shm1.data();
    std::size_t size1 = shm1.size();
    std::string name_str1 = shm1.name();

    // Write test data to shm1
    const char* test_data = "Move assignment test";
    std::memcpy(shm1.data(), test_data, std::strlen(test_data) + 1);

    // Move assign shm1 to shm2
    shm2 = std::move(shm1);

    SECTION("Moved-to object has source's data") {
        REQUIRE(shm2.is_valid());
        REQUIRE(shm2.data() == addr1);
        REQUIRE(shm2.size() == size1);
        REQUIRE(std::string(shm2.name()) == name_str1);

        // Verify data is intact
        const char* read_data = static_cast<const char*>(shm2.data());
        REQUIRE(std::strcmp(read_data, test_data) == 0);
    }

    SECTION("Moved-from object is invalid") {
        REQUIRE(!shm1.is_valid());
        REQUIRE(shm1.data() == nullptr);
        REQUIRE(shm1.size() == 0);
    }
}

TEST_CASE("Self-move assignment is safe", "[move]") {
    std::string name = unique_name("test_self_move");
    shm_cleanup cleanup{name};

    shared_memory shm(name.c_str(), 128, create_only);
    REQUIRE(shm.is_valid());

    void* addr = shm.data();
    std::size_t size = shm.size();

    // Self-move (this is technically UB in C++ but should be safe)
    shm = std::move(shm);

    // Object should still be valid
    REQUIRE(shm.is_valid());
    REQUIRE(shm.data() == addr);
    REQUIRE(shm.size() == size);
}

TEST_CASE("Moved-from object can be destroyed safely", "[move]") {
    std::string name = unique_name("test_move_destroy");
    shm_cleanup cleanup{name};

    {
        shared_memory shm1(name.c_str(), 256, create_only);
        REQUIRE(shm1.is_valid());

        {
            shared_memory shm2(std::move(shm1));
            REQUIRE(shm2.is_valid());
            REQUIRE(!shm1.is_valid());
        }
        // shm2 destroyed, shm1 still in scope but invalid

        // Should be safe to destroy shm1
    }

    // If we got here without crashing, the test passes
    REQUIRE(true);
}

TEST_CASE("Move enables return from function", "[move]") {
    std::string name = unique_name("test_move_return");
    shm_cleanup cleanup{name};

    auto create_shm = [&name]() -> shared_memory {
        shared_memory shm(name.c_str(), 512, create_only);
        const char* data = "Returned from function";
        std::memcpy(shm.data(), data, std::strlen(data) + 1);
        return shm;  // Move happens here
    };

    shared_memory shm = create_shm();

    REQUIRE(shm.is_valid());
    // Windows may round up to page size
    REQUIRE(shm.size() >= 512);

    const char* read_data = static_cast<const char*>(shm.data());
    REQUIRE(std::strcmp(read_data, "Returned from function") == 0);
}
