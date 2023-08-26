#ifndef YAZE_APP_EMU_LOG_H_
#define YAZE_APP_EMU_LOG_H_

#include <iostream>
#include <string>

namespace yaze {
namespace app {
namespace emu {

// Logger.h
class Logger {
 public:
  static Logger& GetInstance() {
    static Logger instance;
    return instance;
  }

  void Log(const std::string& message) const {
    // Write log messages to a file or console
    std::cout << message << std::endl;
  }

 private:
  Logger() = default;
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
};

// Loggable.h
class Loggable {
 protected:
  Logger& logger_ = Logger::GetInstance();

  virtual ~Loggable() = default;
  virtual void LogMessage(const std::string& message) { logger_.Log(message); }
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_LOG_H_