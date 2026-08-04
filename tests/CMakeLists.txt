enable_testing()

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    SYSTEM
)

if (WIN32)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif ()

FetchContent_MakeAvailable(googletest)

add_executable(unit_tests
    utils/test_shm.cpp
    utils/test_tempfile.cpp
    utils/test_lru_cache.cpp
    utils/test_resize_debouncer.cpp
    render/test_threadpool.cpp
    render/test_bounds.cpp
    render/test_parser.cpp
    render/test_PageSpecs.cpp
    terminal/test_kitty.cpp
    viewer/test_pageview.cpp
    # Add new test files here
)

target_compile_definitions(unit_tests PRIVATE PDVU_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

target_link_libraries(unit_tests
    PRIVATE
    pdvu_core
    pdvu_compiler_flags
    GTest::gtest_main
    GTest::gmock_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)