#include <iostream>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedN
#endif
#include "viewer.h"
#include <CLI11.hpp>
#include "kitty.h"
#include "tui.h"

int main(int argc, char** argv) {
    CLI::App app("pdvu");

    bool show_license = false;
    app.add_flag("--license", show_license, "Print license info");

    bool enable_ICC = false;
    app.add_flag("--ICC",enable_ICC,
        "Enable ICC Colour management. May slow down parsing speed");

    int n_threads = 1;
    app.add_option("-j,--jobs,--threads", n_threads,
        "Number of worker threads to use. Default 1.");
    n_threads = n_threads <= 0 ? 1 : n_threads;

    std::filesystem::path pdf_path;
    app.add_option("pdf", pdf_path, "Path to PDF file")
    ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);
    if (show_license) {
        if (show_license) {
            std::cout << "OPEN SOURCE LICENSES\n";
            std::cout << "====================\n\n";

            std::cout << "1. CLI11\n";
            std::cout << "   - License: 3-Clause BSD\n";
            std::cout << "   - Copyright (c) 2017-2024 University of Cincinnati\n\n";

            std::cout << "2. Tracy Profiler\n";
            std::cout << "   - License: 3-Clause BSD\n";
            std::cout << "   - Copyright (c) 2017-2025 Bartosz Taudul\n\n";

            std::cout << "3. MuPDF\n";
            std::cout << "   - License: GNU Affero General Public License (AGPL v3)\n";
            std::cout << "   - Copyright (c) 2006-2025 Artifex Software, Inc.\n";
            std::cout << "   - NOTE: Because this software links against MuPDF, this entire\n";
            std::cout << "     application is provided under the AGPL v3 license.\n\n";

            std::cout << "For full license texts, please read LICENSE-3RD-PARTY.txt included\n";
            std::cout << "with this distribution or visit: https://github.com/Eddrick-23/pdvu\n";

            return 0;
        }
    }

    if (pdf_path.extension() != ".pdf") {
        std::cerr << "Error: Pdf file must be provided" << std::endl;
        return 1;
    }

    // 1) Set up parser and load in document
    std::unique_ptr<pdf::Parser> parser = nullptr;
    {
        ZoneScopedN("Parser setup");
        parser = std::make_unique<pdf::MuPDFParser>(enable_ICC);
        if (!parser->load_document(pdf_path)) {
            throw std::runtime_error("failed to load document");
        }
    }

    // 2) setup render engine
    std::unique_ptr<RenderEngine> render_engine = nullptr;
    {
        ZoneScopedN("Render engine setup");
        render_engine = std::make_unique<RenderEngine>(*parser, n_threads);
    }

    //3) set up viewer and run
    Viewer viewer(std::move(parser), std::move(render_engine), n_threads, pdf_path);
    viewer.run(); // start main loop

    return 0;

}
