// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant

#include <slick/shm/shared_memory.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace slick::shm;

TEST_CASE("is_creator() returns true for create_only", "[is_creator]") {
    const char* name = "test_is_creator_create_only";

    // Clean up any existing
    shared_memory::remove(name);

    shared_memory shm(name, 1024, create_only);

    REQUIRE(shm.is_valid());
    REQUIRE(shm.is_creator());

    shared_memory::remove(name);
}

TEST_CASE("is_creator() returns false for open_existing", "[is_creator]") {
    const char* name = "test_is_creator_open_existing";

    // Clean up any existing
    shared_memory::remove(name);

    // Create first
    {
        shared_memory creator(name, 1024, create_only);
        REQUIRE(creator.is_valid());
        REQUIRE(creator.is_creator());

        // Open existing
        shared_memory opener(name, open_existing);
        REQUIRE(opener.is_valid());
        REQUIRE_FALSE(opener.is_creator());
    }

    shared_memory::remove(name);
}

TEST_CASE("is_creator() for open_or_create - creates new", "[is_creator]") {
    const char* name = "test_is_creator_open_or_create_new";

    // Clean up any existing
    shared_memory::remove(name);

    // Should create new
    shared_memory shm(name, 1024, open_or_create);

    REQUIRE(shm.is_valid());
    REQUIRE(shm.is_creator());

    shared_memory::remove(name);
}

TEST_CASE("is_creator() for open_or_create - opens existing", "[is_creator]") {
    const char* name = "test_is_creator_open_or_create_existing";

    // Clean up any existing
    shared_memory::remove(name);

    // Create first
    {
        shared_memory creator(name, 1024, create_only);
        REQUIRE(creator.is_creator());

        // Open existing with open_or_create
        shared_memory opener(name, 1024, open_or_create);
        REQUIRE(opener.is_valid());
        REQUIRE_FALSE(opener.is_creator());
    }

    shared_memory::remove(name);
}

TEST_CASE("is_creator() for open_always - creates new", "[is_creator]") {
    const char* name = "test_is_creator_open_always_new";

    // Clean up any existing
    shared_memory::remove(name);

    // Should create new
    shared_memory shm(name, 1024, open_always);

    REQUIRE(shm.is_valid());
    REQUIRE(shm.is_creator());

    shared_memory::remove(name);
}

TEST_CASE("is_creator() for open_always - opens existing", "[is_creator]") {
    const char* name = "test_is_creator_open_always_existing";

    // Clean up any existing
    shared_memory::remove(name);

    // Create first
    {
        shared_memory creator(name, 1024, create_only);
        REQUIRE(creator.is_creator());

        // Open existing with open_always
        shared_memory opener(name, 1024, open_always);
        REQUIRE(opener.is_valid());
        REQUIRE_FALSE(opener.is_creator());
    }

    shared_memory::remove(name);
}

TEST_CASE("is_creator() preserved after move constructor", "[is_creator]") {
    const char* name = "test_is_creator_move_ctor";

    // Clean up any existing
    shared_memory::remove(name);

    {
        shared_memory creator(name, 1024, create_only);
        REQUIRE(creator.is_creator());

        // Move to new object
        shared_memory moved(std::move(creator));
        REQUIRE(moved.is_creator());
        REQUIRE_FALSE(creator.is_creator());  // Moved-from should be false
    }

    shared_memory::remove(name);
}

TEST_CASE("is_creator() preserved after move assignment", "[is_creator]") {
    const char* name1 = "test_is_creator_move_assign_1";
    const char* name2 = "test_is_creator_move_assign_2";

    // Clean up any existing
    shared_memory::remove(name1);
    shared_memory::remove(name2);

    {
        shared_memory creator(name1, 1024, create_only);
        REQUIRE(creator.is_creator());

        // Create another for opening
        shared_memory opener(name2, 1024, create_only);
        REQUIRE(opener.is_creator());

        // Open existing in name1
        shared_memory not_creator(name1, open_existing);
        REQUIRE_FALSE(not_creator.is_creator());

        // Move creator to not_creator
        not_creator = std::move(creator);
        REQUIRE(not_creator.is_creator());
        REQUIRE_FALSE(creator.is_creator());  // Moved-from should be false
    }

    shared_memory::remove(name1);
    shared_memory::remove(name2);
}

TEST_CASE("is_creator() with no-throw constructors", "[is_creator]") {
    const char* name = "test_is_creator_nothrow";

    // Clean up any existing
    shared_memory::remove(name);

    // Create with no-throw
    shared_memory creator(name, 1024, create_only, access_mode::read_write, std::nothrow);
    REQUIRE(creator.is_valid());
    REQUIRE(creator.is_creator());

    // Open with no-throw
    shared_memory opener(name, open_existing, access_mode::read_write, std::nothrow);
    REQUIRE(opener.is_valid());
    REQUIRE_FALSE(opener.is_creator());

    shared_memory::remove(name);
}

TEST_CASE("is_creator() returns false for invalid object", "[is_creator]") {
    shared_memory invalid;

    REQUIRE_FALSE(invalid.is_valid());
    REQUIRE_FALSE(invalid.is_creator());
}
