#include <catch2/catch_test_macros.hpp>
#include <slick/shm/shared_memory.hpp>

#include <string>
#include <chrono>

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

TEST_CASE("Exception variants throw on error", "[error][exception]") {
    std::string name = unique_name("test_exception");
    shm_cleanup cleanup{name};

    SECTION("create_only throws if already exists") {
        shared_memory shm1(name.c_str(), 512, create_only);
        REQUIRE(shm1.is_valid());

        try {
            shared_memory shm2(name.c_str(), 512, create_only);
            FAIL("Should have thrown");
        } catch (const shared_memory_error& e) {
            REQUIRE(e.code() == errc::already_exists);
        }
    }

    SECTION("open_existing throws if not found") {
        std::string nonexistent = unique_name("nonexistent");

        try {
            shared_memory shm(nonexistent.c_str(), open_existing);
            FAIL("Should have thrown");
        } catch (const shared_memory_error& e) {
            REQUIRE(e.code() == errc::not_found);
        }
    }

    SECTION("Invalid size throws") {
        try {
            shared_memory shm(name.c_str(), 0, create_only);
            FAIL("Should have thrown");
        } catch (const shared_memory_error& e) {
            REQUIRE(e.code() == errc::invalid_size);
        }
    }

    SECTION("Invalid name throws") {
        try {
            shared_memory shm("", 512, create_only);
            FAIL("Should have thrown");
        } catch (const shared_memory_error& e) {
            REQUIRE(e.code() == errc::invalid_name);
        }
    }
}

TEST_CASE("No-throw variants return error codes", "[error][nothrow]") {
    std::string name = unique_name("test_nothrow");
    shm_cleanup cleanup{name};

    SECTION("create_only fails if already exists") {
        shared_memory shm1(name.c_str(), 512, create_only, access_mode::read_write, std::nothrow);
        REQUIRE(shm1.is_valid());
        REQUIRE(!shm1.last_error());

        shared_memory shm2(name.c_str(), 512, create_only, access_mode::read_write, std::nothrow);
        REQUIRE(!shm2.is_valid());
        REQUIRE(shm2.last_error() == errc::already_exists);
    }

    SECTION("open_existing fails if not found") {
        std::string nonexistent = unique_name("nonexistent");

        shared_memory shm(nonexistent.c_str(), open_existing, access_mode::read_write, std::nothrow);
        REQUIRE(!shm.is_valid());
        REQUIRE(shm.last_error() == errc::not_found);
    }

    SECTION("Invalid size fails") {
        shared_memory shm(name.c_str(), 0, create_only, access_mode::read_write, std::nothrow);
        REQUIRE(!shm.is_valid());
        REQUIRE(shm.last_error() == errc::invalid_size);
    }

    SECTION("Invalid name fails") {
        shared_memory shm("", 512, create_only, access_mode::read_write, std::nothrow);
        REQUIRE(!shm.is_valid());
        REQUIRE(shm.last_error() == errc::invalid_name);
    }

    SECTION("open_or_create succeeds on create") {
        shared_memory shm(name.c_str(), 256, open_or_create, access_mode::read_write, std::nothrow);
        REQUIRE(shm.is_valid());
        REQUIRE(!shm.last_error());
    }
}

TEST_CASE("Error code conversions", "[error][errc]") {
    SECTION("make_error_code works") {
        std::error_code ec = make_error_code(errc::already_exists);
        REQUIRE(ec == errc::already_exists);
        REQUIRE(ec.category() == shm_category());
    }

    SECTION("Error messages are correct") {
        REQUIRE(make_error_code(errc::success).message() == "success");
        REQUIRE(make_error_code(errc::already_exists).message() == "shared memory already exists");
        REQUIRE(make_error_code(errc::not_found).message() == "shared memory not found");
        REQUIRE(make_error_code(errc::invalid_size).message() == "invalid size (must be greater than zero)");
        REQUIRE(make_error_code(errc::invalid_name).message() == "invalid shared memory name");
    }
}

TEST_CASE("Name validation", "[error][validation]") {
    SECTION("Empty name is invalid") {
        REQUIRE_THROWS_AS(
            shared_memory("", 512, create_only),
            shared_memory_error
        );
    }

    SECTION("Null name is invalid") {
        REQUIRE_THROWS_AS(
            shared_memory(nullptr, 512, create_only),
            shared_memory_error
        );
    }

#ifdef SLICK_SHM_WINDOWS
    SECTION("Windows: invalid characters") {
        const char* invalid_names[] = {
            "test\\name",
            "test/name",
            "test:name",
            "test*name",
            "test?name",
            "test\"name",
            "test<name",
            "test>name",
            "test|name"
        };

        for (const char* invalid : invalid_names) {
            REQUIRE_THROWS_AS(
                shared_memory(invalid, 512, create_only),
                shared_memory_error
            );
        }
    }

    SECTION("Windows: object namespace prefixes are valid names") {
        // A backslash is legal as the separator of a leading object namespace
        // prefix. Validated directly so the test does not need the
        // SeCreateGlobalPrivilege that a Global namespace object requires.
        const char* valid_names[] = {
            "Global\\my_shm",
            "Local\\my_shm",
            "global\\my_shm",        // prefixes are case-insensitive
            "LOCAL\\my_shm",
            "Session\\1\\my_shm",
            "Session\\42\\my_shm"
        };

        for (const char* valid : valid_names) {
            REQUIRE(detail::is_valid_name(valid));
        }
    }

    SECTION("Windows: malformed namespace prefixes are invalid") {
        const char* bad_names[] = {
            "Global\\",              // prefix with no object name
            "Local\\",
            "Session\\1\\",
            "Global\\sub\\my_shm",   // backslash past the prefix
            "Local\\my\\shm",
            "Session\\my_shm",       // missing session id
            "Session\\x1\\my_shm",   // non-numeric session id
            "Globals\\my_shm",       // not an object namespace prefix
            "\\my_shm"
        };

        for (const char* bad : bad_names) {
            REQUIRE_FALSE(detail::is_valid_name(bad));
        }
    }

    SECTION("Windows: Local prefixed segment can be created and opened") {
        std::string name = "Local\\" + unique_name("test_local");

        shared_memory shm(name.c_str(), 512, create_only);
        REQUIRE(shm.is_valid());

        shared_memory opener(name.c_str(), open_existing);
        REQUIRE(opener.is_valid());
    }
#endif

#ifdef SLICK_SHM_POSIX
    SECTION("POSIX: slash-only name is invalid") {
        REQUIRE_THROWS_AS(
            shared_memory("/", 512, create_only),
            shared_memory_error
        );
    }

    SECTION("POSIX: slash in middle of name is invalid") {
        // POSIX names can start with '/' but not contain it elsewhere
        REQUIRE_THROWS_AS(
            shared_memory("test/name", 512, create_only),
            shared_memory_error
        );
    }

    SECTION("POSIX: leading slash is allowed") {
        std::string name = unique_name("/test_posix");
        shm_cleanup cleanup{name};

        // This should work (name starts with /)
        shared_memory shm(name.c_str(), 512, create_only);
        REQUIRE(shm.is_valid());
    }
#endif
}

#ifdef SLICK_SHM_POSIX
TEST_CASE("Failed create leaves no segment behind", "[error][cleanup]") {
    // The segment is created before it is sized and mapped, so every failure
    // after shm_open() has to unlink it again - close() alone never does.
    // A size this large is accepted by ftruncate() on tmpfs but can never be
    // mapped, which drives the failure into the mmap() path.
    std::string name = unique_name("test_leak");
    shm_cleanup cleanup{name};

    constexpr std::size_t unmappable_size = std::size_t{1} << 62;
    shared_memory shm(name.c_str(), unmappable_size, create_only,
                      access_mode::read_write, std::nothrow);

    REQUIRE_FALSE(shm.is_valid());
    REQUIRE(shm.last_error());
    REQUIRE_FALSE(shared_memory::exists(name.c_str()));
}
#endif
