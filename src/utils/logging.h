#pragma once
#include <expected>
#include <filesystem>
#include <string>

#include "plog/Initializers/RollingFileInitializer.h"

namespace Logging {

/**
 *  @breif sets up plog logger to write to target log_file
 * @param enable_logging Control if plog should be set up or not.
 *                       If false defaults to plog::none which drops all logs
 * @param log_file path to log file
 * @return true if setup successful, false otherwise.
 */
[[nodiscard]] inline std::expected<void, std::string> setup_logging(
    bool enable_logging, const std::filesystem::path& log_file) {
  if (!enable_logging) {
    plog::init(plog::none, log_file.c_str());
    return {};
  }

  try {
    // manually ensure parent path exists, plog won't auto create paths
    if (log_file.has_parent_path()) {
      std::filesystem::create_directories(log_file.parent_path());
    }
    // Initialise Plog
    plog::init(plog::debug, log_file.c_str(), 10'000'000, 1);
    return {};
  } catch (const std::exception& e) {
    return std::unexpected(std::string(e.what()));
  }
}
}  // namespace Logging