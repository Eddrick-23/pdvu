
# pdvu

<u>__This is project is still in progress, large changes to the codebase expected__</u>

**pdvu** is a high-performance, terminal-based PDF viewer written in C++. 

Designed with a strict focus on memory efficiency, pdvu uses a prefetchless approach with on demand page loading.
Targeting a mininmal memory footprint compared to other pdf viewers.

![License](https://img.shields.io/badge/license-AGPL%20v3-blue)
![Language](https://img.shields.io/badge/language-C%2B%2B23-orange)

## Requirements

* MacOS (have not tested on linux platform yet)
* **C++23 Compiler** (GCC 10+ or Clang 10+)
* **CMake** 3.15+
* **MuPDF** (Built as a static library)
* **Terminal Emulator** with Kitty Graphics Protocol support. E.g. Kitty, Ghostty, Wezterm...

## Installation & Build

`pdvu` links statically against MuPDF. You must have the MuPDF source available in the `external` directory.

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/Eddrick-23/pdvu.git
    cd pdvu
    ```

2.  **Prepare MuPDF:**
    Place the MuPDF source code in `external/mupdf` and build the static libraries (`libmupdf.a`, `libmupdf-third.a`).
    ```bash
    mkdir external && cd external
    git clone --recursive https://git.ghostscript.com/mupdf.git
    cd mupdf
    make HAVE_X11=no HAVE_GLUT=no prefix=./build/release install
    ```

3.  **Build pdvu:**
    ```bash
    #Run from project root
    cd ../.. 
    
    # Configure
    cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release

    # Build
    cmake --build build/release

    # Run
    ./build/release/pdvu <path to pdf>
    ```

## Benchmarks

## Roadmap
Future updates are planned for the following features
- Page Zooming and Panning
- "Ctrl f" for word searching
