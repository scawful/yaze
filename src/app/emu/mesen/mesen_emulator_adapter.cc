#include "app/emu/mesen/mesen_emulator_adapter.h"

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"

namespace yaze {
namespace emu {
namespace mesen {

namespace {

absl::StatusOr<emu::input::SnesButton> MapButton(InputButton button) {
  switch (button) {
    case InputButton::kA: return emu::input::SnesButton::A;
    case InputButton::kB: return emu::input::SnesButton::B;
    case InputButton::kX: return emu::input::SnesButton::X;
    case InputButton::kY: return emu::input::SnesButton::Y;
    case InputButton::kL: return emu::input::SnesButton::L;
    case InputButton::kR: return emu::input::SnesButton::R;
    case InputButton::kSelect: return emu::input::SnesButton::SELECT;
    case InputButton::kStart: return emu::input::SnesButton::START;
    case InputButton::kUp: return emu::input::SnesButton::UP;
    case InputButton::kDown: return emu::input::SnesButton::DOWN;
    case InputButton::kLeft: return emu::input::SnesButton::LEFT;
    case InputButton::kRight: return emu::input::SnesButton::RIGHT;
    default:
      return absl::InvalidArgumentError(absl::StrFormat(
          "Unsupported button value: %d", static_cast<int>(button)));
  }
}

}  // namespace

MesenEmulatorAdapter::MesenEmulatorAdapter()
    : client_(std::make_unique<MesenSocketClient>()) {}

MesenEmulatorAdapter::~MesenEmulatorAdapter() {
  if (client_) {
    client_->Disconnect();
  }
}

absl::Status MesenEmulatorAdapter::Connect() {
  return client_->Connect();
}

absl::Status MesenEmulatorAdapter::Connect(const std::string& socket_path) {
  return client_->Connect(socket_path);
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
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->Step(1, "over");
}

absl::Status MesenEmulatorAdapter::StepOut() {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->Step(1, "out");
}

absl::Status MesenEmulatorAdapter::LoadRom(const std::string& path) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return absl::UnimplementedError(
      "LoadRom not supported via Mesen2 Socket yet");
}

std::string MesenEmulatorAdapter::GetLoadedRomPath() const {
  return "";
}

absl::StatusOr<uint8_t> MesenEmulatorAdapter::ReadByte(uint32_t addr) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->ReadByte(addr);
}

absl::StatusOr<std::vector<uint8_t>> MesenEmulatorAdapter::ReadBlock(
    uint32_t addr, size_t len) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->ReadBlock(addr, len);
}

absl::Status MesenEmulatorAdapter::WriteByte(uint32_t addr, uint8_t val) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->WriteByte(addr, val);
}

absl::Status MesenEmulatorAdapter::WriteBlock(
    uint32_t addr, const std::vector<uint8_t>& data) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->WriteBlock(addr, data);
}

absl::Status MesenEmulatorAdapter::GetCpuState(CpuStateSnapshot* out_state) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  auto cpu_status = client_->GetCpuState();
  if (!cpu_status.ok()) return cpu_status.status();

  auto& mk = *cpu_status;
  out_state->a = mk.A;
  out_state->x = mk.X;
  out_state->y = mk.Y;
  out_state->sp = mk.SP;
  out_state->d = mk.D;
  out_state->pc = mk.PC;
  out_state->pb = mk.K;
  out_state->db = mk.DBR;
  out_state->status = mk.P;
  out_state->flag_n = mk.P & 0x80;
  out_state->flag_v = mk.P & 0x40;
  out_state->flag_z = mk.P & 0x02;
  out_state->flag_c = mk.P & 0x01;

  return absl::OkStatus();
}

absl::Status MesenEmulatorAdapter::GetGameState(GameSnapshot* response) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  auto game_state = client_->GetGameState();
  if (!game_state.ok()) return game_state.status();

  auto& gs = *game_state;
  response->link_pos_x = gs.link.x;
  response->link_pos_y = gs.link.y;
  response->link_state = gs.link.state;
  response->game_mode = gs.game.mode;
  response->link_health = gs.items.current_health;

  return absl::OkStatus();
}

absl::Status MesenEmulatorAdapter::RunToBreakpoint(
    BreakpointHitResult* response) {
  return absl::UnimplementedError(
      "RunToBreakpoint not supported via Mesen2 Socket");
}

absl::StatusOr<uint32_t> MesenEmulatorAdapter::AddBreakpoint(
    uint32_t addr, BreakpointKind type, CpuKind cpu,
    const std::string& condition, const std::string& description) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");

  if (cpu == CpuKind::kSpc700) {
    return absl::UnimplementedError(
        "SPC700 breakpoints are not supported via Mesen2 Socket");
  }
  if (cpu != CpuKind::kUnspecified && cpu != CpuKind::k65816) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Unsupported CPU kind for breakpoint: %d", static_cast<int>(cpu)));
  }

  mesen::BreakpointType mesen_type = mesen::BreakpointType::kExecute;
  if (type == BreakpointKind::kRead) mesen_type = mesen::BreakpointType::kRead;
  else if (type == BreakpointKind::kWrite)
    mesen_type = mesen::BreakpointType::kWrite;
  else if (type == BreakpointKind::kAccess)
    mesen_type = mesen::BreakpointType::kReadWrite;

  auto result = client_->AddBreakpoint(addr, mesen_type, condition);
  if (!result.ok()) return result.status();

  return static_cast<uint32_t>(*result);
}

absl::Status MesenEmulatorAdapter::RemoveBreakpoint(uint32_t breakpoint_id) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->RemoveBreakpoint(breakpoint_id);
}

absl::Status MesenEmulatorAdapter::ToggleBreakpoint(uint32_t breakpoint_id,
                                                     bool enabled) {
  return absl::UnimplementedError(
      "ToggleBreakpoint not supported via Mesen2 Socket");
}

std::vector<BreakpointSnapshot> MesenEmulatorAdapter::ListBreakpoints() {
  return {};
}

absl::Status MesenEmulatorAdapter::PressButton(InputButton button) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  auto mapped_button = MapButton(button);
  if (!mapped_button.ok()) return mapped_button.status();
  return client_->SetButton(*mapped_button, true);
}

absl::Status MesenEmulatorAdapter::ReleaseButton(InputButton button) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  auto mapped_button = MapButton(button);
  if (!mapped_button.ok()) return mapped_button.status();
  return client_->SetButton(*mapped_button, false);
}

absl::Status MesenEmulatorAdapter::SaveState(int slot) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->SaveState(slot);
}

absl::Status MesenEmulatorAdapter::LoadState(int slot) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->LoadState(slot);
}

bool MesenEmulatorAdapter::SupportsFeature(EmulatorFeature feature) const {
  switch (feature) {
    case EmulatorFeature::kCollisionOverlay:
    case EmulatorFeature::kSaveState:
    case EmulatorFeature::kLoadState:
    case EmulatorFeature::kScreenshot:
    case EmulatorFeature::kEventSubscription:
    case EmulatorFeature::kDisassembly:
    case EmulatorFeature::kTraceLog:
      return IsConnected();
    default:
      return false;
  }
}

absl::Status MesenEmulatorAdapter::SetCollisionOverlay(bool enable) {
  if (!IsConnected()) return absl::UnavailableError("Not connected");
  return client_->SetCollisionOverlay(enable);
}

}  // namespace mesen
}  // namespace emu
}  // namespace yaze
