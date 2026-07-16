#include "message_editor.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/editor/message/message_id_resolver.h"
#include "app/editor/system/session/hack_manifest_save_validation.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "rom/rom.h"
#include "rom/transaction.h"
#include "rom/write_fence.h"
#include "util/file_util.h"
#include "util/hex.h"
#include "util/log.h"

namespace yaze {
namespace editor {

namespace {
std::string DisplayTextOverflowError(int pos, bool bank) {
  int space = bank ? kTextDataEnd - kTextData : kTextData2End - kTextData2;
  std::string bankSTR = bank ? "1st" : "2nd";
  std::string posSTR =
      bank ? absl::StrFormat("%X4", pos & 0xFFFF)
           : absl::StrFormat("%X4", (pos - kTextData2) & 0xFFFF);
  std::string message = absl::StrFormat(
      "There is too much text data in the %s block to save.\n"
      "Available: %X4 | Used: %s",
      bankSTR, space, posSTR);
  return message;
}
}  // namespace

using ImGui::BeginChild;
using ImGui::BeginTable;
using ImGui::Button;
using ImGui::EndChild;
using ImGui::EndTable;
using ImGui::InputTextMultiline;
using ImGui::PopID;
using ImGui::PushID;
using ImGui::SameLine;
using ImGui::Separator;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableSetupColumn;
using ImGui::Text;
using ImGui::TextWrapped;

constexpr ImGuiTableFlags kMessageTableFlags = ImGuiTableFlags_Hideable |
                                               ImGuiTableFlags_Borders |
                                               ImGuiTableFlags_Resizable;

void MessageEditor::Initialize() {
  font_graphics_loaded_ = false;
  dirty_state_ = {};
  transaction_dirty_snapshot_ = {};
  transaction_saved_domains_ = {};
  transaction_expanded_address_snapshot_.clear();
  save_transaction_active_ = false;
  // Register panels with WorkspaceWindowManager (dependency injection)
  if (!dependencies_.window_manager)
    return;

  auto* window_manager = dependencies_.window_manager;
  const size_t session_id = dependencies_.session_id;

  // Register WindowContent implementations (they provide both metadata and drawing)
  window_manager->RegisterWindowContent(
      std::make_unique<MessageListPanel>([this]() { DrawMessageList(); }));
  window_manager->RegisterWindowContent(
      std::make_unique<MessageEditorPanel>([this]() { DrawCurrentMessage(); }));
  window_manager->RegisterWindowContent(
      std::make_unique<FontAtlasPanel>([this]() {
        DrawFontAtlas();
        DrawExpandedMessageSettings();
      }));
  window_manager->RegisterWindowContent(
      std::make_unique<DictionaryPanel>([this]() {
        DrawTextCommands();
        DrawSpecialCharacters();
        DrawDictionary();
      }));

  // Show message list by default
  window_manager->OpenWindow(session_id, "message.message_list");

  for (int i = 0; i < kWidthArraySize; i++) {
    message_preview_.width_array[i] = rom()->data()[kCharactersWidth + i];
  }

  message_preview_.all_dictionaries_ = BuildDictionaryEntries(rom());
  list_of_texts_ = ReadAllTextData(rom()->mutable_data());
  LOG_INFO("MessageEditor", "Loaded %zu messages from ROM",
           list_of_texts_.size());

  ResolveFontPalette();
  LoadFontGraphics();
  if (!font_preview_colors_.empty()) {
    *current_font_gfx16_bitmap_.mutable_palette() = font_preview_colors_;
  }
  parsed_messages_ =
      ParseMessageData(list_of_texts_, message_preview_.all_dictionaries_);

  expanded_message_base_id_ = ResolveExpandedMessageBaseId();

  if (!list_of_texts_.empty()) {
    // Default to message 1 if available, otherwise 0
    size_t default_idx = list_of_texts_.size() > 1 ? 1 : 0;
    current_message_ = list_of_texts_[default_idx];
    current_message_index_ = current_message_.ID;
    current_message_is_expanded_ = false;
    message_text_box_.text = parsed_messages_[current_message_.ID];
    current_parse_errors_.clear();
    current_parse_warnings_.clear();
    DrawMessagePreview();
  } else {
    LOG_ERROR("MessageEditor", "No messages found in ROM!");
  }
}

bool MessageEditor::OpenMessageById(int display_id) {
  // Do not discard an invalid in-progress draft by navigating away. The user
  // can correct it or undo it before selecting another message.
  if (!current_parse_errors_.empty()) {
    return false;
  }

  const int vanilla_count = static_cast<int>(list_of_texts_.size());
  const int expanded_base_id = expanded_message_base_id_;

  int expanded_count = static_cast<int>(expanded_messages_.size());
  auto resolved = ResolveMessageDisplayId(display_id, vanilla_count,
                                          expanded_base_id, expanded_count);

  // Convenience: if an expanded ID is requested but we haven't loaded expanded
  // messages yet, try loading from ROM once.
  if (!resolved.has_value() && expanded_count == 0 &&
      display_id >= expanded_base_id && rom_ && rom_->is_loaded()) {
    const int start = GetExpandedTextDataStart();
    const int end = GetExpandedTextDataEnd();
    const size_t rom_size = rom_->size();
    if (start >= 0 && end >= start && static_cast<size_t>(end) < rom_size) {
      const auto status = LoadExpandedMessagesFromRom();
      if (!status.ok()) {
        LOG_DEBUG("MessageEditor",
                  "OpenMessageById: expanded load skipped/failed: %s",
                  std::string(status.message()).c_str());
      }
      expanded_count = static_cast<int>(expanded_messages_.size());
      resolved = ResolveMessageDisplayId(display_id, vanilla_count,
                                         expanded_base_id, expanded_count);
    } else {
      LOG_DEBUG("MessageEditor",
                "OpenMessageById: expanded region out of bounds (0x%X-0x%X, "
                "rom=0x%zX)",
                start, end, rom_size);
    }
  }

  if (!resolved.has_value()) {
    return false;
  }

  if (dependencies_.window_manager) {
    const size_t session_id = dependencies_.session_id;
    dependencies_.window_manager->OpenWindow(session_id,
                                             "message.message_list");
    dependencies_.window_manager->OpenWindow(session_id,
                                             "message.message_editor");
  }

  if (!resolved->is_expanded) {
    const int idx = resolved->index;
    if (idx < 0 || idx >= vanilla_count) {
      return false;
    }

    const auto& message = list_of_texts_[idx];
    current_message_ = message;
    current_message_index_ = message.ID;
    current_message_is_expanded_ = false;

    const int parsed_idx = resolved->display_id;
    if (parsed_idx >= 0 &&
        parsed_idx < static_cast<int>(parsed_messages_.size())) {
      message_text_box_.text = parsed_messages_[parsed_idx];
    } else {
      message_text_box_.text.clear();
    }

    current_parse_errors_.clear();
    current_parse_warnings_.clear();
    DrawMessagePreview();
    return true;
  }

  // Expanded message.
  const int idx = resolved->index;
  if (idx < 0 || idx >= expanded_count) {
    return false;
  }

  const auto& message = expanded_messages_[idx];
  current_message_ = message;
  current_message_index_ = message.ID;
  current_message_is_expanded_ = true;

  const int parsed_idx = resolved->display_id;
  if (parsed_idx >= 0 &&
      parsed_idx < static_cast<int>(parsed_messages_.size())) {
    message_text_box_.text = parsed_messages_[parsed_idx];
  } else {
    message_text_box_.text.clear();
  }

  current_parse_errors_.clear();
  current_parse_warnings_.clear();
  DrawMessagePreview();
  return true;
}

int MessageEditor::ResolveExpandedMessageBaseId() const {
  int base_id = static_cast<int>(list_of_texts_.size());
  if (dependencies_.project && dependencies_.project->hack_manifest.loaded()) {
    const auto& layout = dependencies_.project->hack_manifest.message_layout();
    if (layout.first_expanded_id != 0) {
      base_id = static_cast<int>(layout.first_expanded_id);
    }
  }

  // Never allow the expanded base to precede the vanilla message count; this
  // prevents truncating/overlapping IDs when the manifest is missing/mistyped.
  base_id = std::max(base_id, static_cast<int>(list_of_texts_.size()));
  return base_id;
}

void MessageEditor::ResolveFontPalette() {
  if (game_data() && !game_data()->palette_groups.hud.empty()) {
    font_preview_colors_ = game_data()->palette_groups.hud.palette(0);
  }

  if (font_preview_colors_.empty()) {
    font_preview_colors_ = BuildFallbackFontPalette();
  }
}

gfx::SnesPalette MessageEditor::BuildFallbackFontPalette() const {
  std::vector<gfx::SnesColor> colors;
  colors.reserve(16);
  for (int i = 0; i < 16; ++i) {
    const float value = static_cast<float>(i) / 15.0f;
    colors.emplace_back(ImVec4(value, value, value, 1.0f));
  }

  if (!colors.empty()) {
    colors[0].set_transparent(true);
  }

  return gfx::SnesPalette(colors);
}

void MessageEditor::LoadFontGraphics() {
  ResolveFontPalette();
  if (!rom() || !rom()->is_loaded()) {
    LOG_WARN("MessageEditor", "ROM not loaded - skipping font graphics load");
    return;
  }

  std::fill(raw_font_gfx_data_.begin(), raw_font_gfx_data_.end(), 0);

  const size_t rom_size = rom()->size();
  if (rom_size > static_cast<size_t>(kGfxFont)) {
    const size_t available = std::min(raw_font_gfx_data_.size(),
                                      rom_size - static_cast<size_t>(kGfxFont));
    std::copy_n(rom()->data() + kGfxFont, available,
                raw_font_gfx_data_.begin());
    if (available < raw_font_gfx_data_.size()) {
      LOG_WARN("MessageEditor",
               "Font graphics truncated (ROM size %zu, read %zu bytes)",
               rom_size, available);
    }
  } else {
    LOG_WARN("MessageEditor",
             "ROM size %zu too small for font graphics offset 0x%X", rom_size,
             kGfxFont);
  }

  message_preview_.font_gfx16_data_ =
      gfx::SnesTo8bppSheet(raw_font_gfx_data_, /*bpp=*/2, /*num_sheets=*/2);

  auto load_font = zelda3::LoadFontGraphics(*rom());
  if (load_font.ok()) {
    message_preview_.font_gfx16_data_2_ = load_font.value().vector();
  } else {
    const std::string error_message(load_font.status().message());
    LOG_WARN("MessageEditor", "LoadFontGraphics failed: %s",
             error_message.c_str());
  }

  const auto& font_data = !message_preview_.font_gfx16_data_.empty()
                              ? message_preview_.font_gfx16_data_
                              : message_preview_.font_gfx16_data_2_;
  RefreshFontAtlasBitmap(font_data);
  font_graphics_loaded_ = true;
}

void MessageEditor::RefreshFontAtlasBitmap(
    const std::vector<uint8_t>& font_data) {
  if (font_data.empty()) {
    LOG_WARN("MessageEditor", "Font graphics data missing - atlas stays empty");
    return;
  }

  const int atlas_width = kFontGfxMessageSize;
  const size_t row_count = (font_data.size() + atlas_width - 1) / atlas_width;
  const int atlas_height = static_cast<int>(std::max<size_t>(1, row_count));

  const size_t expected_size = static_cast<size_t>(atlas_width) * atlas_height;
  std::vector<uint8_t> padded(font_data.begin(), font_data.end());
  if (padded.size() < expected_size) {
    padded.resize(expected_size, 0);
  } else if (padded.size() > expected_size) {
    padded.resize(expected_size);
  }

  font_gfx_bitmap_.Create(atlas_width, atlas_height, kFontGfxMessageDepth,
                          padded);
  if (!font_preview_colors_.empty()) {
    font_gfx_bitmap_.SetPalette(font_preview_colors_);
  }
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &font_gfx_bitmap_);
}

absl::Status MessageEditor::Load() {
  gfx::ScopedTimer timer("MessageEditor::Load");
  return absl::OkStatus();
}

absl::Status MessageEditor::Update() {
  // Panel drawing is handled centrally by WorkspaceWindowManager::DrawAllVisiblePanels()
  // via the WindowContent implementations registered in Initialize().
  // No local drawing needed here.
  return absl::OkStatus();
}

void MessageEditor::SetGameData(zelda3::GameData* game_data) {
  Editor::SetGameData(game_data);
  ApplyFontPalette();
}

void MessageEditor::ApplyFontPalette() {
  ResolveFontPalette();

  if (font_preview_colors_.empty()) {
    return;
  }

  auto queue_refresh = [](gfx::Bitmap& bitmap) {
    if (!bitmap.is_active()) {
      return;
    }
    const auto command = bitmap.texture()
                             ? gfx::Arena::TextureCommandType::UPDATE
                             : gfx::Arena::TextureCommandType::CREATE;
    gfx::Arena::Get().QueueTextureCommand(command, &bitmap);
  };

  font_gfx_bitmap_.SetPalette(font_preview_colors_);
  queue_refresh(font_gfx_bitmap_);

  if (current_font_gfx16_bitmap_.is_active()) {
    current_font_gfx16_bitmap_.SetPalette(font_preview_colors_);
    queue_refresh(current_font_gfx16_bitmap_);
  }
}

void MessageEditor::EnsureFontTexturesReady() {
  if (!font_graphics_loaded_) {
    LoadFontGraphics();
  }
  if (font_gfx_bitmap_.is_active() && !font_gfx_bitmap_.texture()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &font_gfx_bitmap_);
  }
  if (!current_font_gfx16_bitmap_.is_active() &&
      !current_message_.Data.empty()) {
    DrawMessagePreview();
  }
  if (current_font_gfx16_bitmap_.is_active() &&
      !current_font_gfx16_bitmap_.texture()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &current_font_gfx16_bitmap_);
  }
}

void MessageEditor::DrawMessageList() {
  gui::BeginNoPadding();
  if (BeginChild("##MessagesList", ImVec2(0, 0), true,
                 ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    gui::EndNoPadding();
    if (ImGui::Button(tr("Import Bundle"))) {
      std::string path = util::FileDialogWrapper::ShowOpenFileDialog();
      if (!path.empty()) {
        ImportMessageBundleFromFile(path);
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Export Bundle"))) {
      std::string path = util::FileDialogWrapper::ShowSaveFileDialog();
      if (!path.empty()) {
        auto status =
            ExportMessageBundleToJson(path, list_of_texts_, expanded_messages_);
        if (!status.ok()) {
          message_bundle_status_ =
              absl::StrFormat("Export failed: %s", status.message());
          message_bundle_status_error_ = true;
        } else {
          message_bundle_status_ = absl::StrFormat("Exported bundle: %s", path);
          message_bundle_status_error_ = false;
        }
      }
    }
    if (!message_bundle_status_.empty()) {
      ImVec4 color = message_bundle_status_error_ ? gui::GetErrorColor()
                                                  : gui::GetSuccessColor();
      ImGui::TextColored(color, "%s", message_bundle_status_.c_str());
    }
    ImGui::Separator();
    if (BeginTable("##MessagesTable", 4, kMessageTableFlags)) {
      TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 50);
      TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80);
      TableSetupColumn("Contents", ImGuiTableColumnFlags_WidthStretch);
      TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 100);

      TableHeadersRow();

      // Calculate total rows for clipper
      const int vanilla_count = static_cast<int>(list_of_texts_.size());
      const int expanded_count = static_cast<int>(expanded_messages_.size());
      const int total_rows = vanilla_count + expanded_count;

      // Use ImGuiListClipper for virtualized rendering
      ImGuiListClipper clipper;
      clipper.Begin(total_rows);

      while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
          if (row < vanilla_count) {
            // Vanilla message
            const auto& message = list_of_texts_[row];
            TableNextColumn();
            PushID(message.ID);
            if (Button(util::HexWord(message.ID).c_str())) {
              if (current_parse_errors_.empty()) {
                FinalizePendingUndo();
                OpenMessageById(message.ID);
              }
            }
            PopID();

            TableNextColumn();
            ImGui::TextColored(gui::GetInfoColor(), tr("Vanilla"));

            TableNextColumn();
            TextWrapped("%s", parsed_messages_[message.ID].c_str());

            TableNextColumn();
            TextWrapped("%s", util::HexLong(message.Address).c_str());
          } else {
            // Expanded message
            int expanded_idx = row - vanilla_count;
            const auto& expanded_message = expanded_messages_[expanded_idx];
            const int display_id =
                expanded_message_base_id_ + expanded_message.ID;
            const char* display_text = "Missing text";
            if (display_id >= 0 &&
                display_id < static_cast<int>(parsed_messages_.size())) {
              display_text = parsed_messages_[display_id].c_str();
            }
            TableNextColumn();
            PushID(display_id);
            if (Button(util::HexWord(display_id).c_str())) {
              if (current_parse_errors_.empty()) {
                FinalizePendingUndo();
                OpenMessageById(display_id);
              }
            }
            PopID();

            TableNextColumn();
            ImGui::TextColored(gui::GetWarningColor(), tr("Expanded"));

            TableNextColumn();
            TextWrapped("%s", display_text);

            TableNextColumn();
            TextWrapped("%s", util::HexLong(expanded_message.Address).c_str());
          }
        }
      }

      EndTable();
    }
  }
  EndChild();
}

void MessageEditor::DrawCurrentMessage() {
  Button(absl::StrCat("Message ", current_message_.ID).c_str());
  if (InputTextMultiline("##MessageEditor", &message_text_box_.text,
                         ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
    UpdateCurrentMessageFromText(message_text_box_.text);
  }
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    FinalizePendingUndo();
  }
  if (!current_parse_errors_.empty()) {
    ImGui::TextColored(gui::GetErrorColor(), tr("Message parse errors"));
    for (const auto& error : current_parse_errors_) {
      ImGui::BulletText("%s", error.c_str());
    }
  }
  if (!current_parse_warnings_.empty()) {
    ImGui::TextColored(gui::GetWarningColor(), tr("Message parse warnings"));
    for (const auto& warning : current_parse_warnings_) {
      ImGui::BulletText("%s", warning.c_str());
    }
  }
  auto line_warnings = ValidateMessageLineWidths(message_text_box_.text);
  if (!line_warnings.empty()) {
    ImGui::TextColored(gui::GetWarningColor(), tr("Line width warnings"));
    for (const auto& warning : line_warnings) {
      ImGui::BulletText("%s", warning.c_str());
    }
  }
  Separator();
  gui::MemoryEditorPopup("Message Data", current_message_.Data);

  ImGui::BeginChild("##MessagePreview", ImVec2(0, 0), true);
  EnsureFontTexturesReady();
  Text(tr("Message Preview"));
  if (Button(tr("View Palette"))) {
    ImGui::OpenPopup("Palette");
  }
  if (ImGui::BeginPopup("Palette")) {
    gui::DisplayPalette(font_preview_colors_, true);
    ImGui::EndPopup();
  }
  gui::BeginPadding(1);
  BeginChild("CurrentGfxFont", ImVec2(348, 0), true,
             ImGuiWindowFlags_NoScrollWithMouse);
  current_font_gfx16_canvas_.GetConfig().role =
      gui::CanvasRole::kSelectionSource;
  current_font_gfx16_canvas_.DrawBackground();
  gui::EndPadding();
  current_font_gfx16_canvas_.DrawContextMenu();

  // Handle mouse wheel scrolling
  if (ImGui::IsWindowHovered()) {
    float wheel = ImGui::GetIO().MouseWheel;
    if (wheel > 0 && message_preview_.shown_lines > 0) {
      message_preview_.shown_lines--;
    } else if (wheel < 0 &&
               message_preview_.shown_lines < message_preview_.text_line - 2) {
      message_preview_.shown_lines++;
    }
  }

  // Draw only the visible portion of the text
  const ImVec2 preview_canvas_size = current_font_gfx16_canvas_.canvas_size();
  const float dest_width = std::max(0.0f, preview_canvas_size.x - 8.0f);
  const float dest_height = std::max(0.0f, preview_canvas_size.y - 8.0f);
  float src_height = 0.0f;
  if (dest_width > 0.0f && dest_height > 0.0f) {
    const float src_width =
        std::min(dest_width * 0.5f, static_cast<float>(kCurrentMessageWidth));
    src_height =
        std::min(dest_height * 0.5f, static_cast<float>(kCurrentMessageHeight));
    current_font_gfx16_canvas_.DrawBitmap(
        current_font_gfx16_bitmap_, ImVec2(0, 0),      // Destination position
        ImVec2(dest_width, dest_height),               // Destination size
        ImVec2(0, message_preview_.shown_lines * 16),  // Source position
        ImVec2(src_width, src_height)                  // Source size
    );
  }

  // Draw scroll break separator lines on the preview canvas
  {
    ImDrawList* overlay_draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_p0 = current_font_gfx16_canvas_.zero_point();
    ImVec2 canvas_sz = current_font_gfx16_canvas_.canvas_size();
    float line_height = 16.0f;
    // The bitmap is drawn scaled: dest occupies full canvas, so compute the
    // vertical scale factor from destination height to source height.
    float scale_y = 1.0f;
    if (dest_height > 0.0f && src_height > 0.0f) {
      scale_y = dest_height / src_height;
    }
    for (int marker_line : message_preview_.scroll_marker_lines) {
      float src_y = (marker_line - message_preview_.shown_lines) * line_height;
      float y = canvas_p0.y + src_y * scale_y;
      if (y >= canvas_p0.y && y <= canvas_p0.y + canvas_sz.y) {
        overlay_draw_list->AddLine(ImVec2(canvas_p0.x, y),
                                   ImVec2(canvas_p0.x + canvas_sz.x, y),
                                   IM_COL32(100, 180, 255, 180), 1.5f);
        overlay_draw_list->AddText(ImVec2(canvas_p0.x + canvas_sz.x + 4, y - 6),
                                   IM_COL32(100, 180, 255, 200), "[V]");
      }
    }
  }

  current_font_gfx16_canvas_.DrawGrid();
  current_font_gfx16_canvas_.DrawOverlay();
  EndChild();

  // Message Structure info panel
  if (ImGui::CollapsingHeader(tr("Message Structure"),
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text(tr("Lines: %d"), message_preview_.text_line + 1);

    int scroll_count = 0;
    int current_line_chars = 0;
    int line_num = 0;

    for (size_t i = 0; i < current_message_.Data.size(); i++) {
      uint8_t byte = current_message_.Data[i];
      if (byte == kScrollVertical) {
        scroll_count++;
        ImGui::TextColored(gui::GetInfoColor(),
                           tr("  [V] Scroll at byte %zu (line %d, %d chars)"),
                           i, line_num, current_line_chars);
        current_line_chars = 0;
        line_num++;
      } else if (byte == kLine1) {
        ImGui::TextColored(ImVec4(0.7f, 0.85f, 0.5f, 1.0f),
                           tr("  [1] Line 1 at byte %zu"), i);
        current_line_chars = 0;
        line_num = 0;
      } else if (byte == kLine2) {
        ImGui::TextColored(ImVec4(0.7f, 0.85f, 0.5f, 1.0f),
                           tr("  [2] Line 2 at byte %zu"), i);
        current_line_chars = 0;
        line_num = 1;
      } else if (byte == kLine3) {
        ImGui::TextColored(ImVec4(0.7f, 0.85f, 0.5f, 1.0f),
                           tr("  [3] Line 3 at byte %zu"), i);
        current_line_chars = 0;
        line_num = 2;
      } else if (byte < 100) {
        current_line_chars++;
      }
    }

    if (scroll_count == 0) {
      ImGui::TextDisabled(tr("No scroll breaks in this message"));
    } else {
      ImGui::Text(tr("Total scroll breaks: %d"), scroll_count);
    }

    // Character width budget
    ImGui::Separator();
    ImGui::TextDisabled(tr("Line width budget (max ~170px):"));
    int estimated_line_width = current_line_chars * 8;
    float width_ratio = static_cast<float>(estimated_line_width) / 170.0f;
    ImVec4 width_color = (width_ratio > 1.0f)    ? gui::GetErrorColor()
                         : (width_ratio > 0.85f) ? gui::GetWarningColor()
                                                 : gui::GetSuccessColor();
    ImGui::TextColored(width_color, tr("Last line: ~%dpx / 170px (%d chars)"),
                       estimated_line_width, current_line_chars);
  }

  ImGui::EndChild();
}

void MessageEditor::DrawFontAtlas() {
  EnsureFontTexturesReady();
  font_gfx_canvas_.GetConfig().role = gui::CanvasRole::kSelectionSource;
  gui::BeginCanvas(font_gfx_canvas_, ImVec2(256, 256));
  font_gfx_canvas_.DrawBitmap(font_gfx_bitmap_, 0, 0, 2.0f);
  font_gfx_canvas_.DrawTileSelector(16, 32);
  gui::EndCanvas(font_gfx_canvas_);
}

void MessageEditor::DrawExpandedMessageSettings() {
  ImGui::BeginChild("##ExpandedMessageSettings", ImVec2(0, 130), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  ImGui::Text(tr("Expanded Messages"));

  if (ImGui::Button(tr("Load from ROM"))) {
    auto status = LoadExpandedMessagesFromRom();
    if (!status.ok()) {
      LOG_WARN("MessageEditor", "Load from ROM: %s",
               std::string(status.message()).c_str());
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(tr("Load from File"))) {
    std::string path = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!path.empty()) {
      expanded_message_path_ = path;
      expanded_message_base_id_ = ResolveExpandedMessageBaseId();
      parsed_messages_.resize(expanded_message_base_id_);
      expanded_messages_.clear();
      std::vector<std::string> parsed_expanded;
      auto status = LoadExpandedMessages(expanded_message_path_,
                                         parsed_expanded, expanded_messages_,
                                         message_preview_.all_dictionaries_);
      if (!status.ok()) {
        if (auto* popup_manager = dependencies_.popup_manager) {
          popup_manager->Show("Error");
        }
      } else {
        parsed_messages_.insert(parsed_messages_.end(), parsed_expanded.begin(),
                                parsed_expanded.end());
        MarkDomainDirty(SaveDomain::kExpandedMessages);
      }
    }
  }

  if (expanded_messages_.size() > 0) {
    ImGui::Text(tr("Source: %s"), expanded_message_path_.c_str());
    ImGui::Text(tr("Messages: %lu"), expanded_messages_.size());

    // Capacity indicator
    int capacity = GetExpandedTextDataEnd() - GetExpandedTextDataStart() + 1;
    int used = CalculateExpandedBankUsage();
    int remaining = capacity - used;
    float usage_ratio = static_cast<float>(used) / static_cast<float>(capacity);

    ImVec4 capacity_color;
    if (usage_ratio < 0.75f) {
      capacity_color = gui::GetSuccessColor();
    } else if (usage_ratio < 0.90f) {
      capacity_color = gui::GetWarningColor();
    } else {
      capacity_color = gui::GetErrorColor();
    }
    ImGui::TextColored(capacity_color, tr("Bank: %d / %d bytes (%d free)"),
                       used, capacity, remaining);

    if (ImGui::Button(tr("Add New Message"))) {
      MessageData new_message;
      new_message.ID = expanded_messages_.back().ID + 1;
      new_message.Address = expanded_messages_.back().Address +
                            expanded_messages_.back().Data.size();
      expanded_messages_.push_back(new_message);
      MarkDomainDirty(SaveDomain::kExpandedMessages);
      const int display_id = expanded_message_base_id_ + new_message.ID;
      if (display_id >= 0 &&
          static_cast<size_t>(display_id) >= parsed_messages_.size()) {
        parsed_messages_.resize(display_id + 1);
      }
    }

    ImGui::SameLine();
    if (ImGui::Button(tr("Export to JSON"))) {
      std::string path = util::FileDialogWrapper::ShowSaveFileDialog();
      if (!path.empty()) {
        PRINT_IF_ERROR(ExportMessagesToJson(path, expanded_messages_));
      }
    }
  }

  EndChild();
}

void MessageEditor::DrawTextCommands() {
  ImGui::BeginChild("##TextCommands",
                    ImVec2(0, ImGui::GetContentRegionAvail().y / 2), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  static uint8_t command_parameter = 0;
  gui::InputHexByte("Command Parameter", &command_parameter);
  for (const auto& text_element : TextCommands) {
    if (Button(text_element.GenericToken.c_str())) {
      message_text_box_.text.append(
          text_element.GetParamToken(command_parameter));
      UpdateCurrentMessageFromText(message_text_box_.text);
    }
    SameLine();
    TextWrapped("%s", text_element.Description.c_str());
    Separator();
  }
  EndChild();
}

void MessageEditor::DrawSpecialCharacters() {
  ImGui::BeginChild("##SpecialChars",
                    ImVec2(0, ImGui::GetContentRegionAvail().y / 2), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  for (const auto& text_element : SpecialChars) {
    if (Button(text_element.GenericToken.c_str())) {
      message_text_box_.text.append(text_element.GenericToken);
      UpdateCurrentMessageFromText(message_text_box_.text);
    }
    SameLine();
    TextWrapped("%s", text_element.Description.c_str());
    Separator();
  }
  EndChild();
}

void MessageEditor::DrawDictionary() {
  if (ImGui::BeginChild("##DictionaryChild",
                        ImVec2(0, ImGui::GetContentRegionAvail().y), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    if (BeginTable("##Dictionary", 2, kMessageTableFlags)) {
      TableSetupColumn("ID");
      TableSetupColumn("Contents");
      TableHeadersRow();

      // Use ImGuiListClipper for virtualized rendering
      const int dict_count =
          static_cast<int>(message_preview_.all_dictionaries_.size());
      ImGuiListClipper clipper;
      clipper.Begin(dict_count);

      while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
          const auto& dictionary = message_preview_.all_dictionaries_[row];
          TableNextColumn();
          Text("%s", util::HexWord(dictionary.ID).c_str());
          TableNextColumn();
          Text("%s", dictionary.Contents.c_str());
        }
      }

      EndTable();
    }
  }
  EndChild();
}

void MessageEditor::UpdateCurrentMessageFromText(const std::string& text) {
  PushUndoSnapshot();

  MarkDomainDirty(current_message_is_expanded_ ? SaveDomain::kExpandedMessages
                                               : SaveDomain::kVanillaMessages);

  auto parse_result = ParseMessageToDataWithDiagnostics(text);
  current_parse_errors_ = parse_result.errors;
  current_parse_warnings_ = parse_result.warnings;
  if (rom_) {
    // Invalid intermediate input is still unsaved work. Save() will fail
    // closed until the diagnostics are resolved rather than silently dropping
    // unsupported characters or unknown tokens.
    rom_->set_dirty(true);
  }
  if (!parse_result.ok()) {
    return;
  }

  std::string raw_text = text;
  raw_text.erase(std::remove(raw_text.begin(), raw_text.end(), '\n'),
                 raw_text.end());

  current_message_.RawString = raw_text;
  current_message_.ContentsParsed = text;
  current_message_.Data = std::move(parse_result.bytes);

  int parsed_index = current_message_index_;
  if (current_message_is_expanded_) {
    parsed_index = expanded_message_base_id_ + current_message_index_;
  }

  if (parsed_index >= 0) {
    if (static_cast<size_t>(parsed_index) >= parsed_messages_.size()) {
      parsed_messages_.resize(parsed_index + 1);
    }
    parsed_messages_[parsed_index] = text;
  }

  if (current_message_is_expanded_) {
    if (current_message_index_ >= 0 &&
        current_message_index_ < static_cast<int>(expanded_messages_.size())) {
      expanded_messages_[current_message_index_] = current_message_;
    }
  } else {
    if (current_message_index_ >= 0 &&
        current_message_index_ < static_cast<int>(list_of_texts_.size())) {
      list_of_texts_[current_message_index_] = current_message_;
    }
  }

  DrawMessagePreview();
}

void MessageEditor::ImportMessageBundleFromFile(const std::string& path) {
  message_bundle_status_.clear();
  message_bundle_status_error_ = false;

  auto entries_or = LoadMessageBundleFromJson(path);
  if (!entries_or.ok()) {
    message_bundle_status_ =
        absl::StrFormat("Import failed: %s", entries_or.status().message());
    message_bundle_status_error_ = true;
    return;
  }

  int applied = 0;
  int errors = 0;
  int warnings = 0;
  int duplicate_errors = 0;
  int parse_error_entries = 0;
  int vanilla_updated = 0;
  int expanded_updated = 0;
  int expanded_created = 0;
  bool expanded_modified = false;
  std::vector<std::string> issue_samples;

  auto add_issue_sample = [&issue_samples](const std::string& issue) {
    constexpr size_t kMaxIssueSamples = 4;
    if (issue_samples.size() < kMaxIssueSamples) {
      issue_samples.push_back(issue);
    }
  };

  auto make_entry_key = [](const MessageBundleEntry& entry) {
    return absl::StrFormat("%s:%d", MessageBankToString(entry.bank), entry.id);
  };

  std::unordered_map<std::string, int> seen_entries;

  auto entries = entries_or.value();
  for (const auto& entry : entries) {
    const std::string entry_key = make_entry_key(entry);
    if (seen_entries.find(entry_key) != seen_entries.end()) {
      errors++;
      duplicate_errors++;
      add_issue_sample(absl::StrFormat("Duplicate entry for %s", entry_key));
      continue;
    }
    seen_entries.emplace(entry_key, 1);

    auto parse_result = ParseMessageToDataWithDiagnostics(entry.text);
    auto line_warnings = ValidateMessageLineWidths(entry.text);
    warnings += static_cast<int>(parse_result.warnings.size());
    warnings += static_cast<int>(line_warnings.size());

    if (!parse_result.ok()) {
      errors++;
      parse_error_entries++;
      if (!parse_result.errors.empty()) {
        add_issue_sample(absl::StrFormat("Parse error for %s: %s", entry_key,
                                         parse_result.errors.front()));
      } else {
        add_issue_sample(absl::StrFormat("Parse error for %s", entry_key));
      }
      continue;
    }

    if (entry.bank == MessageBank::kVanilla) {
      if (entry.id < 0 || entry.id >= static_cast<int>(list_of_texts_.size())) {
        errors++;
        add_issue_sample(
            absl::StrFormat("Vanilla ID out of range: %d", entry.id));
        continue;
      }
      auto& message = list_of_texts_[entry.id];
      message.RawString = entry.text;
      message.ContentsParsed = entry.text;
      message.Data = parse_result.bytes;
      message.DataParsed = parse_result.bytes;
      if (entry.id >= 0 &&
          entry.id < static_cast<int>(parsed_messages_.size())) {
        parsed_messages_[entry.id] = entry.text;
      }
      vanilla_updated++;
      applied++;
    } else {
      if (entry.id < 0) {
        errors++;
        add_issue_sample(
            absl::StrFormat("Expanded ID out of range: %d", entry.id));
        continue;
      }
      if (entry.id >= static_cast<int>(expanded_messages_.size())) {
        const int old_size = static_cast<int>(expanded_messages_.size());
        const int target_size = entry.id + 1;
        expanded_messages_.resize(target_size);
        for (int i = old_size; i < target_size; ++i) {
          expanded_messages_[i].ID = i;
        }
        expanded_created += target_size - old_size;
      }
      auto& message = expanded_messages_[entry.id];
      message.RawString = entry.text;
      message.ContentsParsed = entry.text;
      message.Data = parse_result.bytes;
      message.DataParsed = parse_result.bytes;
      const int parsed_index = expanded_message_base_id_ + entry.id;
      if (parsed_index >= 0) {
        if (static_cast<size_t>(parsed_index) >= parsed_messages_.size()) {
          parsed_messages_.resize(parsed_index + 1);
        }
        parsed_messages_[parsed_index] = entry.text;
      }
      expanded_modified = true;
      expanded_updated++;
      applied++;
    }
  }

  if (expanded_modified) {
    int pos = GetExpandedTextDataStart();
    for (auto& message : expanded_messages_) {
      message.Address = pos;
      pos += static_cast<int>(message.Data.size()) + 1;
    }
  }

  int current_display_id = current_message_index_;
  if (current_message_is_expanded_) {
    current_display_id = expanded_message_base_id_ + current_message_index_;
  }
  if (current_display_id >= 0) {
    OpenMessageById(current_display_id);
  }

  if (errors > 0) {
    message_bundle_status_ = absl::StrFormat(
        "Import finished with %d errors (%d applied: vanilla %d updated, "
        "expanded %d updated/%d created; %d warnings, %d duplicates, %d "
        "parse failures).",
        errors, applied, vanilla_updated, expanded_updated, expanded_created,
        warnings, duplicate_errors, parse_error_entries);
    if (!issue_samples.empty()) {
      message_bundle_status_ = absl::StrFormat(
          "%s Example: %s", message_bundle_status_, issue_samples.front());
    }
    message_bundle_status_error_ = true;
  } else {
    message_bundle_status_ = absl::StrFormat(
        "Imported %d messages (vanilla %d updated, expanded %d updated/%d "
        "created, %d warnings).",
        applied, vanilla_updated, expanded_updated, expanded_created, warnings);
  }
  if (applied > 0 && rom_) {
    rom_->set_dirty(true);
  }
  if (vanilla_updated > 0) {
    MarkDomainDirty(SaveDomain::kVanillaMessages);
  }
  if (expanded_modified) {
    MarkDomainDirty(SaveDomain::kExpandedMessages);
  }
}

void MessageEditor::DrawMessagePreview() {
  // Render the message to the preview bitmap
  message_preview_.DrawMessagePreview(current_message_);

  // Validate preview data before updating
  if (message_preview_.current_preview_data_.empty()) {
    LOG_WARN("MessageEditor", "Preview data is empty, skipping bitmap update");
    return;
  }

  if (current_font_gfx16_bitmap_.is_active()) {
    // CRITICAL: Use set_data() to properly update both data_ AND surface_
    // mutable_data() returns a reference but doesn't update the surface!
    current_font_gfx16_bitmap_.set_data(message_preview_.current_preview_data_);

    if (current_font_gfx16_bitmap_.palette().empty() &&
        !font_preview_colors_.empty()) {
      current_font_gfx16_bitmap_.SetPalette(font_preview_colors_);
    }

    // Validate surface was updated
    if (!current_font_gfx16_bitmap_.surface()) {
      LOG_ERROR("MessageEditor", "Bitmap surface is null after set_data()");
      return;
    }

    // Queue texture update (or create if missing) so changes are visible
    const auto command = current_font_gfx16_bitmap_.texture()
                             ? gfx::Arena::TextureCommandType::UPDATE
                             : gfx::Arena::TextureCommandType::CREATE;
    gfx::Arena::Get().QueueTextureCommand(command, &current_font_gfx16_bitmap_);

    LOG_DEBUG(
        "MessageEditor",
        "Updated message preview bitmap (size: %zu) and queued texture update",
        message_preview_.current_preview_data_.size());
  } else {
    // Create bitmap and queue texture creation with 8-bit indexed depth
    current_font_gfx16_bitmap_.Create(kCurrentMessageWidth,
                                      kCurrentMessageHeight, 8,
                                      message_preview_.current_preview_data_);
    current_font_gfx16_bitmap_.SetPalette(font_preview_colors_);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &current_font_gfx16_bitmap_);

    LOG_INFO("MessageEditor",
             "Created message preview bitmap (%dx%d) with 8-bit depth and "
             "queued texture creation",
             kCurrentMessageWidth, kCurrentMessageHeight);
  }
}

absl::Status MessageEditor::Save() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (!current_parse_errors_.empty()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Current message has parse errors: %s", current_parse_errors_.front()));
  }

  return SaveDirtyDomains(/*include_font_widths=*/true,
                          /*include_vanilla_messages=*/true,
                          /*include_expanded_messages=*/true);
}

absl::StatusOr<MessageEditor::SavePlan> MessageEditor::BuildSavePlan(
    bool include_font_widths, bool include_vanilla_messages,
    bool include_expanded_messages) const {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  SavePlan plan;

  if (include_font_widths && dirty_state_.font_widths) {
    plan.saves_font_widths = true;
    plan.writes.push_back(
        {SaveDomain::kFontWidths, kCharactersWidth,
         std::vector<uint8_t>(message_preview_.width_array.begin(),
                              message_preview_.width_array.end())});
  }

  if (include_vanilla_messages && dirty_state_.vanilla_messages) {
    std::vector<uint8_t> primary;
    std::vector<uint8_t> secondary;
    constexpr size_t kPrimaryCapacity = kTextDataEnd - kTextData + 1;
    constexpr size_t kSecondaryCapacity = kTextData2End - kTextData2 + 1;
    primary.reserve(kPrimaryCapacity);
    secondary.reserve(kSecondaryCapacity);

    bool in_second_bank = false;
    auto append_byte = [&](uint8_t value, bool is_bank_switch) -> absl::Status {
      auto& destination = in_second_bank ? secondary : primary;
      const size_t capacity =
          in_second_bank ? kSecondaryCapacity : kPrimaryCapacity;
      if (destination.size() >= capacity) {
        const int overflow_pos = (in_second_bank ? kTextData2 : kTextData) +
                                 static_cast<int>(destination.size());
        return absl::ResourceExhaustedError(
            DisplayTextOverflowError(overflow_pos, !in_second_bank));
      }
      destination.push_back(value);
      if (!in_second_bank && is_bank_switch) {
        in_second_bank = true;
      }
      return absl::OkStatus();
    };

    for (const auto& message : list_of_texts_) {
      bool next_byte_is_command_argument = false;
      for (uint8_t value : message.Data) {
        const bool is_command_argument = next_byte_is_command_argument;
        next_byte_is_command_argument = false;
        RETURN_IF_ERROR(append_byte(
            value, !is_command_argument && value == kBankSwitchCommand));
        if (!is_command_argument) {
          const auto command = FindMatchingCommand(value);
          next_byte_is_command_argument =
              command.has_value() && command->HasArgument;
        }
      }
      RETURN_IF_ERROR(
          append_byte(kMessageTerminator, /*is_bank_switch=*/false));
    }
    RETURN_IF_ERROR(append_byte(0xFF, /*is_bank_switch=*/false));

    plan.saves_vanilla_messages = true;
    if (!primary.empty()) {
      plan.writes.push_back(
          {SaveDomain::kVanillaMessages, kTextData, std::move(primary)});
    }
    if (!secondary.empty()) {
      plan.writes.push_back(
          {SaveDomain::kVanillaMessages, kTextData2, std::move(secondary)});
    }
  }

  if (include_expanded_messages && dirty_state_.expanded_messages &&
      !expanded_messages_.empty()) {
    const int start = GetExpandedTextDataStart();
    const int end = GetExpandedTextDataEnd();
    if (start < 0 || end < start) {
      return absl::InvalidArgumentError("Invalid expanded message region");
    }
    if (static_cast<uint64_t>(start) >= rom_->size() ||
        static_cast<uint64_t>(end) >= rom_->size()) {
      return absl::OutOfRangeError(
          "Expanded message region is outside the ROM");
    }

    const int64_t capacity = static_cast<int64_t>(end) - start + 1;
    std::vector<uint8_t> bytes;
    bytes.reserve(static_cast<size_t>(capacity));
    plan.expanded_message_addresses.reserve(expanded_messages_.size());

    for (size_t index = 0; index < expanded_messages_.size(); ++index) {
      const auto& message = expanded_messages_[index];
      auto parsed = ParseMessageToDataWithDiagnostics(message.RawString);
      if (!parsed.ok()) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Expanded message %d is invalid: %s",
                            static_cast<int>(index), parsed.errors.front()));
      }
      if (message.RawString.find("[BANK]") != std::string::npos) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Expanded message %d contains [BANK], which is only valid in the "
            "vanilla message stream",
            static_cast<int>(index)));
      }

      const int64_t needed = static_cast<int64_t>(parsed.bytes.size()) + 1;
      if (static_cast<int64_t>(bytes.size()) + needed + 1 > capacity) {
        return absl::ResourceExhaustedError(absl::StrFormat(
            "Expanded message data exceeds bank boundary "
            "(at message %d, used=%d, needed=%d, capacity=%d, end=0x%06X)",
            static_cast<int>(index), static_cast<int>(bytes.size()),
            static_cast<int>(needed), static_cast<int>(capacity), end));
      }

      plan.expanded_message_addresses.push_back(start +
                                                static_cast<int>(bytes.size()));
      bytes.insert(bytes.end(), parsed.bytes.begin(), parsed.bytes.end());
      bytes.push_back(kMessageTerminator);
    }
    bytes.push_back(0xFF);

    plan.saves_expanded_messages = true;
    plan.writes.push_back({SaveDomain::kExpandedMessages,
                           static_cast<uint32_t>(start), std::move(bytes)});
  }

  for (const auto& write : plan.writes) {
    const uint64_t end =
        static_cast<uint64_t>(write.start) + write.bytes.size();
    if (end > rom_->size() || end > std::numeric_limits<uint32_t>::max()) {
      return absl::OutOfRangeError(absl::StrFormat(
          "Message save range [0x%06X, 0x%06llX) is outside the ROM",
          write.start, static_cast<unsigned long long>(end)));
    }
  }

  return plan;
}

absl::Status MessageEditor::ValidateSavePlan(const SavePlan& plan) const {
  if (!dependencies_.project ||
      !dependencies_.project->hack_manifest.loaded() || plan.writes.empty()) {
    return absl::OkStatus();
  }

  std::vector<std::pair<uint32_t, uint32_t>> ranges;
  ranges.reserve(plan.writes.size());
  for (const auto& write : plan.writes) {
    ranges.emplace_back(write.start, write.end());
  }

  const auto& yaze_project = *dependencies_.project;
  if (yaze_project.rom_metadata.write_policy ==
          project::RomWritePolicy::kBlock &&
      plan.saves_expanded_messages &&
      absl::StrContains(yaze_project.hack_manifest.hack_name(),
                        "Oracle of Secrets")) {
    return absl::PermissionDeniedError(
        "Oracle of Secrets expanded messages are owned by ASM. No message "
        "bytes were written; keep the draft in Yaze, edit Core/message.asm, "
        "rebuild Oracle of Secrets, and reopen the rebuilt ROM before "
        "retrying.");
  }

  const auto status = ValidateHackManifestSaveConflicts(
      yaze_project.hack_manifest, yaze_project.rom_metadata.write_policy,
      ranges, "message data", "MessageEditor", dependencies_.toast_manager);
  return status;
}

absl::Status MessageEditor::ApplySavePlan(const SavePlan& plan) {
  if (plan.writes.empty()) {
    return absl::OkStatus();
  }

  yaze::rom::WriteFence fence;
  for (const auto& write : plan.writes) {
    const char* label = "MessageData";
    switch (write.domain) {
      case SaveDomain::kFontWidths:
        label = "MessageFontWidths";
        break;
      case SaveDomain::kVanillaMessages:
        label = "VanillaMessageBank";
        break;
      case SaveDomain::kExpandedMessages:
        label = "ExpandedMessageBank";
        break;
    }
    RETURN_IF_ERROR(fence.Allow(write.start, write.end(), label));
  }

  ScopedRomTransaction transaction(*rom_);
  yaze::rom::ScopedWriteFence fence_scope(rom_, &fence);
  for (const auto& write : plan.writes) {
    RETURN_IF_ERROR(
        rom_->WriteVector(static_cast<int>(write.start), write.bytes));
  }
  transaction.Commit();

  if (plan.saves_expanded_messages) {
    for (size_t index = 0; index < expanded_messages_.size() &&
                           index < plan.expanded_message_addresses.size();
         ++index) {
      expanded_messages_[index].Address =
          plan.expanded_message_addresses[index];
    }
    SyncCurrentExpandedMessageAddress();
  }

  return absl::OkStatus();
}

absl::Status MessageEditor::SaveDirtyDomains(bool include_font_widths,
                                             bool include_vanilla_messages,
                                             bool include_expanded_messages) {
  ASSIGN_OR_RETURN(const SavePlan plan,
                   BuildSavePlan(include_font_widths, include_vanilla_messages,
                                 include_expanded_messages));
  RETURN_IF_ERROR(ValidateSavePlan(plan));
  RETURN_IF_ERROR(ApplySavePlan(plan));
  RecordSavedDomains(plan);
  return absl::OkStatus();
}

absl::Status MessageEditor::SaveExpandedMessages() {
  return SaveDirtyDomains(/*include_font_widths=*/false,
                          /*include_vanilla_messages=*/false,
                          /*include_expanded_messages=*/true);
}

absl::Status MessageEditor::BeginSaveTransaction() {
  if (save_transaction_active_) {
    return absl::FailedPreconditionError(
        "Message save transaction is already active");
  }
  transaction_dirty_snapshot_ = dirty_state_;
  transaction_saved_domains_ = {};
  transaction_expanded_address_snapshot_.clear();
  transaction_expanded_address_snapshot_.reserve(expanded_messages_.size());
  for (const auto& message : expanded_messages_) {
    transaction_expanded_address_snapshot_.push_back(message.Address);
  }
  save_transaction_active_ = true;
  return absl::OkStatus();
}

void MessageEditor::RollbackSaveTransaction() {
  if (!save_transaction_active_) {
    return;
  }
  dirty_state_ = transaction_dirty_snapshot_;
  for (size_t index = 0; index < expanded_messages_.size() &&
                         index < transaction_expanded_address_snapshot_.size();
       ++index) {
    expanded_messages_[index].Address =
        transaction_expanded_address_snapshot_[index];
  }
  SyncCurrentExpandedMessageAddress();
  transaction_dirty_snapshot_ = {};
  transaction_saved_domains_ = {};
  transaction_expanded_address_snapshot_.clear();
  save_transaction_active_ = false;
}

void MessageEditor::CommitSaveTransaction() {
  if (!save_transaction_active_) {
    return;
  }
  ClearSavedDomains(transaction_saved_domains_);
  transaction_dirty_snapshot_ = {};
  transaction_saved_domains_ = {};
  transaction_expanded_address_snapshot_.clear();
  save_transaction_active_ = false;
}

void MessageEditor::MarkDomainDirty(SaveDomain domain) {
  bool* dirty = nullptr;
  bool* transaction_saved = nullptr;
  switch (domain) {
    case SaveDomain::kFontWidths:
      dirty = &dirty_state_.font_widths;
      transaction_saved = &transaction_saved_domains_.font_widths;
      break;
    case SaveDomain::kVanillaMessages:
      dirty = &dirty_state_.vanilla_messages;
      transaction_saved = &transaction_saved_domains_.vanilla_messages;
      break;
    case SaveDomain::kExpandedMessages:
      dirty = &dirty_state_.expanded_messages;
      transaction_saved = &transaction_saved_domains_.expanded_messages;
      break;
  }
  *dirty = true;
  if (save_transaction_active_) {
    *transaction_saved = false;
  }
  if (rom_) {
    rom_->set_dirty(true);
  }
}

void MessageEditor::RecordSavedDomains(const SavePlan& plan) {
  DirtyState saved{plan.saves_font_widths, plan.saves_vanilla_messages,
                   plan.saves_expanded_messages};
  if (!save_transaction_active_) {
    ClearSavedDomains(saved);
    return;
  }
  transaction_saved_domains_.font_widths |= saved.font_widths;
  transaction_saved_domains_.vanilla_messages |= saved.vanilla_messages;
  transaction_saved_domains_.expanded_messages |= saved.expanded_messages;
}

void MessageEditor::ClearSavedDomains(const DirtyState& saved) {
  if (saved.font_widths) {
    dirty_state_.font_widths = false;
  }
  if (saved.vanilla_messages) {
    dirty_state_.vanilla_messages = false;
  }
  if (saved.expanded_messages) {
    dirty_state_.expanded_messages = false;
  }
}

void MessageEditor::SyncCurrentExpandedMessageAddress() {
  if (!current_message_is_expanded_ || current_message_index_ < 0 ||
      current_message_index_ >= static_cast<int>(expanded_messages_.size())) {
    return;
  }
  current_message_.Address = expanded_messages_[current_message_index_].Address;
}

absl::Status MessageEditor::LoadExpandedMessagesFromRom() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  expanded_message_base_id_ = ResolveExpandedMessageBaseId();
  parsed_messages_.resize(expanded_message_base_id_);

  expanded_messages_.clear();
  expanded_messages_ = ReadExpandedTextData(
      rom_->mutable_data(), GetExpandedTextDataStart(),
      std::min(GetExpandedTextDataEnd(), static_cast<int>(rom_->size()) - 1));

  if (expanded_messages_.empty()) {
    return absl::NotFoundError(
        "No expanded messages found in ROM at expanded text region");
  }

  // Parse the expanded messages and append to the unified list
  auto parsed_expanded =
      ParseMessageData(expanded_messages_, message_preview_.all_dictionaries_);
  for (const auto& msg : expanded_messages_) {
    if (msg.ID >= 0 && msg.ID < static_cast<int>(parsed_expanded.size())) {
      parsed_messages_.push_back(parsed_expanded[msg.ID]);
    }
  }

  expanded_message_path_ = "(ROM)";
  dirty_state_.expanded_messages = false;
  return absl::OkStatus();
}

int MessageEditor::CalculateExpandedBankUsage() const {
  if (expanded_messages_.empty())
    return 0;
  int total = 0;
  for (const auto& msg : expanded_messages_) {
    total += static_cast<int>(msg.Data.size()) + 1;  // +1 for 0x7F
  }
  total += 1;  // +1 for final 0xFF
  return total;
}

absl::Status MessageEditor::Cut() {
  // Ensure that text is currently selected in the text box.
  if (!message_text_box_.text.empty()) {
    // Cut the selected text in the control and paste it into the Clipboard.
    message_text_box_.Cut();
  }
  return absl::OkStatus();
}

absl::Status MessageEditor::Paste() {
  // Determine if there is any text in the Clipboard to paste into the
  if (ImGui::GetClipboardText() != nullptr) {
    // Paste the text from the Clipboard into the text box.
    message_text_box_.Paste();
  }
  return absl::OkStatus();
}

absl::Status MessageEditor::Copy() {
  // Ensure that text is selected in the text box.
  if (message_text_box_.selection_length > 0) {
    // Copy the selected text to the Clipboard.
    message_text_box_.Copy();
  }
  return absl::OkStatus();
}

void MessageEditor::PushUndoSnapshot() {
  if (pending_undo_before_.has_value()) {
    // If we're still editing the same message, keep the existing "before"
    // snapshot so the entire edit session becomes a single undo step.
    if (pending_undo_before_->message_index == current_message_index_ &&
        pending_undo_before_->is_expanded == current_message_is_expanded_) {
      return;
    }
    FinalizePendingUndo();
  }

  // Capture current state as "before"
  int parsed_index = current_message_index_;
  if (current_message_is_expanded_) {
    parsed_index = expanded_message_base_id_ + current_message_index_;
  }
  std::string text;
  if (parsed_index >= 0 &&
      parsed_index < static_cast<int>(parsed_messages_.size())) {
    text = parsed_messages_[parsed_index];
  }
  pending_undo_before_ =
      MessageSnapshot{current_message_, text, current_message_index_,
                      current_message_is_expanded_};
}

void MessageEditor::FinalizePendingUndo() {
  if (!pending_undo_before_.has_value())
    return;

  // The "after" snapshot must correspond to the same message as the pending
  // "before", even if the user navigated to a different message in the UI.
  const int message_index = pending_undo_before_->message_index;
  const bool is_expanded = pending_undo_before_->is_expanded;

  MessageData after_message;
  if (is_expanded) {
    if (message_index < 0 ||
        message_index >= static_cast<int>(expanded_messages_.size())) {
      pending_undo_before_.reset();
      return;
    }
    after_message = expanded_messages_[message_index];
  } else {
    if (message_index < 0 ||
        message_index >= static_cast<int>(list_of_texts_.size())) {
      pending_undo_before_.reset();
      return;
    }
    after_message = list_of_texts_[message_index];
  }

  int parsed_index = message_index;
  if (is_expanded) {
    parsed_index = expanded_message_base_id_ + message_index;
  }
  std::string text;
  if (parsed_index >= 0 &&
      parsed_index < static_cast<int>(parsed_messages_.size())) {
    text = parsed_messages_[parsed_index];
  }
  MessageSnapshot after{std::move(after_message), std::move(text),
                        message_index, is_expanded};

  undo_manager_.Push(std::make_unique<MessageEditAction>(
      std::move(*pending_undo_before_), std::move(after),
      [this](const MessageSnapshot& s) { ApplySnapshot(s); }));
  pending_undo_before_.reset();
}

void MessageEditor::ApplySnapshot(const MessageSnapshot& snapshot) {
  current_message_ = snapshot.message;
  current_message_index_ = snapshot.message_index;
  current_message_is_expanded_ = snapshot.is_expanded;
  message_text_box_.text = snapshot.parsed_text;
  const auto diagnostics =
      ParseMessageToDataWithDiagnostics(snapshot.message.RawString);
  current_parse_errors_ = diagnostics.errors;
  current_parse_warnings_ = diagnostics.warnings;

  int parsed_index = snapshot.message_index;
  if (snapshot.is_expanded) {
    parsed_index = expanded_message_base_id_ + snapshot.message_index;
  }
  if (parsed_index >= 0 &&
      parsed_index < static_cast<int>(parsed_messages_.size())) {
    parsed_messages_[parsed_index] = snapshot.parsed_text;
  }

  if (snapshot.is_expanded) {
    if (snapshot.message_index >= 0 &&
        snapshot.message_index < static_cast<int>(expanded_messages_.size())) {
      expanded_messages_[snapshot.message_index] = snapshot.message;
    }
  } else {
    if (snapshot.message_index >= 0 &&
        snapshot.message_index < static_cast<int>(list_of_texts_.size())) {
      list_of_texts_[snapshot.message_index] = snapshot.message;
    }
  }

  if (rom_) {
    rom_->set_dirty(true);
  }
  MarkDomainDirty(snapshot.is_expanded ? SaveDomain::kExpandedMessages
                                       : SaveDomain::kVanillaMessages);
  DrawMessagePreview();
}

absl::Status MessageEditor::Undo() {
  FinalizePendingUndo();
  return undo_manager_.Undo();
}

absl::Status MessageEditor::Redo() {
  return undo_manager_.Redo();
}

void MessageEditor::Delete() {
  // Determine if any text is selected in the TextBox control.
  if (message_text_box_.selection_length == 0) {
    // clear all of the text in the textbox.
    message_text_box_.clear();
  }
}

void MessageEditor::SelectAll() {
  // Determine if any text is selected in the TextBox control.
  if (message_text_box_.selection_length == 0) {
    // Select all text in the text box.
    message_text_box_.SelectAll();

    // Move the cursor to the text box.
    message_text_box_.Focus();
  }
}

absl::Status MessageEditor::Find() {
  if (ImGui::Begin("Find & Replace", nullptr,
                   ImGuiWindowFlags_AlwaysAutoResize)) {
    static char find_text[256] = "";
    static char replace_text[256] = "";
    ImGui::InputText(tr("Search"), find_text, IM_ARRAYSIZE(find_text));
    ImGui::InputText(tr("Replace with"), replace_text,
                     IM_ARRAYSIZE(replace_text));

    if (ImGui::Button(tr("Find Next"))) {
      search_text_ = find_text;
      replace_status_.clear();
    }

    ImGui::SameLine();
    if (ImGui::Button(tr("Find All"))) {
      search_text_ = find_text;
      replace_status_.clear();
    }

    ImGui::SameLine();
    if (ImGui::Button(tr("Replace"))) {
      search_text_ = find_text;
      replace_text_ = replace_text;
      int count = ReplaceCurrentMatch();
      if (count > 0) {
        replace_status_ = "Replaced 1 occurrence";
        replace_status_error_ = false;
      } else {
        replace_status_ = "No match found in current message";
        replace_status_error_ = true;
      }
    }

    ImGui::SameLine();
    if (ImGui::Button(tr("Replace All"))) {
      search_text_ = find_text;
      replace_text_ = replace_text;
      int count = ReplaceAllMatches();
      if (count >= 0) {
        replace_status_ = absl::StrFormat("Replaced %d occurrence%s", count,
                                          count == 1 ? "" : "s");
        replace_status_error_ = (count == 0);
      }
    }

    ImGui::Checkbox(tr("Case Sensitive"), &case_sensitive_);
    ImGui::SameLine();
    ImGui::Checkbox(tr("Match Whole Word"), &match_whole_word_);

    if (!replace_status_.empty()) {
      ImVec4 color =
          replace_status_error_ ? gui::GetErrorColor() : gui::GetSuccessColor();
      ImGui::TextColored(color, "%s", replace_status_.c_str());
    }
  }
  ImGui::End();

  return absl::OkStatus();
}

int MessageEditor::ReplaceCurrentMatch() {
  if (search_text_.empty())
    return 0;

  std::string& text = message_text_box_.text;
  std::string search = search_text_;
  std::string source = text;

  if (!case_sensitive_) {
    std::transform(search.begin(), search.end(), search.begin(), ::tolower);
    std::transform(source.begin(), source.end(), source.begin(), ::tolower);
  }

  size_t pos = source.find(search);
  if (pos == std::string::npos)
    return 0;

  // Check whole word boundary if required
  if (match_whole_word_) {
    bool start_ok = (pos == 0 || !std::isalnum(source[pos - 1]));
    bool end_ok = (pos + search.size() >= source.size() ||
                   !std::isalnum(source[pos + search.size()]));
    if (!start_ok || !end_ok) {
      // Search for a whole-word match further in the string
      while (pos != std::string::npos) {
        start_ok = (pos == 0 || !std::isalnum(source[pos - 1]));
        end_ok = (pos + search.size() >= source.size() ||
                  !std::isalnum(source[pos + search.size()]));
        if (start_ok && end_ok)
          break;
        pos = source.find(search, pos + 1);
      }
      if (pos == std::string::npos)
        return 0;
    }
  }

  // Perform the replacement in the original (case-preserving) text
  text.replace(pos, search_text_.size(), replace_text_);
  UpdateCurrentMessageFromText(text);
  FinalizePendingUndo();
  return 1;
}

int MessageEditor::ReplaceAllMatches() {
  if (search_text_.empty()) {
    return 0;
  }
  if (!current_parse_errors_.empty()) {
    replace_status_ =
        "Replace All blocked: resolve the current message parse errors first";
    replace_status_error_ = true;
    return -1;
  }

  auto replace_in_text = [&](std::string& text) -> int {
    int count = 0;
    std::string search = search_text_;

    if (!case_sensitive_) {
      std::transform(search.begin(), search.end(), search.begin(), ::tolower);
    }

    size_t pos = 0;
    while (pos < text.size()) {
      std::string source = text;
      if (!case_sensitive_) {
        std::transform(source.begin(), source.end(), source.begin(), ::tolower);
      }

      size_t found = source.find(search, pos);
      if (found == std::string::npos)
        break;

      if (match_whole_word_) {
        bool start_ok = (found == 0 || !std::isalnum(source[found - 1]));
        bool end_ok = (found + search.size() >= source.size() ||
                       !std::isalnum(source[found + search.size()]));
        if (!start_ok || !end_ok) {
          pos = found + 1;
          continue;
        }
      }

      text.replace(found, search_text_.size(), replace_text_);
      pos = found + replace_text_.size();
      count++;
    }
    return count;
  };

  struct PlannedReplacement {
    int message_index;
    bool is_expanded;
    std::string text;
    int count;
  };
  std::vector<PlannedReplacement> plan;

  const auto plan_replacements = [&](const std::vector<MessageData>& messages,
                                     bool is_expanded) -> bool {
    for (size_t i = 0; i < messages.size(); ++i) {
      const int parsed_index =
          (is_expanded ? expanded_message_base_id_ : 0) + static_cast<int>(i);
      if (parsed_index < 0 ||
          parsed_index >= static_cast<int>(parsed_messages_.size())) {
        continue;
      }

      std::string text = parsed_messages_[parsed_index];
      const int count = replace_in_text(text);
      if (count == 0) {
        continue;
      }

      const auto parsed = ParseMessageToDataWithDiagnostics(text);
      if (!parsed.ok() ||
          (is_expanded && text.find("[BANK]") != std::string::npos)) {
        const std::string error =
            !parsed.ok() ? parsed.errors.front()
                         : "[BANK] is not valid in expanded messages";
        replace_status_ = absl::StrFormat(
            "Replace All aborted at %s message %d: %s",
            is_expanded ? "expanded" : "vanilla", static_cast<int>(i), error);
        replace_status_error_ = true;
        return false;
      }

      plan.push_back(
          {static_cast<int>(i), is_expanded, std::move(text), count});
    }
    return true;
  };

  // Validate the complete batch before changing any message or undo state.
  if (!plan_replacements(list_of_texts_, false) ||
      !plan_replacements(expanded_messages_, true)) {
    return -1;
  }

  const int previous_index = current_message_index_;
  const bool previous_expanded = current_message_is_expanded_;
  int total_replacements = 0;

  for (const auto& replacement : plan) {
    current_message_ = replacement.is_expanded
                           ? expanded_messages_[replacement.message_index]
                           : list_of_texts_[replacement.message_index];
    current_message_index_ = replacement.message_index;
    current_message_is_expanded_ = replacement.is_expanded;
    message_text_box_.text = replacement.text;
    UpdateCurrentMessageFromText(replacement.text);
    FinalizePendingUndo();
    total_replacements += replacement.count;
  }

  current_message_index_ = previous_index;
  current_message_is_expanded_ = previous_expanded;

  // Refresh the current message's text box from updated data
  int current_parsed_idx = current_message_index_;
  if (current_message_is_expanded_) {
    current_parsed_idx = expanded_message_base_id_ + current_message_index_;
  }
  if (current_parsed_idx >= 0 &&
      current_parsed_idx < static_cast<int>(parsed_messages_.size())) {
    message_text_box_.text = parsed_messages_[current_parsed_idx];
  }

  // Refresh current_message_ to reflect replacements
  if (current_message_is_expanded_) {
    if (current_message_index_ >= 0 &&
        current_message_index_ < static_cast<int>(expanded_messages_.size())) {
      current_message_ = expanded_messages_[current_message_index_];
    }
  } else {
    if (current_message_index_ >= 0 &&
        current_message_index_ < static_cast<int>(list_of_texts_.size())) {
      current_message_ = list_of_texts_[current_message_index_];
    }
  }

  const auto current_diagnostics =
      ParseMessageToDataWithDiagnostics(message_text_box_.text);
  current_parse_errors_ = current_diagnostics.errors;
  current_parse_warnings_ = current_diagnostics.warnings;

  return total_replacements;
}

}  // namespace editor
}  // namespace yaze
