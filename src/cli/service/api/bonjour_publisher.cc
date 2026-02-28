#include "cli/service/api/bonjour_publisher.h"

#include "util/log.h"

#ifdef __APPLE__
#include <dns_sd.h>
#endif

namespace yaze {
namespace cli {
namespace api {

BonjourPublisher::~BonjourPublisher() {
  Unpublish();
}

void BonjourPublisher::Publish(int port, const std::string& rom_title) {
#ifdef __APPLE__
  if (published_)
    return;

  // Build TXT record: version=1, rom=<title>, capabilities=render,commands,annotations
  TXTRecordRef txt;
  TXTRecordCreate(&txt, 0, nullptr);
  TXTRecordSetValue(&txt, "version", 1, "1");

  std::string capabilities = "render,commands,annotations";
  TXTRecordSetValue(&txt, "capabilities",
                    static_cast<uint8_t>(capabilities.size()),
                    capabilities.c_str());

  if (!rom_title.empty()) {
    TXTRecordSetValue(&txt, "rom", static_cast<uint8_t>(rom_title.size()),
                      rom_title.c_str());
  }

  DNSServiceErrorType err =
      DNSServiceRegister(&service_ref_,
                         0,              // no flags
                         0,              // all interfaces
                         nullptr,        // use default host name
                         "_yaze._tcp.",  // service type
                         nullptr,        // default domain
                         nullptr,        // default host
                         htons(static_cast<uint16_t>(port)),
                         TXTRecordGetLength(&txt), TXTRecordGetBytesPtr(&txt),
                         nullptr,   // no callback
                         nullptr);  // no context

  TXTRecordDeallocate(&txt);

  if (err == kDNSServiceErr_NoError) {
    published_ = true;
    LOG_INFO("BonjourPublisher", "Published _yaze._tcp. on port %d", port);
  } else {
    LOG_ERROR("BonjourPublisher", "Failed to publish Bonjour service: %d", err);
  }
#else
  (void)port;
  (void)rom_title;
  LOG_INFO("BonjourPublisher", "Bonjour not available on this platform");
#endif
}

void BonjourPublisher::Unpublish() {
#ifdef __APPLE__
  if (!published_)
    return;
  if (service_ref_) {
    DNSServiceRefDeallocate(service_ref_);
    service_ref_ = nullptr;
  }
  published_ = false;
  LOG_INFO("BonjourPublisher", "Unpublished _yaze._tcp.");
#endif
}

}  // namespace api
}  // namespace cli
}  // namespace yaze
