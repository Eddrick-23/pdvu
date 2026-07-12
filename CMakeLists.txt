cmake_minimum_required(VERSION 3.31)
project(pdvu)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- GLOBAL LANGUAGE CONFIGS ---
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- TESTING OPTIONS ---
include(CTest) # includes BUILD_TESTING option
if (NOT DEFINED BUILD_TESTING)
  set(BUILD_TESTING OFF CACHE BOOL "Build unit tests")
endif ()

# --- THIRD PARTY: PLOG LOGGER ---
option(ENABLE_LOGGING "Enable plog logging" OFF)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(ENABLE_LOGGING ON)
endif ()

if (ENABLE_LOGGING)
  message(STATUS "Logging ENABLED")
  include(FetchContent)
  FetchContent_Declare(
      plog
      GIT_REPOSITORY https://github.com/SergiusTheBest/plog.git
      GIT_TAG 1.1.11
      GIT_SHALLOW true
  )
  FetchContent_MakeAvailable(plog)
else ()
  message(STATUS "Logging DISABLED")
endif ()

# --- THIRD PARTY: TRACY PROFILER ---
option(ENABLE_TRACY "Enable Tracy profiler" OFF)
if (ENABLE_TRACY)
  message(STATUS "Tracy profiling ENABLED")

  # Build Tracy Client
  set(TRACY_CLIENT_SRC "${CMAKE_SOURCE_DIR}/external/tracy/public/TracyClient.cpp")
  add_library(TracyClient STATIC ${TRACY_CLIENT_SRC})
  target_compile_definitions(TracyClient PUBLIC TRACY_ENABLE)
  target_include_directories(TracyClient PUBLIC "${CMAKE_SOURCE_DIR}/external/tracy/public")

  find_package(Threads REQUIRED)
  target_link_libraries(TracyClient PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
else ()
  message(STATUS "Tracy profiling DISABLED")
endif ()

# --- GLOBAL COMPILER FLAGS ---
add_library(pdvu_compiler_flags INTERFACE)
set(BASE_COMPILER_FLAGS
    $<$<CONFIG:Debug>:-fno-omit-frame-pointer -ggdb -O0>
    $<$<CONFIG:RelWithDebInfo>:-O2 -ggdb -fno-omit-frame-pointer>
    $<$<CONFIG:Release>:-O3>
)
target_compile_options(pdvu_compiler_flags INTERFACE
    ${BASE_COMPILER_FLAGS}
    -Wall
    -Weffc++
    -Wextra
    -Wconversion
    -Wsign-conversion
    -Werror
)
target_compile_definitions(pdvu_compiler_flags INTERFACE
    $<$<BOOL:${ENABLE_LOGGING}>:ENABLE_LOGGING>
    $<$<BOOL:${ENABLE_TRACY}>:TRACY_ENABLE>
)

# --- PROJECT SOURCE TARGETS ---
add_subdirectory(src)

add_executable(pdvu src/main.cpp)
target_link_libraries(pdvu PRIVATE pdvu_core pdvu_compiler_flags)

# --- LINK OPTIONAL LIBRARIES IF ENABLED ---
if (ENABLE_LOGGING)
  target_link_libraries(pdvu_core PUBLIC plog::plog)
endif ()
if (ENABLE_TRACY)
  target_link_libraries(pdvu_core PUBLIC TracyClient)
endif ()


# --- LINK UNIT TESTS ---
if (BUILD_TESTING)
  message(STATUS "Unit tests ENABLED")
  add_subdirectory(tests)
else ()
  message(STATUS "Unit tests DISABLED")
endif ()