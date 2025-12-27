cmake_minimum_required(VERSION 3.14)

project(slick_shm
    VERSION 0.1.1
    LANGUAGES CXX
    DESCRIPTION "C++ header-only cross-platform shared memory library"
)

message(STATUS "slick_shm: ${PROJECT_VERSION}")

# Options
option(SLICK_SHM_BUILD_EXAMPLES "Build example programs" ON)
option(SLICK_SHM_BUILD_TESTS "Build unit tests" ON)
option(SLICK_SHM_INSTALL "Generate install target" ON)

# Interface library (header-only)
add_library(slick_shm INTERFACE)
add_library(slick::shm ALIAS slick_shm)

target_include_directories(slick_shm INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(slick_shm INTERFACE cxx_std_17)

# Platform-specific linking
if(APPLE)
    # macOS doesn't need rt library (shm_open/shm_unlink are in libc)
    # pthread may be needed for some implementations
    find_package(Threads)
    if(Threads_FOUND)
        target_link_libraries(slick_shm INTERFACE Threads::Threads)
    endif()
elseif(UNIX)
    # Linux needs rt (realtime) for shm_open/shm_unlink
    target_link_libraries(slick_shm INTERFACE rt)

    # pthread may be needed for some implementations
    find_package(Threads)
    if(Threads_FOUND)
        target_link_libraries(slick_shm INTERFACE Threads::Threads)
    endif()
endif()

# Examples
if(SLICK_SHM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(SLICK_SHM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install
if(SLICK_SHM_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Install headers
    install(DIRECTORY include/slick
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Install targets
    install(TARGETS slick_shm
        EXPORT slick_shm-targets
    )

    # Install export set
    install(EXPORT slick_shm-targets
        FILE slick_shm-targets.cmake
        NAMESPACE slick::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/slick_shm
    )

    # Generate and install package config file
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/slick_shm-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/slick_shm-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/slick_shm
    )

    # Generate and install package version file
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/slick_shm-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    # Install config files
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/slick_shm-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/slick_shm-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/slick_shm
    )
endif()
