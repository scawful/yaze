// Application entry point - separated from C API implementation
#include "yaze.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>

#include "app/core/controller.h"
#include "app/core/platform/app_delegate.h"
#include "util/flag.h"
#include "util/log.h"
#include "yaze_config.h"

DEFINE_FLAG(std::string, rom_file, "",
            "Path to the ROM file to load. "
            "If not specified, the app will run without a ROM.");

DEFINE_FLAG(
    std::string, log_level, "info",
    "Minimum log level to output (e.g., debug, info, warn, error, fatal).");
DEFINE_FLAG(std::string, log_file, "",
            "Path to the log file. If empty, logs to stderr.");
DEFINE_FLAG(std::string, log_categories, "",
            "Comma-separated list of log categories to enable.");

int yaze_app_main(int argc, char** argv) {
  yaze::util::FlagParser parser(yaze::util::global_flag_registry());
  RETURN_IF_EXCEPTION(parser.Parse(argc, argv));

  // --- Configure Logging System ---
  auto string_to_log_level = [](const std::string& s) {
    std::string upper_s;
    std::transform(s.begin(), s.end(), std::back_inserter(upper_s),
                   ::toupper);
    if (upper_s == "YAZE_DEBUG") return yaze::util::LogLevel::YAZE_DEBUG;
    if (upper_s == "INFO") return yaze::util::LogLevel::INFO;
    if (upper_s == "WARN" || upper_s == "WARNING")
      return yaze::util::LogLevel::WARNING;
    if (upper_s == "ERROR") return yaze::util::LogLevel::ERROR;
    if (upper_s == "FATAL") return yaze::util::LogLevel::FATAL;
    return yaze::util::LogLevel::INFO;  // Default
  };

  auto split_categories = [](const std::string& s) {
    std::set<std::string> result;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
      if (!item.empty()) {
        result.insert(item);
      }
    }
    return result;
  };

  yaze::util::LogManager::instance().configure(
      string_to_log_level(FLAGS_log_level->Get()), FLAGS_log_file->Get(),
      split_categories(FLAGS_log_categories->Get()));

  LOG_INFO("App", "Yaze starting up...");
  LOG_INFO("App", "Version: %s", YAZE_VERSION_STRING);

  std::string rom_filename = "";
  if (!FLAGS_rom_file->Get().empty()) {
    rom_filename = FLAGS_rom_file->Get();
    LOG_INFO("App", "Loading ROM file: %s", rom_filename);
  }

#ifdef __APPLE__
  return yaze_run_cocoa_app_delegate(rom_filename.c_str());
#endif

  auto controller = std::make_unique<yaze::core::Controller>();
  EXIT_IF_ERROR(controller->OnEntry(rom_filename))
  while (controller->IsActive()) {
    controller->OnInput();
    if (auto status = controller->OnLoad(); !status.ok()) {
      LOG_ERROR("App", "Controller OnLoad failed: %s", status.message());
      break;
    }
    controller->DoRender();
  }
  controller->OnExit();
  LOG_INFO("App", "Yaze shutting down.");
  return EXIT_SUCCESS;
}
