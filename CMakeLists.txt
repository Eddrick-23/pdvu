cmake_minimum_required(VERSION 3.31)
project(Termivue)

set(CMAKE_CXX_STANDARD 23)

set(SOURCES
        src/main.cpp
        src/terminal.cpp
        src/renderer.cpp
        src/parser.cpp
        src/viewer.cpp
)

add_executable(Termivue ${SOURCES})

set(MUPDF_ROOT "${CMAKE_SOURCE_DIR}/external/mupdf")

target_include_directories(Termivue PRIVATE
        src               # So you can do #include "terminal.h"
        "${MUPDF_ROOT}/include"
)

#Link against MuPdf library
target_link_libraries(Termivue PRIVATE
        "${MUPDF_ROOT}/build/release/libmupdf.a"
        "${MUPDF_ROOT}/build/release/libmupdf-third.a"
)
