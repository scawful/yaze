#include "app/emu/mesen/mesen_client_registry.h"

namespace yaze {
namespace emu {
namespace mesen {

namespace {
std::shared_ptr<MesenSocketClient> g_mesen_client;
}  // namespace

std::shared_ptr<MesenSocketClient>& MesenClientRegistry::GetClient() {
  return g_mesen_client;
}

void MesenClientRegistry::SetClient(
    std::shared_ptr<MesenSocketClient> client) {
  g_mesen_client = std::move(client);
}

std::shared_ptr<MesenSocketClient> MesenClientRegistry::GetOrCreate() {
  if (!g_mesen_client) {
    g_mesen_client = std::make_shared<MesenSocketClient>();
  }
  return g_mesen_client;
}

}  // namespace mesen
}  // namespace emu
}  // namespace yaze
