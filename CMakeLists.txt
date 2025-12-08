cmake_minimum_required(VERSION 3.31)
project(pdvu)

set(CMAKE_CXX_STANDARD 23)

set(SOURCES
        src/main.cpp
        src/terminal.cpp
        src/renderer.cpp
        src/parser.cpp
        src/viewer.cpp
        src/tempfile.cpp
        src/shm.cpp
)

add_executable(pdvu ${SOURCES})

target_include_directories(pdvu PRIVATE external) #CLI11
target_precompile_headers(pdvu PRIVATE <CLI11.hpp>)

set(MUPDF_ROOT "${CMAKE_SOURCE_DIR}/external/mupdf")

target_include_directories(pdvu PRIVATE
        src
        "${MUPDF_ROOT}/include"
)

#Link against MuPdf library
target_link_libraries(pdvu PRIVATE
        "${MUPDF_ROOT}/build/release/libmupdf.a"
        "${MUPDF_ROOT}/build/release/libmupdf-third.a"
)

# --- TRACY PROFILER CONFIGURATION ---

# Enable for Debug OR RelWithDebInfo (Optimized + Symbols is best for profiling)
if(CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")

    # 1. Define the macro globally
    add_compile_definitions(TRACY_ENABLE)

    # 2. You only need THIS single file. It includes the others internally.
    set(TRACY_CLIENT_SRC "${CMAKE_SOURCE_DIR}/external/tracy/public/TracyClient.cpp")

    # 3. Create the library
    add_library(TracyClient STATIC ${TRACY_CLIENT_SRC})

    # 4. Include directories
    target_include_directories(TracyClient PUBLIC
            "${CMAKE_SOURCE_DIR}/external/tracy/public"
    )

    # 5. Link required system libraries (Pthread and dl are mandatory on Linux)
    find_package(Threads REQUIRED)
    target_link_libraries(TracyClient PUBLIC
            Threads::Threads
            ${CMAKE_DL_LIBS} # This handles -ldl
    )

    # 6. Link Tracy to your executable
    target_link_libraries(pdvu PRIVATE TracyClient)

    message(STATUS "Tracy profiling ENABLED (Build Type: ${CMAKE_BUILD_TYPE})")
else()
    message(STATUS "Tracy profiling DISABLED")
endif()