#include "app/editor/agent/asm_follow_service.h"

#include "app/emu/mesen/mesen_client_registry.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

AsmFollowService::AsmFollowService(emu::debug::SymbolProvider* symbol_provider)
    : symbol_provider_(symbol_provider) {}

void AsmFollowService::Update() {
  if (!enabled_) return;

  double current_time = ImGui::GetTime();
  if (current_time - last_poll_time_ < 0.1) return; // 10Hz poll for PC sync
  last_poll_time_ = current_time;

  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  if (!client || !client->IsConnected()) return;

  auto cpu_state_or = client->GetCpuState();
  if (!cpu_state_or.ok()) return;

  const auto& cpu = *cpu_state_or;
  // Combine Bank (K) and PC
  uint32_t combined_pc = (cpu.K << 16) | (cpu.PC & 0xFFFF);

  if (combined_pc != last_pc_) {
    last_pc_ = combined_pc;
    
    if (symbol_provider_) {
      std::string loc = symbol_provider_->GetSourceLocation(combined_pc);
      if (!loc.empty() && callback_) {
        callback_(loc);
      }
    }
  }
}

}  // namespace editor
}  // namespace yaze
