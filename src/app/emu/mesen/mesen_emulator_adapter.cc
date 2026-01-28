#include "app/emu/mesen/mesen_emulator_adapter.h"

#include <iostream>

namespace yaze {
namespace emu {
namespace mesen {

namespace {

// Helper to convert Proto Button to Mesen Button (implicit in MesenSocketClient?)
// MesenSocketClient uses its own enum? No, it uses emu::input::SnesButton internally but
// exposed via `agent::Button` in the CLI handler.
// Wait, MesenSocketClient.h header shows it doesn't use SnesButton in the public API?
// Let's check MesenSocketClient.h again.
// It seems I missed checking the Button enum in MesenSocketClient.
// Actually, MesenSocketClient doesn't seem to have a PressButton that takes an enum in the header I saw?
// Let's re-read MesenSocketClient.h carefully.
// I see `absl::Status SendCommand(const std::string& json);`
// I DON'T see `PressButton` in the header I viewed earlier (Id: 34).
// Let me check Id: 34 again.
// Ah, `MesenSocketClient.h` has `GetState`, `Pause`, `Resume`...
// It DOES NOT have input methods like `PressButton` in the header!
// Wait, `EmulatorServiceImpl` has `PressButtons` but that was for the INTERNAL emulator.
// The `MesenSocketClient` I viewed seems to be "Observer" focused + some limited control (Pause/Resume/Step).
// Does Mesen2 API support input? The socket API reference suggests it does.
// But `MesenSocketClient` class might be incomplete.

// Let's double check if I missed Input methods in `MesenSocketClient.h`.
// Lines 178-216: Control Commands (Ping, GetState, Pause, Resume, Reset, Frame, Step).
// Lines 218-249: Memory Access.
// Lines 252-284: Debugging (CpuState, Disasm, Breakpoints, Trace).
// Lines 288-305: ALTTP (GameState, Sprites, Collision).
// Lines 308-325: Save State.
// Lines 328-345: Event Sub.
// Lines 348-354: Low Level.

// CONFIRMED: `MesenSocketClient` DOES NOT have `PressButton` methods.
// I will need to extend `MesenSocketClient` OR send raw JSON commands from the Adapter if I know the API.
// "INPUT" command structure?
// I'll stick to NOT implementing Input for now or simply log "Not Supported". 
// The implementation plan mainly focused on State/Memory/Breakpoints. I'll stub Input.

}  // namespace

MesenEmulatorAdapter::MesenEmulatorAdapter() {
  client_ = std::make_unique<MesenSocketClient>();
  // Auto-connect on startup?
  // Maybe we should allow explicit connect, but IEmulator implies "ready to go" or "init".
  // Let's try to connect immediately.
  auto status = client_->Connect();
  if (!status.ok()) {
    std::cerr << "MesenEmulatorAdapter: Failed to connect on init: " << status << std::endl;
  }
}

MesenEmulatorAdapter::~MesenEmulatorAdapter() {
  if (client_) {
    client_->Disconnect();
  }
}

bool MesenEmulatorAdapter::IsConnected() const {
  return client_ && client_->IsConnected();
}

bool MesenEmulatorAdapter::IsRunning() const {
  if (!IsConnected()) return false;
  auto state = client_->GetState();
  if (!state.ok()) return false;
  return state->running;
}

void MesenEmulatorAdapter::Pause() {
  if (IsConnected()) client_->Pause();
}

void MesenEmulatorAdapter::Resume() {
  if (IsConnected()) client_->Resume();
}

void MesenEmulatorAdapter::Reset() {
  if (IsConnected()) client_->Reset();
}

absl::Status MesenEmulatorAdapter::Step(int count) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->Step(count);
}

absl::Status MesenEmulatorAdapter::StepOver() {
  // Not directly supported by MesenSocketClient
  return absl::UnimplementedError("StepOver not supported via Mesen2 Socket");
}

absl::Status MesenEmulatorAdapter::StepOut() {
  // Not directly supported by MesenSocketClient
  return absl::UnimplementedError("StepOut not supported via Mesen2 Socket");
}

absl::Status MesenEmulatorAdapter::LoadRom(const std::string& path) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  // Mesen2 socket api "LOADROM"?
  // MesenSocketClient doesn't have LoadRom.
  // I will assume not supported via socket for now, or just stub it.
  return absl::UnimplementedError("LoadRom not supported via Mesen2 Socket yet");
}

std::string MesenEmulatorAdapter::GetLoadedRomPath() const {
  // Not supported via socket?
  return "";
}

absl::StatusOr<uint8_t> MesenEmulatorAdapter::ReadByte(uint32_t addr) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->ReadByte(addr);
}

absl::StatusOr<std::vector<uint8_t>> MesenEmulatorAdapter::ReadBlock(uint32_t addr, size_t len) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->ReadBlock(addr, len);
}

absl::Status MesenEmulatorAdapter::WriteByte(uint32_t addr, uint8_t val) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->WriteByte(addr, val);
}

absl::Status MesenEmulatorAdapter::WriteBlock(uint32_t addr, const std::vector<uint8_t>& data) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->WriteBlock(addr, data);
}

absl::Status MesenEmulatorAdapter::GetCpuState(yaze::agent::CPUState* out_state) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  auto cpu_status = client_->GetCpuState();
  if (!cpu_status.ok()) return cpu_status.status();

  auto& mk = *cpu_status;
  out_state->set_a(mk.A);
  out_state->set_x(mk.X);
  out_state->set_y(mk.Y);
  out_state->set_sp(mk.SP);
  out_state->set_d(mk.D);
  out_state->set_pc(mk.PC);
  out_state->set_pb(mk.K);
  out_state->set_db(mk.DBR);
  out_state->set_status(mk.P);
  // Flags extraction from P
  out_state->set_flag_n(mk.P & 0x80);
  out_state->set_flag_v(mk.P & 0x40);
  out_state->set_flag_z(mk.P & 0x02);
  out_state->set_flag_c(mk.P & 0x01);
  
  return absl::OkStatus();
}

absl::Status MesenEmulatorAdapter::GetGameState(yaze::agent::GameStateResponse* response) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  auto game_state = client_->GetGameState();
  if (!game_state.ok()) return game_state.status();
  
  auto& gs = *game_state;
  response->set_link_pos_x(gs.link.x);
  response->set_link_pos_y(gs.link.y);
  response->set_link_state(gs.link.state);
  // Map other fields...
  // Note: EmulatorService proto GameStateResponse has specific fields.
  // We need to map Mesen's comprehensive state to the proto.
  response->set_game_mode(gs.game.mode);
  response->set_link_health(gs.items.current_health);
  
  return absl::OkStatus();
}

absl::Status MesenEmulatorAdapter::RunToBreakpoint(yaze::agent::BreakpointHitResponse* response) {
    return absl::UnimplementedError("RunToBreakpoint not supported via Mesen2 Socket");
}


absl::StatusOr<uint32_t> MesenEmulatorAdapter::AddBreakpoint(uint32_t addr, 
                                       yaze::agent::BreakpointType type,
                                       yaze::agent::CpuType cpu,
                                       const std::string& condition,
                                       const std::string& description) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  
  // Map type
  BreakpointType mesen_type = BreakpointType::kExecute;
  if (type == yaze::agent::READ) mesen_type = BreakpointType::kRead;
  else if (type == yaze::agent::WRITE) mesen_type = BreakpointType::kWrite;
  else if (type == yaze::agent::ACCESS) mesen_type = BreakpointType::kReadWrite;
  
  auto result = client_->AddBreakpoint(addr, mesen_type, condition);
  if (!result.ok()) return result.status();
  
  return static_cast<uint32_t>(*result);
}

absl::Status MesenEmulatorAdapter::RemoveBreakpoint(uint32_t id) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->RemoveBreakpoint(id);
}

absl::Status MesenEmulatorAdapter::ToggleBreakpoint(uint32_t id, bool enabled) {
  // Not supported by MesenSocketClient
  return absl::UnimplementedError("ToggleBreakpoint not supported via Mesen2 Socket");
}

std::vector<yaze::agent::BreakpointInfo> MesenEmulatorAdapter::ListBreakpoints() {
    // Not supported
    return {};
}

absl::Status MesenEmulatorAdapter::PressButton(yaze::agent::Button button) {
    return absl::UnimplementedError("Input not supported via Mesen2 Socket");
}

absl::Status MesenEmulatorAdapter::ReleaseButton(yaze::agent::Button button) {
    return absl::UnimplementedError("Input not supported via Mesen2 Socket");
}

}  // namespace mesen
}  // namespace emu
}  // namespace yaze
