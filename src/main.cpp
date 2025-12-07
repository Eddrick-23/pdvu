#include <iostream>
#include "viewer.h"

int main() {
    // TO DO: logic for taking in file path as input
    // const std::string file_path = "samples/introduction-to-advanced-mathematics.pdf";
    const std::string file_path("samples/large_174mb.pdf");
    Viewer viewer(file_path);
    viewer.run(); // start main loop
    return 0;

}
