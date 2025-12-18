cmake_minimum_required(VERSION 3.31)
project(pdvu)

# GoogleTest requires at least C++17
include(CTest) # includes BUILD_TESTING option
if(NOT DEFINED BUILD_TESTING)
    set(BUILD_TESTING OFF CACHE BOOL "Build unit tests")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)

option(ENABLE_TRACY "Enable Tracy profiler" OFF)
# --- TRACY PROFILER CONFIGURATION ---
if(ENABLE_TRACY)
    message(STATUS "Tracy profiling ENABLED")

    # Define macro for the library sources
    target_compile_definitions(pdvu_core PUBLIC TRACY_ENABLE)

    # Build Tracy Client
    set(TRACY_CLIENT_SRC "${CMAKE_SOURCE_DIR}/external/tracy/public/TracyClient.cpp")
    add_library(TracyClient STATIC ${TRACY_CLIENT_SRC})
    target_compile_definitions(TracyClient PUBLIC TRACY_ENABLE)
    target_include_directories(TracyClient PUBLIC "${CMAKE_SOURCE_DIR}/external/tracy/public")

    find_package(Threads REQUIRED)
    target_link_libraries(TracyClient PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

    # Link Tracy to the core library
    target_link_libraries(pdvu_core PUBLIC TracyClient)
else()
    message(STATUS "Tracy profiling DISABLED")
endif()

add_executable(pdvu src/main.cpp)

target_link_libraries(pdvu PRIVATE pdvu_core)

# --- GoogleTest config ---
if (BUILD_TESTING)
    message(STATUS "Unit tests ENABLED")
    add_subdirectory(tests)
else ()
    message(STATUS "Unit tests DISABLED")
endif ()