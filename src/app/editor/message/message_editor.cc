#include "message_editor.h"

#include <algorithm>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/editor/system/panel_manager.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style.h"
#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "rom/rom.h"
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
  // Register panels with PanelManager (dependency injection)
  if (!dependencies_.panel_manager)
    return;

  auto* panel_manager = dependencies_.panel_manager;
  const size_t session_id = dependencies_.session_id;

  // Register EditorPanel implementations (they provide both metadata and drawing)
  panel_manager->RegisterEditorPanel(
      std::make_unique<MessageListPanel>([this]() { DrawMessageList(); }));
  panel_manager->RegisterEditorPanel(
      std::make_unique<MessageEditorPanel>([this]() { DrawCurrentMessage(); }));
  panel_manager->RegisterEditorPanel(std::make_unique<FontAtlasPanel>([this]() {
    DrawFontAtlas();
    DrawExpandedMessageSettings();
  }));
  panel_manager->RegisterEditorPanel(
      std::make_unique<DictionaryPanel>([this]() {
        DrawTextCommands();
        DrawSpecialCharacters();
        DrawDictionary();
      }));

  // Show message list by default
  panel_manager->ShowPanel(session_id, "message.message_list");

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
    DrawMessagePreview();
  } else {
    LOG_ERROR("MessageEditor", "No messages found in ROM!");
  }
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
    const size_t available =
        std::min(raw_font_gfx_data_.size(),
                 rom_size - static_cast<size_t>(kGfxFont));
    std::copy_n(rom()->data() + kGfxFont, available, raw_font_gfx_data_.begin());
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
  const size_t row_count =
      (font_data.size() + atlas_width - 1) / atlas_width;
  const int atlas_height =
      static_cast<int>(std::max<size_t>(1, row_count));

  const size_t expected_size =
      static_cast<size_t>(atlas_width) * atlas_height;
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
  // Panel drawing is handled centrally by PanelManager::DrawAllVisiblePanels()
  // via the EditorPanel implementations registered in Initialize().
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
    gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                          &font_gfx_bitmap_);
  }
  if (!current_font_gfx16_bitmap_.is_active() && !current_message_.Data.empty()) {
    DrawMessagePreview();
  }
  if (current_font_gfx16_bitmap_.is_active() &&
      !current_font_gfx16_bitmap_.texture()) {
    gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                          &current_font_gfx16_bitmap_);
  }
}

void MessageEditor::DrawMessageList() {
  gui::BeginNoPadding();
  if (BeginChild("##MessagesList", ImVec2(0, 0), true,
                 ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    gui::EndNoPadding();
    if (ImGui::Button("Import Bundle")) {
      std::string path = util::FileDialogWrapper::ShowOpenFileDialog();
      if (!path.empty()) {
        ImportMessageBundleFromFile(path);
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Export Bundle")) {
      std::string path = util::FileDialogWrapper::ShowSaveFileDialog();
      if (!path.empty()) {
        auto status =
            ExportMessageBundleToJson(path, list_of_texts_, expanded_messages_);
        if (!status.ok()) {
          message_bundle_status_ =
              absl::StrFormat("Export failed: %s", status.message());
          message_bundle_status_error_ = true;
        } else {
          message_bundle_status_ =
              absl::StrFormat("Exported bundle: %s", path);
          message_bundle_status_error_ = false;
        }
      }
    }
    if (!message_bundle_status_.empty()) {
      ImVec4 color = message_bundle_status_error_
                         ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
                         : ImVec4(0.6f, 0.9f, 0.6f, 1.0f);
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
              current_message_ = message;
              current_message_index_ = message.ID;
              current_message_is_expanded_ = false;
              message_text_box_.text = parsed_messages_[message.ID];
              DrawMessagePreview();
            }
            PopID();
            
            TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 1.0f, 1.0f), "Vanilla");
            
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
              current_message_ = expanded_message;
              current_message_index_ = expanded_message.ID;
              current_message_is_expanded_ = true;
              message_text_box_.text = display_text;
              DrawMessagePreview();
            }
            PopID();
            
            TableNextColumn();
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.4f, 1.0f), "Expanded");
            
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
  auto line_warnings = ValidateMessageLineWidths(message_text_box_.text);
  if (!line_warnings.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f),
                       "Line width warnings");
    for (const auto& warning : line_warnings) {
      ImGui::BulletText("%s", warning.c_str());
    }
  }
  Separator();
  gui::MemoryEditorPopup("Message Data", current_message_.Data);

  ImGui::BeginChild("##MessagePreview", ImVec2(0, 0), true, 1);
  EnsureFontTexturesReady();
  Text("Message Preview");
  if (Button("View Palette")) {
    ImGui::OpenPopup("Palette");
  }
  if (ImGui::BeginPopup("Palette")) {
    gui::DisplayPalette(font_preview_colors_, true);
    ImGui::EndPopup();
  }
  gui::BeginPadding(1);
  BeginChild("CurrentGfxFont", ImVec2(348, 0), true,
             ImGuiWindowFlags_NoScrollWithMouse);
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
    const float src_width = std::min(dest_width * 0.5f,
                                     static_cast<float>(kCurrentMessageWidth));
    src_height = std::min(dest_height * 0.5f,
                          static_cast<float>(kCurrentMessageHeight));
    current_font_gfx16_canvas_.DrawBitmap(
        current_font_gfx16_bitmap_, ImVec2(0, 0),  // Destination position
        ImVec2(dest_width, dest_height),           // Destination size
        ImVec2(0, message_preview_.shown_lines * 16),  // Source position
        ImVec2(src_width, src_height)                 // Source size
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
        overlay_draw_list->AddLine(
            ImVec2(canvas_p0.x, y),
            ImVec2(canvas_p0.x + canvas_sz.x, y),
            IM_COL32(100, 180, 255, 180), 1.5f);
        overlay_draw_list->AddText(
            ImVec2(canvas_p0.x + canvas_sz.x + 4, y - 6),
            IM_COL32(100, 180, 255, 200), "[V]");
      }
    }
  }

  current_font_gfx16_canvas_.DrawGrid();
  current_font_gfx16_canvas_.DrawOverlay();
  EndChild();

  // Message Structure info panel
  if (ImGui::CollapsingHeader("Message Structure",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("Lines: %d", message_preview_.text_line + 1);

    int scroll_count = 0;
    int current_line_chars = 0;
    int line_num = 0;

    for (size_t i = 0; i < current_message_.Data.size(); i++) {
      uint8_t byte = current_message_.Data[i];
      if (byte == kScrollVertical) {
        scroll_count++;
        ImGui::TextColored(
            ImVec4(0.4f, 0.7f, 1.0f, 1.0f),
            "  [V] Scroll at byte %zu (line %d, %d chars)",
            i, line_num, current_line_chars);
        current_line_chars = 0;
        line_num++;
      } else if (byte == kLine1) {
        ImGui::TextColored(ImVec4(0.7f, 0.85f, 0.5f, 1.0f),
                           "  [1] Line 1 at byte %zu", i);
        current_line_chars = 0;
        line_num = 0;
      } else if (byte == kLine2) {
        ImGui::TextColored(ImVec4(0.7f, 0.85f, 0.5f, 1.0f),
                           "  [2] Line 2 at byte %zu", i);
        current_line_chars = 0;
        line_num = 1;
      } else if (byte == kLine3) {
        ImGui::TextColored(ImVec4(0.7f, 0.85f, 0.5f, 1.0f),
                           "  [3] Line 3 at byte %zu", i);
        current_line_chars = 0;
        line_num = 2;
      } else if (byte < 100) {
        current_line_chars++;
      }
    }

    if (scroll_count == 0) {
      ImGui::TextDisabled("No scroll breaks in this message");
    } else {
      ImGui::Text("Total scroll breaks: %d", scroll_count);
    }

    // Character width budget
    ImGui::Separator();
    ImGui::TextDisabled("Line width budget (max ~170px):");
    int estimated_line_width = current_line_chars * 8;
    float width_ratio = static_cast<float>(estimated_line_width) / 170.0f;
    ImVec4 width_color =
        (width_ratio > 1.0f)
            ? ImVec4(0.9f, 0.2f, 0.2f, 1.0f)
            : (width_ratio > 0.85f)
                  ? ImVec4(0.9f, 0.7f, 0.1f, 1.0f)
                  : ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
    ImGui::TextColored(width_color, "Last line: ~%dpx / 170px (%d chars)",
                       estimated_line_width, current_line_chars);
  }

  ImGui::EndChild();
}

void MessageEditor::DrawFontAtlas() {
  EnsureFontTexturesReady();
  gui::BeginCanvas(font_gfx_canvas_, ImVec2(256, 256));
  font_gfx_canvas_.DrawBitmap(font_gfx_bitmap_, 0, 0, 2.0f);
  font_gfx_canvas_.DrawTileSelector(16, 32);
  gui::EndCanvas(font_gfx_canvas_);
}

void MessageEditor::DrawExpandedMessageSettings() {
  ImGui::BeginChild("##ExpandedMessageSettings", ImVec2(0, 130), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  ImGui::Text("Expanded Messages");

  if (ImGui::Button("Load from ROM")) {
    auto status = LoadExpandedMessagesFromRom();
    if (!status.ok()) {
      LOG_WARN("MessageEditor", "Load from ROM: %s",
               std::string(status.message()).c_str());
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Load from File")) {
    std::string path = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!path.empty()) {
      expanded_message_path_ = path;
      expanded_message_base_id_ = ResolveExpandedMessageBaseId();
      parsed_messages_.resize(expanded_message_base_id_);
      expanded_messages_.clear();
      std::vector<std::string> parsed_expanded;
      auto status = LoadExpandedMessages(expanded_message_path_, parsed_expanded,
                                         expanded_messages_,
                                         message_preview_.all_dictionaries_);
      if (!status.ok()) {
        if (auto* popup_manager = dependencies_.popup_manager) {
          popup_manager->Show("Error");
        }
      } else {
        parsed_messages_.insert(parsed_messages_.end(), parsed_expanded.begin(),
                                parsed_expanded.end());
      }
    }
  }

  if (expanded_messages_.size() > 0) {
    ImGui::Text("Source: %s", expanded_message_path_.c_str());
    ImGui::Text("Messages: %lu", expanded_messages_.size());

    // Capacity indicator
    int capacity = GetExpandedTextDataEnd() - GetExpandedTextDataStart() + 1;
    int used = CalculateExpandedBankUsage();
    int remaining = capacity - used;
    float usage_ratio = static_cast<float>(used) / static_cast<float>(capacity);

    ImVec4 capacity_color;
    if (usage_ratio < 0.75f) {
      capacity_color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);  // Green
    } else if (usage_ratio < 0.90f) {
      capacity_color = ImVec4(0.9f, 0.7f, 0.1f, 1.0f);  // Yellow
    } else {
      capacity_color = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);  // Red
    }
    ImGui::TextColored(capacity_color, "Bank: %d / %d bytes (%d free)",
                       used, capacity, remaining);

    if (ImGui::Button("Add New Message")) {
      MessageData new_message;
      new_message.ID = expanded_messages_.back().ID + 1;
      new_message.Address = expanded_messages_.back().Address +
                            expanded_messages_.back().Data.size();
      expanded_messages_.push_back(new_message);
      const int display_id = expanded_message_base_id_ + new_message.ID;
      if (display_id >= 0 &&
          static_cast<size_t>(display_id) >= parsed_messages_.size()) {
        parsed_messages_.resize(display_id + 1);
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Export to JSON")) {
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

  std::string raw_text = text;
  raw_text.erase(std::remove(raw_text.begin(), raw_text.end(), '\n'),
                 raw_text.end());

  current_message_.RawString = raw_text;
  current_message_.ContentsParsed = text;
  current_message_.Data = ParseMessageToData(raw_text);

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
        current_message_index_ <
            static_cast<int>(expanded_messages_.size())) {
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
  bool expanded_modified = false;

  auto entries = entries_or.value();
  for (const auto& entry : entries) {
    auto parse_result = ParseMessageToDataWithDiagnostics(entry.text);
    auto line_warnings = ValidateMessageLineWidths(entry.text);
    warnings += static_cast<int>(parse_result.warnings.size());
    warnings += static_cast<int>(line_warnings.size());

    if (!parse_result.ok()) {
      errors++;
      continue;
    }

    if (entry.bank == MessageBank::kVanilla) {
      if (entry.id < 0 ||
          entry.id >= static_cast<int>(list_of_texts_.size())) {
        errors++;
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
      applied++;
    } else {
      if (entry.id < 0) {
        errors++;
        continue;
      }
      if (entry.id >= static_cast<int>(expanded_messages_.size())) {
        const int target_size = entry.id + 1;
        expanded_messages_.resize(target_size);
        for (int i = 0; i < target_size; ++i) {
          expanded_messages_[i].ID = i;
        }
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

  if (errors > 0) {
    message_bundle_status_ = absl::StrFormat(
        "Import finished with %d errors (%d applied, %d warnings).", errors,
        applied, warnings);
    message_bundle_status_error_ = true;
  } else {
    message_bundle_status_ = absl::StrFormat(
        "Imported %d messages (%d warnings).", applied, warnings);
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
  std::vector<uint8_t> backup = rom()->vector();

  for (int i = 0; i < kWidthArraySize; i++) {
    RETURN_IF_ERROR(rom()->WriteByte(kCharactersWidth + i,
                                     message_preview_.width_array[i]));
  }

  int pos = kTextData;
  bool in_second_bank = false;

  for (const auto& message : list_of_texts_) {
    for (const auto value : message.Data) {
      RETURN_IF_ERROR(rom()->WriteByte(pos, value));

      if (value == kBlockTerminator) {
        // Make sure we didn't go over the space available in the first block.
        // 0x7FFF available.
        if (!in_second_bank && pos > kTextDataEnd) {
          return absl::InternalError(DisplayTextOverflowError(pos, true));
        }

        // Switch to the second block.
        pos = kTextData2 - 1;
        in_second_bank = true;
      }

      pos++;
    }

    RETURN_IF_ERROR(rom()->WriteByte(pos++, kMessageTerminator));
  }

  // Verify that we didn't go over the space available for the second block.
  // 0x14BF available.
  if (in_second_bank && pos > kTextData2End) {
    std::copy(backup.begin(), backup.end(), rom()->mutable_data());
    return absl::InternalError(DisplayTextOverflowError(pos, false));
  }

  RETURN_IF_ERROR(rom()->WriteByte(pos, 0xFF));

  // Also save expanded messages to main ROM if any are loaded
  if (!expanded_messages_.empty()) {
    auto status = SaveExpandedMessages();
    if (!status.ok()) {
      std::copy(backup.begin(), backup.end(), rom()->mutable_data());
      return status;
    }
  }

  return absl::OkStatus();
}

absl::Status MessageEditor::SaveExpandedMessages() {
  if (expanded_messages_.empty()) {
    return absl::OkStatus();
  }

  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Collect all expanded message text strings (mirrors CLI message-write path)
  std::vector<std::string> all_texts;
  all_texts.reserve(expanded_messages_.size());
  for (const auto& msg : expanded_messages_) {
    all_texts.push_back(msg.RawString);
  }

  // Write to main ROM buffer at the expanded text data region
  RETURN_IF_ERROR(WriteExpandedTextData(
      rom_->mutable_data(),
      GetExpandedTextDataStart(),
      GetExpandedTextDataEnd(),
      all_texts));

  // Recalculate addresses after sequential write
  int pos = GetExpandedTextDataStart();
  for (auto& msg : expanded_messages_) {
    msg.Address = pos;
    auto bytes = ParseMessageToData(msg.RawString);
    pos += static_cast<int>(bytes.size()) + 1;  // +1 for 0x7F terminator
  }

  return absl::OkStatus();
}

absl::Status MessageEditor::LoadExpandedMessagesFromRom() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  expanded_message_base_id_ = ResolveExpandedMessageBaseId();
  parsed_messages_.resize(expanded_message_base_id_);

  expanded_messages_.clear();
  expanded_messages_ = ReadExpandedTextData(
      rom_->mutable_data(), GetExpandedTextDataStart());

  if (expanded_messages_.empty()) {
    return absl::NotFoundError(
        "No expanded messages found in ROM at expanded text region");
  }

  // Parse the expanded messages and append to the unified list
  auto parsed_expanded =
      ParseMessageData(expanded_messages_, message_preview_.all_dictionaries_);
  for (const auto& msg : expanded_messages_) {
    if (msg.ID >= 0 &&
        msg.ID < static_cast<int>(parsed_expanded.size())) {
      parsed_messages_.push_back(parsed_expanded[msg.ID]);
    }
  }

  expanded_message_path_ = "(ROM)";
  return absl::OkStatus();
}

int MessageEditor::CalculateExpandedBankUsage() const {
  if (expanded_messages_.empty()) return 0;
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
  redo_stack_.clear();

  int parsed_index = current_message_index_;
  if (current_message_is_expanded_) {
    parsed_index = expanded_message_base_id_ + current_message_index_;
  }
  std::string text;
  if (parsed_index >= 0 &&
      parsed_index < static_cast<int>(parsed_messages_.size())) {
    text = parsed_messages_[parsed_index];
  }

  undo_stack_.push_back({current_message_, text, current_message_index_,
                         current_message_is_expanded_});

  if (undo_stack_.size() > kMaxUndoHistory) {
    undo_stack_.pop_front();
  }
}

void MessageEditor::ApplySnapshot(const MessageSnapshot& snapshot) {
  current_message_ = snapshot.message;
  current_message_index_ = snapshot.message_index;
  current_message_is_expanded_ = snapshot.is_expanded;
  message_text_box_.text = snapshot.parsed_text;

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
        snapshot.message_index <
            static_cast<int>(expanded_messages_.size())) {
      expanded_messages_[snapshot.message_index] = snapshot.message;
    }
  } else {
    if (snapshot.message_index >= 0 &&
        snapshot.message_index < static_cast<int>(list_of_texts_.size())) {
      list_of_texts_[snapshot.message_index] = snapshot.message;
    }
  }

  DrawMessagePreview();
}

absl::Status MessageEditor::Undo() {
  if (undo_stack_.empty()) {
    return absl::OkStatus();
  }

  // Save current state to redo stack before undoing
  int parsed_index = current_message_index_;
  if (current_message_is_expanded_) {
    parsed_index = expanded_message_base_id_ + current_message_index_;
  }
  std::string current_text;
  if (parsed_index >= 0 &&
      parsed_index < static_cast<int>(parsed_messages_.size())) {
    current_text = parsed_messages_[parsed_index];
  }
  redo_stack_.push_back({current_message_, current_text,
                         current_message_index_, current_message_is_expanded_});

  ApplySnapshot(undo_stack_.back());
  undo_stack_.pop_back();
  return absl::OkStatus();
}

absl::Status MessageEditor::Redo() {
  if (redo_stack_.empty()) {
    return absl::OkStatus();
  }

  // Save current state to undo stack before redoing
  int parsed_index = current_message_index_;
  if (current_message_is_expanded_) {
    parsed_index = expanded_message_base_id_ + current_message_index_;
  }
  std::string current_text;
  if (parsed_index >= 0 &&
      parsed_index < static_cast<int>(parsed_messages_.size())) {
    current_text = parsed_messages_[parsed_index];
  }
  undo_stack_.push_back({current_message_, current_text,
                         current_message_index_, current_message_is_expanded_});

  ApplySnapshot(redo_stack_.back());
  redo_stack_.pop_back();
  return absl::OkStatus();
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
  if (ImGui::Begin("Find Text", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    static char find_text[256] = "";
    ImGui::InputText("Search", find_text, IM_ARRAYSIZE(find_text));

    if (ImGui::Button("Find Next")) {
      search_text_ = find_text;
    }

    ImGui::SameLine();
    if (ImGui::Button("Find All")) {
      search_text_ = find_text;
    }

    ImGui::SameLine();
    if (ImGui::Button("Replace")) {
      // TODO: Implement replace functionality
    }

    ImGui::Checkbox("Case Sensitive", &case_sensitive_);
    ImGui::SameLine();
    ImGui::Checkbox("Match Whole Word", &match_whole_word_);
  }
  ImGui::End();

  return absl::OkStatus();
}

}  // namespace editor
}  // namespace yaze
