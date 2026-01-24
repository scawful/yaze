#include "app/editor/agent/panels/mesen_debug_panel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

namespace {

const char* DirectionToString(uint8_t dir) {
  switch (dir) {
    case 0:
      return "Up";
    case 2:
      return "Down";
    case 4:
      return "Left";
    case 6:
      return "Right";
    default:
      return "???";
  }
}

ImVec4 HealthColor(float ratio) {
  if (ratio > 0.66f)
    return ImVec4(0.2f, 0.8f, 0.2f, 1.0f);  // Green
  if (ratio > 0.33f)
    return ImVec4(0.9f, 0.7f, 0.1f, 1.0f);  // Yellow
  return ImVec4(0.9f, 0.2f, 0.2f, 1.0f);    // Red
}

}  // namespace

MesenDebugPanel::MesenDebugPanel() {
  RefreshSocketList();
  if (!socket_paths_.empty()) {
    selected_socket_index_ = 0;
    std::snprintf(socket_path_buffer_, sizeof(socket_path_buffer_), "%s",
                  socket_paths_[0].c_str());
  }
}
MesenDebugPanel::~MesenDebugPanel() = default;

void MesenDebugPanel::SetClient(
    std::shared_ptr<emu::mesen::MesenSocketClient> client) {
  client_ = std::move(client);
}

bool MesenDebugPanel::IsConnected() const {
  return client_ && client_->IsConnected();
}

void MesenDebugPanel::Connect() {
  if (!client_) {
    client_ = std::make_shared<emu::mesen::MesenSocketClient>();
  }
  auto status = client_->Connect();
  if (!status.ok()) {
    connection_error_ = std::string(status.message());
  } else {
    connection_error_.clear();
    RefreshState();
  }
}

void MesenDebugPanel::ConnectToPath(const std::string& socket_path) {
  if (!client_) {
    client_ = std::make_shared<emu::mesen::MesenSocketClient>();
  }
  auto status = client_->Connect(socket_path);
  if (!status.ok()) {
    connection_error_ = std::string(status.message());
  } else {
    connection_error_.clear();
    RefreshState();
  }
}

void MesenDebugPanel::Disconnect() {
  if (client_) {
    client_->Disconnect();
  }
}

void MesenDebugPanel::RefreshSocketList() {
  socket_paths_ = emu::mesen::MesenSocketClient::ListAvailableSockets();
  if (!socket_paths_.empty()) {
    if (selected_socket_index_ < 0 ||
        selected_socket_index_ >= static_cast<int>(socket_paths_.size())) {
      selected_socket_index_ = 0;
    }
    if (socket_path_buffer_[0] == '\0') {
      std::snprintf(socket_path_buffer_, sizeof(socket_path_buffer_), "%s",
                    socket_paths_[selected_socket_index_].c_str());
    }
  } else {
    selected_socket_index_ = -1;
  }
}

void MesenDebugPanel::RefreshState() {
  if (!IsConnected()) return;

  // Get emulator state
  auto emu_result = client_->GetState();
  if (emu_result.ok()) {
    emu_state_ = *emu_result;
  }

  // Get ALTTP game state
  auto game_result = client_->GetGameState();
  if (game_result.ok()) {
    game_state_ = *game_result;
  }

  // Get sprites
  auto sprite_result = client_->GetSprites(show_all_sprites_);
  if (sprite_result.ok()) {
    sprites_ = *sprite_result;
  }

  // Get CPU state if expanded
  if (show_cpu_state_) {
    auto cpu_result = client_->GetCpuState();
    if (cpu_result.ok()) {
      cpu_state_ = *cpu_result;
    }
  }
}

void MesenDebugPanel::Draw() {
  ImGui::PushID("MesenDebugPanel");

  // Auto-refresh logic
  if (IsConnected() && auto_refresh_) {
    time_since_refresh_ += ImGui::GetIO().DeltaTime;
    if (time_since_refresh_ >= refresh_interval_) {
      RefreshState();
      time_since_refresh_ = 0.0f;
    }
  }

  AgentUI::PushPanelStyle();
  if (ImGui::BeginChild("MesenDebug_Panel", ImVec2(0, 0), true)) {
    DrawConnectionHeader();

    if (IsConnected()) {
      ImGui::Spacing();
      DrawLinkState();
      ImGui::Spacing();
      DrawSpriteList();
      ImGui::Spacing();
      DrawGameMode();
      ImGui::Spacing();
      DrawControlButtons();
    }
  }
  ImGui::EndChild();
  AgentUI::PopPanelStyle();

  ImGui::PopID();
}

void MesenDebugPanel::DrawConnectionHeader() {
  const auto& theme = AgentUI::GetTheme();

  // Header
  ImGui::TextColored(theme.accent_color, "%s Mesen2 Debug", ICON_MD_BUG_REPORT);

  // Connection status indicator
  ImGui::SameLine(ImGui::GetWindowWidth() - 100);
  if (IsConnected()) {
    float pulse = 0.7f + 0.3f * std::sin(ImGui::GetTime() * 2.0f);
    ImVec4 connected_color = ImVec4(0.1f, pulse, 0.3f, 1.0f);
    ImGui::TextColored(connected_color, "%s Connected", ICON_MD_CHECK_CIRCLE);
  } else {
    ImGui::TextColored(theme.status_error, "%s Disconnected", ICON_MD_ERROR);
  }

  ImGui::Separator();

  // Connection controls
  if (!IsConnected()) {
    ImGui::TextDisabled("Socket");
    const char* preview =
        (selected_socket_index_ >= 0 &&
         selected_socket_index_ < static_cast<int>(socket_paths_.size()))
            ? socket_paths_[selected_socket_index_].c_str()
            : "No sockets found";
    ImGui::SetNextItemWidth(-40);
    if (ImGui::BeginCombo("##mesen_socket_combo", preview)) {
      for (int i = 0; i < static_cast<int>(socket_paths_.size()); ++i) {
        bool selected = (i == selected_socket_index_);
        if (ImGui::Selectable(socket_paths_[i].c_str(), selected)) {
          selected_socket_index_ = i;
          std::snprintf(socket_path_buffer_, sizeof(socket_path_buffer_), "%s",
                        socket_paths_[i].c_str());
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_REFRESH "##mesen_refresh")) {
      RefreshSocketList();
    }

    ImGui::TextDisabled("Path");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##mesen_socket_path",
                             "/tmp/mesen2-12345.sock",
                             socket_path_buffer_,
                             sizeof(socket_path_buffer_));

    if (ImGui::Button(ICON_MD_LINK " Connect")) {
      std::string path = socket_path_buffer_;
      if (path.empty() && selected_socket_index_ >= 0 &&
          selected_socket_index_ < static_cast<int>(socket_paths_.size())) {
        path = socket_paths_[selected_socket_index_];
      }
      if (path.empty()) {
        Connect();
      } else {
        ConnectToPath(path);
      }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_AUTO_MODE " Auto")) {
      Connect();
    }
    if (!connection_error_.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(theme.status_error, "%s", connection_error_.c_str());
    }
  } else {
    if (ImGui::Button(ICON_MD_LINK_OFF " Disconnect")) {
      Disconnect();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-refresh", &auto_refresh_);
    if (auto_refresh_) {
      ImGui::SameLine();
      ImGui::SetNextItemWidth(60);
      ImGui::SliderFloat("##RefreshRate", &refresh_interval_, 0.05f, 1.0f,
                         "%.2fs");
    }
  }

  if (!status_message_.empty()) {
    ImGui::Spacing();
    ImGui::TextColored(theme.text_secondary_color, "%s",
                       status_message_.c_str());
  }
}

void MesenDebugPanel::DrawLinkState() {
  const auto& theme = AgentUI::GetTheme();

  if (ImGui::CollapsingHeader(ICON_MD_PERSON " Link State",
                              link_expanded_ ? ImGuiTreeNodeFlags_DefaultOpen
                                             : 0)) {
    link_expanded_ = true;

    const auto& link = game_state_.link;
    const auto& items = game_state_.items;

    // Position
    ImGui::Text("Position:");
    ImGui::SameLine();
    ImGui::TextColored(theme.text_secondary_color, "X=%d, Y=%d  Layer: %d",
                       link.x, link.y, link.layer);

    // Direction and state
    ImGui::Text("Direction:");
    ImGui::SameLine();
    ImGui::TextColored(theme.text_secondary_color, "%s (0x%02X)",
                       DirectionToString(link.direction), link.direction);
    ImGui::SameLine();
    ImGui::Text("  State:");
    ImGui::SameLine();
    ImGui::TextColored(theme.text_secondary_color, "0x%02X", link.state);

    // Health bar
    float health_ratio =
        items.max_health > 0 ? static_cast<float>(items.current_health) /
                                   static_cast<float>(items.max_health)
                             : 0.0f;
    ImVec4 health_color = HealthColor(health_ratio);

    ImGui::Text("Health:");
    ImGui::SameLine();

    // Draw hearts
    int full_hearts = items.current_health / 8;
    int max_hearts = items.max_health / 8;
    ImGui::PushStyleColor(ImGuiCol_Text, health_color);
    for (int i = 0; i < full_hearts; ++i) {
      ImGui::SameLine(0, 0);
      ImGui::Text("%s", ICON_MD_FAVORITE);
    }
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    for (int i = full_hearts; i < max_hearts; ++i) {
      ImGui::SameLine(0, 0);
      ImGui::Text("%s", ICON_MD_FAVORITE_BORDER);
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::TextColored(theme.text_secondary_color, " (%d/%d)",
                       items.current_health, items.max_health);

    // Items row
    ImGui::Text("Items:");
    ImGui::SameLine();
    ImGui::TextColored(theme.text_secondary_color,
                       "Magic: %d  Rupees: %d  Bombs: %d  Arrows: %d",
                       items.magic, items.rupees, items.bombs, items.arrows);
  } else {
    link_expanded_ = false;
  }
}

void MesenDebugPanel::DrawSpriteList() {
  const auto& theme = AgentUI::GetTheme();

  std::string header = absl::StrFormat("%s Active Sprites (%zu/16)",
                                       ICON_MD_PEST_CONTROL, sprites_.size());

  if (ImGui::CollapsingHeader(header.c_str(),
                              sprites_expanded_
                                  ? ImGuiTreeNodeFlags_DefaultOpen
                                  : 0)) {
    sprites_expanded_ = true;

    ImGui::Checkbox("Show inactive", &show_all_sprites_);

    if (sprites_.empty()) {
      ImGui::TextDisabled("  No active sprites");
    } else {
      // Scrollable sprite list
      if (ImGui::BeginChild("SpriteList", ImVec2(0, 120), true)) {
        for (const auto& sprite : sprites_) {
          ImGui::PushID(sprite.slot);

          // Slot indicator
          ImGui::TextColored(theme.text_secondary_color, "[%d]", sprite.slot);
          ImGui::SameLine();

          // Type with color based on state
          ImVec4 sprite_color =
              sprite.state > 0
                  ? ImVec4(0.4f, 0.8f, 0.4f, 1.0f)
                  : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
          ImGui::TextColored(sprite_color, "Type: 0x%02X", sprite.type);

          ImGui::SameLine();
          ImGui::Text("@ %d,%d", sprite.x, sprite.y);

          ImGui::SameLine();
          if (sprite.health > 0) {
            ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "HP:%d",
                               sprite.health);
          }

          ImGui::SameLine();
          ImGui::TextColored(theme.text_secondary_color, "State:%d",
                             sprite.state);

          ImGui::PopID();
        }
      }
      ImGui::EndChild();
    }
  } else {
    sprites_expanded_ = false;
  }
}

void MesenDebugPanel::DrawGameMode() {
  const auto& theme = AgentUI::GetTheme();
  const auto& game = game_state_.game;

  if (ImGui::CollapsingHeader(ICON_MD_GAMEPAD " Game Mode",
                              game_mode_expanded_
                                  ? ImGuiTreeNodeFlags_DefaultOpen
                                  : 0)) {
    game_mode_expanded_ = true;

    ImGui::Text("Mode:");
    ImGui::SameLine();
    ImGui::TextColored(theme.text_secondary_color, "%d (Submode: %d)",
                       game.mode, game.submode);

    ImGui::Text("Location:");
    ImGui::SameLine();
    if (game.indoors) {
      ImGui::TextColored(ImVec4(0.6f, 0.4f, 0.2f, 1.0f),
                         "Dungeon Room: 0x%04X", game.room_id);
    } else {
      ImGui::TextColored(ImVec4(0.2f, 0.6f, 0.2f, 1.0f),
                         "Overworld Area: 0x%02X", game.overworld_area);
    }

    // Frame counter
    ImGui::Text("Frame:");
    ImGui::SameLine();
    ImGui::TextColored(theme.text_secondary_color, "%llu", emu_state_.frame);

    ImGui::SameLine();
    ImGui::Text("  FPS:");
    ImGui::SameLine();
    ImGui::TextColored(theme.text_secondary_color, "%.1f", emu_state_.fps);

    // CPU state toggle
    ImGui::Checkbox("Show CPU State", &show_cpu_state_);
    if (show_cpu_state_) {
      ImGui::Indent();
      ImGui::TextColored(theme.text_secondary_color,
                         "PC=$%02X:%04X  A=$%04X  X=$%04X  Y=$%04X",
                         cpu_state_.K, cpu_state_.PC & 0xFFFF, cpu_state_.A,
                         cpu_state_.X, cpu_state_.Y);
      ImGui::TextColored(theme.text_secondary_color,
                         "SP=$%04X  D=$%04X  DBR=$%02X  P=$%02X",
                         cpu_state_.SP, cpu_state_.D, cpu_state_.DBR,
                         cpu_state_.P);
      ImGui::Unindent();
    }
  } else {
    game_mode_expanded_ = false;
  }
}

void MesenDebugPanel::DrawControlButtons() {
  if (!IsConnected()) return;

  ImGui::Separator();

  // Emulation control buttons
  if (emu_state_.paused) {
    if (ImGui::Button(ICON_MD_PLAY_ARROW " Resume")) {
      client_->Resume();
      RefreshState();
    }
  } else {
    if (ImGui::Button(ICON_MD_PAUSE " Pause")) {
      client_->Pause();
      RefreshState();
    }
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SKIP_NEXT " Step")) {
    client_->Step(1);
    RefreshState();
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FAST_FORWARD " Frame")) {
    client_->Frame();
    RefreshState();
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_REFRESH " Refresh")) {
    RefreshState();
  }

  ImGui::Spacing();
  DrawOverlayControls();
  ImGui::Spacing();
  DrawStateControls();
}

void MesenDebugPanel::DrawOverlayControls() {
  if (!IsConnected()) return;

  const char* colmaps[] = {"A", "B", "C"};
  bool overlay_changed = false;

  ImGui::TextDisabled("Collision Overlay");
  if (ImGui::Checkbox("Enable overlay", &collision_overlay_enabled_)) {
    overlay_changed = true;
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(60);
  if (ImGui::Combo("##colmap", &collision_map_index_, colmaps,
                   IM_ARRAYSIZE(colmaps))) {
    overlay_changed = true;
  }

  if (overlay_changed) {
    auto status = client_->SetCollisionOverlay(
        collision_overlay_enabled_, colmaps[collision_map_index_]);
    if (!status.ok()) {
      status_message_ = std::string(status.message());
    } else {
      status_message_ = collision_overlay_enabled_
                            ? "Collision overlay enabled"
                            : "Collision overlay disabled";
    }
  }
}

void MesenDebugPanel::DrawStateControls() {
  if (!IsConnected()) return;

  ImGui::TextDisabled("Save States");
  ImGui::SetNextItemWidth(60);
  ImGui::InputInt("##state_slot", &save_state_slot_, 1, 1);
  save_state_slot_ = std::clamp(save_state_slot_, 0, 9);

  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_SAVE " Save")) {
    auto status = client_->SaveState(save_state_slot_);
    status_message_ = status.ok()
                          ? absl::StrFormat("Saved state %d", save_state_slot_)
                          : std::string(status.message());
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_UPLOAD " Load")) {
    auto status = client_->LoadState(save_state_slot_);
    status_message_ = status.ok()
                          ? absl::StrFormat("Loaded state %d", save_state_slot_)
                          : std::string(status.message());
  }

  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_PHOTO_CAMERA " Screenshot")) {
    auto screenshot = client_->Screenshot();
    if (!screenshot.ok()) {
      status_message_ = std::string(screenshot.status().message());
    } else {
      status_message_ = "Screenshot captured (base64 in clipboard)";
      ImGui::SetClipboardText(screenshot->c_str());
    }
  }
}

}  // namespace editor
}  // namespace yaze
