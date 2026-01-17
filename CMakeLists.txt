cmake_minimum_required(VERSION 3.31)
project(pdvu)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# GoogleTest requires at least C++17
include(CTest) # includes BUILD_TESTING option
if(NOT DEFINED BUILD_TESTING)
    set(BUILD_TESTING OFF CACHE BOOL "Build unit tests")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set flags for builds
set(CMAKE_CXX_FLAGS_DEBUG "-fno-omit-frame-pointer -g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -fno-omit-frame-pointer -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

add_subdirectory(src)

# --- PLOG LOGGER CONFIGURATION ---
option(ENABLE_LOGGING "Enable plog logging" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(ENABLE_LOGGING ON)
endif()

if(ENABLE_LOGGING)
  message(STATUS "Logging ENABLED")
  include(FetchContent)
  FetchContent_Declare(
      plog
      GIT_REPOSITORY https://github.com/SergiusTheBest/plog.git
      GIT_TAG 1.1.11
      GIT_SHALLOW true
  )
  FetchContent_MakeAvailable(plog)
  target_link_libraries(pdvu_core PUBLIC plog::plog)
else ()
  message(STATUS "Logging DISABLED")
endif()
# --- TRACY PROFILER CONFIGURATION ---
option(ENABLE_TRACY "Enable Tracy profiler" OFF)
if(ENABLE_TRACY)
    message(STATUS "Tracy profiling ENABLED")

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

#define macros
target_compile_definitions(pdvu_core PUBLIC
    $<$<BOOL:${ENABLE_LOGGING}>:ENABLE_LOGGING>
    $<$<BOOL:${ENABLE_TRACY}>:TRACY_ENABLE>
)
add_executable(pdvu src/main.cpp)
target_link_libraries(pdvu PRIVATE pdvu_core)

# --- GoogleTest config ---
if (BUILD_TESTING)
    message(STATUS "Unit tests ENABLED")
    add_subdirectory(tests)
else ()
    message(STATUS "Unit tests DISABLED")
endif ()