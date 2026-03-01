#ifndef YAZE_SRC_CLI_SERVICE_API_BONJOUR_PUBLISHER_H_
#define YAZE_SRC_CLI_SERVICE_API_BONJOUR_PUBLISHER_H_

#include <string>

#ifdef __APPLE__
#include <dns_sd.h>
#endif

namespace yaze {
namespace cli {
namespace api {

/// Publishes a `_yaze._tcp.` Bonjour service so iOS clients on the LAN can
/// discover this desktop instance automatically.
class BonjourPublisher {
 public:
  BonjourPublisher() = default;
  ~BonjourPublisher();

  /// Start advertising.  Call after the HTTP server socket is bound.
  void Publish(int port, const std::string& rom_title = "");

  /// Stop advertising.  Safe to call multiple times.
  void Unpublish();

  bool IsPublished() const { return published_; }

  /// Returns true on macOS where Bonjour (dns_sd) is natively available,
  /// false on all other platforms.
  bool IsAvailable() const {
#ifdef __APPLE__
    return true;
#else
    return false;
#endif
  }

 private:
  bool published_ = false;
#ifdef __APPLE__
  DNSServiceRef service_ref_ = nullptr;
#endif
};

}  // namespace api
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_API_BONJOUR_PUBLISHER_H_
