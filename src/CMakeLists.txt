set(CORE_SOURCES
    viewer/viewer.cpp
    viewer/pageview.cpp
    viewer/frame_layout.cpp
    terminal/terminal.cpp
    terminal/kitty.cpp
    terminal/tui.cpp
    terminal/inputbar.cpp
    render/parser.cpp
    render/bounds.cpp
    render/render_engine.cpp
    render/threadpool.cpp
    utils/tempfile.cpp
    utils/shm.cpp
)

# create core library
add_library(pdvu_core STATIC ${CORE_SOURCES})

target_include_directories(pdvu_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(MUPDF_ROOT "${CMAKE_SOURCE_DIR}/external/mupdf")
# Link against MuPdf library
add_library(mupdf STATIC IMPORTED)
set_target_properties(mupdf PROPERTIES
    IMPORTED_LOCATION "${MUPDF_ROOT}/build/release/libmupdf.a"
    INTERFACE_INCLUDE_DIRECTORIES "${MUPDF_ROOT}/include"
)
add_library(mupdf_third STATIC IMPORTED)
set_target_properties(mupdf_third PROPERTIES
    IMPORTED_LOCATION "${MUPDF_ROOT}/build/release/libmupdf-third.a"
)
target_link_libraries(pdvu_core
    PRIVATE
    pdvu_compiler_flags
    PUBLIC
    mupdf
    mupdf_third
)