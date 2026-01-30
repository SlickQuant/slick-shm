# Fetch Catch2 for testing
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# Main test executable
add_executable(slick_shm_tests
    test_main.cpp
    test_create_open.cpp
    test_error_handling.cpp
    test_move_semantics.cpp
    test_cross_process.cpp
    test_is_creator.cpp
)

target_link_libraries(slick_shm_tests PRIVATE
    slick::shm
    Catch2::Catch2WithMain
)

# Register tests with CTest
include(CTest)
include(Catch)
catch_discover_tests(slick_shm_tests)

# Helper executables for cross-process tests
add_executable(test_process_writer test_process_writer.cpp)
target_link_libraries(test_process_writer PRIVATE slick::shm)

add_executable(test_process_reader test_process_reader.cpp)
target_link_libraries(test_process_reader PRIVATE slick::shm)
