#ifndef YAZE_APP_ACTIVITY_FILE_H_
#define YAZE_APP_ACTIVITY_FILE_H_

#include <cstdint>
#include <string>

namespace yaze::app {

/**
 * @brief Status information for an active YAZE instance
 *
 * This structure is serialized to JSON and written to /tmp/yaze-<pid>.status
 * to enable discovery by the Oracle Agent Manager and other tools.
 */
struct ActivityStatus {
  int pid = 0;
  std::string version;
  std::string socket_path;  // gRPC endpoint if enabled (e.g., "localhost:50052")
  std::string active_rom;   // Path to currently loaded ROM
  int64_t start_timestamp = 0;  // Unix epoch seconds
};

/**
 * @class ActivityFile
 * @brief Manages a JSON status file for instance discovery
 *
 * Creates a file at /tmp/yaze-<pid>.status on construction and
 * removes it on destruction. Updates can be pushed via Update().
 *
 * This enables the Oracle Agent Manager to discover running YAZE
 * instances and route commands to the appropriate gRPC endpoint.
 */
class ActivityFile {
 public:
  /**
   * @brief Construct an activity file at the specified path
   * @param path Full path to the status file (e.g., /tmp/yaze-12345.status)
   */
  explicit ActivityFile(const std::string& path);

  /**
   * @brief Destructor removes the status file
   */
  ~ActivityFile();

  // Non-copyable
  ActivityFile(const ActivityFile&) = delete;
  ActivityFile& operator=(const ActivityFile&) = delete;

  // Movable
  ActivityFile(ActivityFile&& other) noexcept;
  ActivityFile& operator=(ActivityFile&& other) noexcept;

  /**
   * @brief Update the status file with new information
   * @param status The current activity status to write
   */
  void Update(const ActivityStatus& status);

  /**
   * @brief Check if the status file exists on disk
   */
  bool Exists() const;

  /**
   * @brief Get the path to the status file
   */
  const std::string& GetPath() const { return path_; }

 private:
  std::string path_;
};

}  // namespace yaze::app

#endif  // YAZE_APP_ACTIVITY_FILE_H_
