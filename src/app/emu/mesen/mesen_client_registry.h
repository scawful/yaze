#ifndef YAZE_APP_EMU_MESEN_MESEN_CLIENT_REGISTRY_H_
#define YAZE_APP_EMU_MESEN_MESEN_CLIENT_REGISTRY_H_

#include <memory>

#include "app/emu/mesen/mesen_socket_client.h"

namespace yaze {
namespace emu {
namespace mesen {

/**
 * @brief Shared Mesen2 socket client registry
 *
 * Provides a single shared client instance for UI panels and CLI handlers.
 */
class MesenClientRegistry {
 public:
  static std::shared_ptr<MesenSocketClient>& GetClient();
  static void SetClient(std::shared_ptr<MesenSocketClient> client);
  static std::shared_ptr<MesenSocketClient> GetOrCreate();
};

}  // namespace mesen
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_MESEN_MESEN_CLIENT_REGISTRY_H_
