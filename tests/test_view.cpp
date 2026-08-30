// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 Slick Quant

#include <catch2/catch_test_macros.hpp>
#include <slick/shm/shared_memory_view.hpp>
#include "test_helpers.hpp"

#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

using namespace slick::shm;
using namespace slick_shm_test;

// The view is meant to be a cheap pass-by-value handle: copying it must not
// allocate, which shows up in the type traits.
static_assert(std::is_nothrow_copy_constructible<shared_memory_view>::value,
              "copying a view must be allocation-free");
static_assert(std::is_nothrow_copy_assignable<shared_memory_view>::value,
              "copy-assigning a view must be allocation-free");

// Building a view from a temporary would dangle immediately, so it is rejected
// at compile time.
static_assert(!std::is_constructible<shared_memory_view, shared_memory&&>::value,
              "a view must not bind to a temporary shared_memory");

TEST_CASE("View mirrors its source", "[view]") {
    std::string name = unique_name("v_mirror");
    shm_cleanup cleanup{name};

    shared_memory shm(name.c_str(), 1024, create_only);
    shared_memory_view view(shm);

    REQUIRE(view.is_valid());
    REQUIRE(view.data() == shm.data());
    REQUIRE(view.size() == shm.size());
    REQUIRE(view.mode() == shm.mode());
    REQUIRE(std::strcmp(view.name(), shm.name()) == 0);
}

TEST_CASE("View survives a move of its source", "[view]") {
    // Deliberately a short name: it is short names that a std::string keeps
    // inside the object itself, so a borrowed pointer into one would be left
    // behind in the moved-from object. Long names would mask the bug.
    std::string name = unique_name("v_mv");
    shm_cleanup cleanup{name};
    REQUIRE(name.size() < 16);

    shared_memory shm(name.c_str(), 1024, create_only);
    shared_memory_view view(shm);
    const void* addr = shm.data();

    shared_memory moved(std::move(shm));

    REQUIRE(std::string(view.name()) == name);
    REQUIRE(std::strcmp(view.name(), moved.name()) == 0);
    REQUIRE(view.data() == addr);
}

TEST_CASE("View survives move-assignment of its source", "[view]") {
    std::string name = unique_name("v_ma");
    shm_cleanup cleanup{name};

    shared_memory shm(name.c_str(), 1024, create_only);
    shared_memory_view view(shm);

    shared_memory target;
    target = std::move(shm);

    REQUIRE(std::string(view.name()) == name);
    REQUIRE(view.data() == target.data());
}

TEST_CASE("View survives vector reallocation of its source", "[view]") {
    std::string first = unique_name("v_vec1");
    std::string second = unique_name("v_vec2");
    shm_cleanup cleanup_first{first};
    shm_cleanup cleanup_second{second};

    std::vector<shared_memory> segments;
    segments.reserve(1);
    segments.emplace_back(first.c_str(), 1024, create_only);

    shared_memory_view view(segments[0]);

    // Growing past the reserved capacity moves every element to new storage.
    segments.emplace_back(second.c_str(), 1024, create_only);
    REQUIRE(segments.size() == 2);

    REQUIRE(std::string(view.name()) == first);
    REQUIRE(view.data() == segments[0].data());
}

TEST_CASE("View from raw parameters", "[view]") {
    char buffer[64] = {};
    shared_memory_view view(buffer, sizeof(buffer), "raw_name", access_mode::read_only);

    REQUIRE(view.is_valid());
    REQUIRE(view.data() == buffer);
    REQUIRE(view.size() == sizeof(buffer));
    REQUIRE(std::strcmp(view.name(), "raw_name") == 0);
    REQUIRE(view.mode() == access_mode::read_only);
}

TEST_CASE("Default view is invalid", "[view]") {
    shared_memory_view view;

    REQUIRE_FALSE(view.is_valid());
    REQUIRE(view.data() == nullptr);
    REQUIRE(view.size() == 0);
    REQUIRE(view.name() != nullptr);          // never null - empty, not dangling
    REQUIRE(std::strcmp(view.name(), "") == 0);
}

TEST_CASE("Copied view keeps pointing at the same segment", "[view]") {
    std::string name = unique_name("v_copy");
    shm_cleanup cleanup{name};

    shared_memory shm(name.c_str(), 512, create_only);
    shared_memory_view original(shm);
    shared_memory_view copy = original;

    REQUIRE(copy.data() == original.data());
    REQUIRE(copy.size() == original.size());
    REQUIRE(copy.name() == original.name());  // same borrowed pointer, no copy
}
