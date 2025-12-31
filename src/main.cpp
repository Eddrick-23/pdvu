#include <cstdio>
#include <print>
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedN
#endif
#include "viewer.h"
#include <CLI11.hpp>
#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Log.h>

int main(int argc, char **argv) {
  CLI::App app("pdvu");

  bool show_license = false;
  app.add_flag("--license", show_license, "Print license info");

  bool enable_ICC = false;
  app.add_flag("--ICC", enable_ICC,
               "Enable ICC Colour management. May slow down parsing speed");

  bool use_shm = false;
  app.add_flag(
      "--shm", use_shm,
      "Use Posix Shared Memory image transmission. Default uses temp file");

  bool enable_cache = true;
  app.add_flag("--nocache{false}", enable_cache,
               "Disable PDVU caching of rendered pages and display lists. "
               "(MuPDF cache unaffected)");

  int n_threads = 1;
  app.add_option("-j,--jobs,--threads", n_threads,
                 "Number of worker threads to use. Default 1.");
  n_threads = n_threads <= 0 ? 1 : n_threads;

  std::filesystem::path pdf_path;
  app.add_option("pdf", pdf_path, "Path to PDF file")->check(CLI::ExistingFile);

  CLI11_PARSE(app, argc, argv);
  if (show_license) {
    std::println("OPEN SOURCE LICENSES");
    std::print("====================\n\n");

    std::println("1. PLOG");
    std::println("   - License: MIT");
    std::print("   - Copyright (c) 2022 Sergey Podobry\n\n");

    std::println("2. CLI11");
    std::println("   - License: 3-Clause BSD");
    std::print("   - Copyright (c) 2017-2024 University of Cincinnati\n\n");

    std::println("3. Tracy Profiler");
    std::println("   - License: 3-Clause BSD");
    std::print("   - Copyright (c) 2017-2025 Bartosz Taudul\n\n");

    std::println("4. MuPDF");
    std::println("   - License: GNU Affero General Public License (AGPL v3)");
    std::println("   - Copyright (c) 2006-2025 Artifex Software, Inc.");
    std::println(
        "   - NOTE: Because this software links against MuPDF, this entire");
    std::print("     application is provided under the AGPL v3 license.\n\n");

    std::println(
        "For full license texts, please read LICENSE-3RD-PARTY.txt included");
    std::println("with this distribution or visit: "
                 "https://github.com/Eddrick-23/pdvu");

    return 0;
  }

  if (pdf_path.extension() != ".pdf") {
    std::println(stderr, "Error: Pdf file must be provided");
    return 1;
  }
  // logging for debugging
  plog::init(plog::debug, "/tmp/pdvu.log", 10000000, 1);

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
    render_engine =
        std::make_unique<RenderEngine>(*parser, n_threads, enable_cache);
  }

  // 3) set up viewer and run
  Viewer viewer(std::move(parser), std::move(render_engine), use_shm);
  PLOG_INFO << "Start up complete, starting loop";
  viewer.run(); // start main loop
  PLOG_INFO << "Shutdown session";
  return 0;
}
