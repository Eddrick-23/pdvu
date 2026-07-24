enable_testing()

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

if (WIN32)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif ()

FetchContent_MakeAvailable(googletest)

add_executable(unit_tests
    utils/test_shm.cpp
    utils/test_tempfile.cpp
    utils/test_lru_cache.cpp
    render/test_threadpool.cpp
    render/test_parser_PageSpecs.cpp
    terminal/test_kitty.cpp
    viewer/test_pageview.cpp
    # Add new test files here
)

target_link_libraries(unit_tests
    PRIVATE
    pdvu_core
    GTest::gtest_main
    GTest::gmock_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)