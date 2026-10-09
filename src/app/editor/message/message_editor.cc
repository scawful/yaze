#include "message_editor.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/editor/menu/status_bar.h"
#include "app/editor/message/message_id_resolver.h"
#include "app/editor/message/message_list_query.h"
#include "app/editor/message/message_source_sync.h"
#include "app/editor/system/session/hack_manifest_save_validation.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/canvas/item_context_menu.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/empty_state.h"
#include "imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "rom/transaction.h"
#include "rom/write_fence.h"
#include "util/file_util.h"
#include "util/hex.h"
#include "util/log.h"

namespace yaze {
namespace editor {

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
  project_source_load_attempted_ = false;
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
      std::make_unique<DictionaryPanel>([this]() { DrawMessageTools(); }));

  // Show message list by default
  window_manager->OpenWindow(session_id, "message.message_list");

  for (int i = 0; i < kWidthArraySize; i++) {
    message_preview_.width_array[i] = rom()->data()[kCharactersWidth + i];
  }

  message_preview_.all_dictionaries_ = BuildDictionaryEntries(rom());
  const int message_read_limit = static_cast<int>(
      std::min(static_cast<size_t>(rom()->size()),
               static_cast<size_t>(std::numeric_limits<int>::max())));
  list_of_texts_ =
      ReadAllTextData(rom()->mutable_data(), kTextData, message_read_limit);
  LOG_INFO("MessageEditor", "Loaded %zu messages from ROM",
           list_of_texts_.size());

  ResolveFontPalette();
  LoadFontGraphics();
  if (!font_preview_colors_.empty()) {
    *current_font_gfx16_bitmap_.mutable_palette() = font_preview_colors_;
  }
  parsed_messages_ =
      ParseMessageData(list_of_texts_, message_preview_.all_dictionaries_);
  NoteLoadedMessages();

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

  // Projects without a canonical source can inspect expanded ROM messages on
  // navigation. A missing project source must remain an explicit source error.
  if (!resolved.has_value() && expanded_count == 0 &&
      !HasMessageSourceLayout() && display_id >= expanded_base_id && rom_ &&
      rom_->is_loaded()) {
    const auto range = ExpandedInspectionRange();
    const int start = range.first;
    const int end = range.second;
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
    editor_cursor_ = 0;
    find_has_match_ = false;
    pending_selection_start_ = -1;
    pending_selection_end_ = -1;
    scroll_list_to_selection_ = true;
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
  editor_cursor_ = 0;
  find_has_match_ = false;
  pending_selection_start_ = -1;
  pending_selection_end_ = -1;
  scroll_list_to_selection_ = true;
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
  if (!HasMessageSourceLayout() || project_source_load_attempted_) {
    return absl::OkStatus();
  }
  project_source_load_attempted_ = true;
  // Loading assets may be requested again during Save or editor activation.
  // Never replace an existing draft (including an explicit ROM inspection).
  if (!expanded_messages_.empty() || !source_loaded_canonical_sha_.empty()) {
    return absl::OkStatus();
  }
  const auto status = LoadCanonicalExpandedSource();
  if (!status.ok()) {
    message_bundle_status_ = absl::StrFormat(
        "Expanded source unavailable: %s. Vanilla dialogue remains available.",
        status.message());
    message_bundle_status_error_ = true;
  }
  // Source errors belong in the Message editor; they must not prevent opening
  // the project or editing its independent vanilla messages.
  return absl::OkStatus();
}

absl::Status MessageEditor::Update() {
  // Panels are drawn by WorkspaceWindowManager. Shortcuts stay here so they
  // run once per frame, and only while a message window actually has focus.
  HandleMessageShortcuts();
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

namespace {

struct MessageInputState {
  int* cursor = nullptr;
  int* select_start = nullptr;
  int* select_end = nullptr;
};

int MessageInputCallback(ImGuiInputTextCallbackData* data) {
  auto* state = static_cast<MessageInputState*>(data->UserData);
  if (state == nullptr) {
    return 0;
  }
  if (state->select_start != nullptr && state->select_end != nullptr &&
      *state->select_start >= 0) {
    const int start = *state->select_start;
    const int end = std::max(start, *state->select_end);
    data->CursorPos = std::min(end, data->BufTextLen);
    data->SelectionStart = std::min(start, data->BufTextLen);
    data->SelectionEnd = data->CursorPos;
    *state->select_start = -1;
    *state->select_end = -1;
  }
  if (state->cursor != nullptr) {
    *state->cursor = data->CursorPos;
  }
  return 0;
}

bool MessageSurfaceFocused() {
  ImGuiContext* context = ImGui::GetCurrentContext();
  if (context == nullptr || context->NavWindow == nullptr) {
    return false;
  }
  ImGuiWindow* root = context->NavWindow->RootWindow != nullptr
                          ? context->NavWindow->RootWindow
                          : context->NavWindow;
  return root->Name != nullptr &&
         std::strstr(root->Name, "message.message") != nullptr;
}

}  // namespace

void MessageEditor::HandleMessageShortcuts() {
  if (!MessageSurfaceFocused()) {
    return;
  }
  const ImGuiIO& io = ImGui::GetIO();
  if (io.KeyAlt && !io.KeyCtrl && !io.KeyShift &&
      ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
    SelectAdjacentMessage(-1);
  }
  if (io.KeyAlt && !io.KeyCtrl && !io.KeyShift &&
      ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
    SelectAdjacentMessage(1);
  }
  if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F3, false)) {
    if (!find_panel_open_) {
      find_panel_open_ = true;
      find_focus_search_ = true;
    }
    AdvanceFind(!io.KeyShift);
  }
}

void MessageEditor::ContributeStatus(StatusBar* status_bar) {
  if (status_bar == nullptr) {
    return;
  }
  const char* bank = current_message_is_expanded_ ? "Expanded" : "Vanilla";
  status_bar->SetCustomSegment(
      "Message",
      absl::StrFormat("%s $%04X", bank, CurrentDisplayId() & 0xFFFF));
  status_bar->SetCustomSegment(
      "Size", absl::StrFormat("%zu B", current_message_.Data.size()));
  status_bar->SetCustomSegment(
      "Lines", absl::StrFormat("%d", message_preview_.text_line + 1));
  const int shown = static_cast<int>(CollectMessageRows(true).size());
  const int total = static_cast<int>(CollectMessageRows(false).size());
  if (!list_filter_.empty() || list_bank_filter_ != 0) {
    status_bar->SetCustomSegment("Shown",
                                 absl::StrFormat("%d/%d", shown, total));
  }
  if (!current_parse_errors_.empty()) {
    status_bar->SetEditorMode("Parse error");
  } else if (dirty_state_.vanilla_messages || dirty_state_.expanded_messages ||
             dirty_state_.font_widths) {
    status_bar->SetEditorMode("Unsaved");
  } else {
    status_bar->SetEditorMode("Dialogue");
  }
}

int MessageEditor::CurrentDisplayId() const {
  if (current_message_is_expanded_) {
    return expanded_message_base_id_ + current_message_index_;
  }
  return current_message_index_;
}

std::string_view MessageEditor::ContentsForDisplayId(int display_id) const {
  if (display_id < 0 ||
      display_id >= static_cast<int>(parsed_messages_.size())) {
    return {};
  }
  return parsed_messages_[display_id];
}

std::vector<MessageEditor::MessageRow> MessageEditor::CollectMessageRows(
    bool apply_filter) const {
  std::vector<MessageRow> rows;
  rows.reserve(list_of_texts_.size() + expanded_messages_.size());
  const auto bank = static_cast<MessageBankFilter>(list_bank_filter_);
  const auto consider = [&](int display_id, bool expanded, uint32_t address) {
    if (apply_filter &&
        !MessageListQueryMatches(display_id, address,
                                 ContentsForDisplayId(display_id), expanded,
                                 bank, list_filter_)) {
      return;
    }
    rows.push_back(MessageRow{display_id, expanded, address});
  };
  for (const auto& message : list_of_texts_) {
    consider(message.ID, false, static_cast<uint32_t>(message.Address));
  }
  for (const auto& message : expanded_messages_) {
    consider(expanded_message_base_id_ + message.ID, true,
             static_cast<uint32_t>(message.Address));
  }
  return rows;
}

void MessageEditor::InsertTokenAtCursor(const std::string& token) {
  if (token.empty()) {
    return;
  }
  auto& text = message_text_box_.text;
  const int pos = std::clamp(editor_cursor_, 0, static_cast<int>(text.size()));
  text.insert(static_cast<size_t>(pos), token);
  editor_cursor_ = pos + static_cast<int>(token.size());
  pending_selection_start_ = editor_cursor_;
  pending_selection_end_ = editor_cursor_;
  ImGui::ClearActiveID();
  UpdateCurrentMessageFromText(text);
}

void MessageEditor::SelectAdjacentMessage(int delta) {
  if (delta == 0) {
    return;
  }
  if (!current_parse_errors_.empty()) {
    replace_status_ = "Resolve parse errors before switching messages";
    replace_status_error_ = true;
    find_panel_open_ = true;
    return;
  }
  const std::vector<MessageRow> rows = CollectMessageRows(true);
  if (rows.empty()) {
    return;
  }
  int current = 0;
  const int current_id = CurrentDisplayId();
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    if (rows[i].display_id == current_id &&
        rows[i].expanded == current_message_is_expanded_) {
      current = i;
      break;
    }
  }
  const int next =
      std::clamp(current + delta, 0, static_cast<int>(rows.size()) - 1);
  if (next == current) {
    return;
  }
  FinalizePendingUndo();
  OpenMessageById(rows[next].display_id);
}

bool MessageEditor::ActivateFindMatch(const MessageRow& row, size_t offset,
                                      bool wrapped) {
  const bool same = row.display_id == CurrentDisplayId() &&
                    row.expanded == current_message_is_expanded_;
  if (!same) {
    if (!current_parse_errors_.empty()) {
      replace_status_ = "Resolve parse errors before leaving this message";
      replace_status_error_ = true;
      return false;
    }
    FinalizePendingUndo();
    if (!OpenMessageById(row.display_id)) {
      replace_status_ = "Could not open the matching message";
      replace_status_error_ = true;
      return false;
    }
  }
  find_match_start_ = offset;
  find_has_match_ = true;
  const int end = static_cast<int>(offset + search_text_.size());
  pending_selection_start_ = static_cast<int>(offset);
  pending_selection_end_ = end;
  editor_cursor_ = end;
  scroll_list_to_selection_ = true;
  replace_status_ = absl::StrFormat("Match in $%04X%s", row.display_id & 0xFFFF,
                                    wrapped ? " (wrapped)" : "");
  replace_status_error_ = false;
  return true;
}

bool MessageEditor::AdvanceFind(bool forward) {
  if (search_text_.empty()) {
    replace_status_ = "Enter text to search";
    replace_status_error_ = true;
    return false;
  }
  const std::vector<MessageRow> rows = CollectMessageRows(false);
  if (rows.empty()) {
    replace_status_ = "No messages to search";
    replace_status_error_ = true;
    return false;
  }

  int start = 0;
  const int current_id = CurrentDisplayId();
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    if (rows[i].display_id == current_id &&
        rows[i].expanded == current_message_is_expanded_) {
      start = i;
      break;
    }
  }

  const int count = static_cast<int>(rows.size());
  const size_t cursor = static_cast<size_t>(std::clamp(
      editor_cursor_, 0, static_cast<int>(message_text_box_.text.size())));
  const size_t forward_from = find_has_match_ ? find_match_start_ + 1 : cursor;
  const size_t backward_before = find_has_match_ ? find_match_start_ : cursor;

  auto match_at = [&](int row_index, size_t bound,
                      bool from_start) -> std::optional<size_t> {
    const std::string_view text =
        ContentsForDisplayId(rows[row_index].display_id);
    if (forward) {
      return FindNextTextMatch(text, search_text_, from_start ? 0 : bound,
                               case_sensitive_, match_whole_word_);
    }
    const size_t before = from_start ? std::string::npos : bound;
    return FindPreviousTextMatch(text, search_text_, before, case_sensitive_,
                                 match_whole_word_);
  };

  if (auto found =
          match_at(start, forward ? forward_from : backward_before, false)) {
    return ActivateFindMatch(rows[start], *found, false);
  }
  for (int step = 1; step < count; ++step) {
    const int row_index =
        forward ? (start + step) % count : (start - step + count) % count;
    if (auto found = match_at(row_index, 0, true)) {
      return ActivateFindMatch(rows[row_index], *found, false);
    }
  }
  // Forward wrap only matters once the cursor has moved past the start.
  // Backward wrap also covers "previous" from the first match, which should
  // land on the last match in this message.
  const bool can_wrap = forward ? forward_from > 0 : true;
  if (can_wrap) {
    if (auto found = match_at(start, 0, true)) {
      return ActivateFindMatch(rows[start], *found, true);
    }
  }
  replace_status_ = "No matches";
  replace_status_error_ = true;
  return false;
}

void MessageEditor::OptimizeCurrentMessage() {
  MessageData scratch;
  const std::string optimized = scratch.OptimizeMessageForDictionary(
      message_text_box_.text, message_preview_.all_dictionaries_);
  if (optimized == message_text_box_.text) {
    replace_status_ = "Dictionary optimize made no changes";
    replace_status_error_ = false;
    find_panel_open_ = true;
    return;
  }
  const auto parsed = ParseMessageToDataWithDiagnostics(optimized);
  if (!parsed.ok()) {
    replace_status_ = "Dictionary optimize produced invalid text";
    replace_status_error_ = true;
    find_panel_open_ = true;
    return;
  }
  message_text_box_.text = optimized;
  pending_selection_start_ = 0;
  pending_selection_end_ = 0;
  editor_cursor_ = 0;
  ImGui::ClearActiveID();
  UpdateCurrentMessageFromText(optimized);
  FinalizePendingUndo();
  replace_status_ = "Applied dictionary compression";
  replace_status_error_ = false;
  find_panel_open_ = true;
}

void MessageEditor::DrawFindBar() {
  if (!find_panel_open_) {
    return;
  }
  ImGui::PushID("FindReplace");
  if (find_focus_search_) {
    ImGui::SetKeyboardFocusHere();
    find_focus_search_ = false;
  }
  const float half =
      std::max(80.0f, ImGui::GetContentRegionAvail().x * 0.5f - 8.0f);
  ImGui::SetNextItemWidth(half);
  if (ImGui::InputTextWithHint("##find", "Find in dialogue", &search_text_)) {
    find_has_match_ = false;
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  ImGui::InputTextWithHint("##replace", "Replace with", &replace_text_);

  if (ImGui::Button(tr("Find Next"))) {
    AdvanceFind(true);
  }
  ImGui::SameLine();
  if (ImGui::Button(tr("Previous"))) {
    AdvanceFind(false);
  }
  ImGui::SameLine();
  if (ImGui::Button(tr("Find All"))) {
    if (search_text_.empty()) {
      replace_status_ = "Enter text to search";
      replace_status_error_ = true;
    } else {
      list_filter_ = search_text_;
      list_bank_filter_ = 0;
      const int matches = static_cast<int>(CollectMessageRows(true).size());
      replace_status_ = absl::StrFormat("List filtered to %d message%s",
                                        matches, matches == 1 ? "" : "s");
      replace_status_error_ = matches == 0;
      scroll_list_to_selection_ = true;
      if (dependencies_.window_manager) {
        dependencies_.window_manager->OpenWindow(dependencies_.session_id,
                                                 "message.message_list");
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(tr("Replace"))) {
    const int count = ReplaceCurrentMatch();
    if (count > 0) {
      replace_status_ = "Replaced 1 occurrence";
      replace_status_error_ = false;
    } else if (search_text_.empty()) {
      replace_status_ = "Enter text to search";
      replace_status_error_ = true;
    } else {
      replace_status_ = "No match found in current message";
      replace_status_error_ = true;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(tr("Replace All"))) {
    int count = ReplaceAllMatches();
    if (count >= 0) {
      replace_status_ = absl::StrFormat("Replaced %d occurrence%s", count,
                                        count == 1 ? "" : "s");
      replace_status_error_ = count == 0;
    }
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(tr("Close"))) {
    find_panel_open_ = false;
  }

  ImGui::Checkbox(tr("Case Sensitive"), &case_sensitive_);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Applies to Find Next, Previous, and Replace.");
  }
  ImGui::SameLine();
  ImGui::Checkbox(tr("Whole Word"), &match_whole_word_);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Match only when both sides are not letters or digits.");
  }
  if (!replace_status_.empty()) {
    const ImVec4 color =
        replace_status_error_ ? gui::GetErrorColor() : gui::GetSuccessColor();
    ImGui::TextColored(color, "%s", replace_status_.c_str());
  }
  ImGui::PopID();
}

void MessageEditor::NoteLoadedMessages() {
  if (baseline_messages_.size() >= parsed_messages_.size()) {
    return;
  }
  baseline_messages_.insert(
      baseline_messages_.end(),
      parsed_messages_.begin() +
          static_cast<std::ptrdiff_t>(baseline_messages_.size()),
      parsed_messages_.end());
}

std::string MessageEditor::CurrentReferenceText() const {
  const int display_id = CurrentDisplayId();
  if (display_id < 0 ||
      display_id >= static_cast<int>(baseline_messages_.size())) {
    return {};
  }
  return baseline_messages_[display_id];
}

void MessageEditor::AcceptCurrentAsReference() {
  if (!current_parse_errors_.empty()) {
    return;
  }
  const int display_id = CurrentDisplayId();
  if (display_id < 0) {
    return;
  }
  if (display_id >= static_cast<int>(baseline_messages_.size())) {
    baseline_messages_.resize(static_cast<size_t>(display_id) + 1);
  }
  baseline_messages_[display_id] = message_text_box_.text;
  reference_view_text_ = message_text_box_.text;
}

void MessageEditor::DrawDialogueTextPane(float height) {
  const std::string reference = CurrentReferenceText();
  if (reference != reference_view_text_) {
    reference_view_text_ = reference;
  }
  const bool show_reference = show_reference_ && !reference_view_text_.empty();
  const float label_h = ImGui::GetFrameHeightWithSpacing();
  const float text_h = std::max(48.0f, height - label_h);

  auto draw_editor = [&](float editor_height) {
    MessageInputState input_state{&editor_cursor_, &pending_selection_start_,
                                  &pending_selection_end_};
    if (InputTextMultiline("##MessageEditor", &message_text_box_.text,
                           ImVec2(-1.0f, editor_height),
                           ImGuiInputTextFlags_CallbackAlways,
                           MessageInputCallback, &input_state)) {
      UpdateCurrentMessageFromText(message_text_box_.text);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      FinalizePendingUndo();
    }
  };

  if (!show_reference) {
    draw_editor(-1.0f);
    return;
  }

  const bool side_by_side = ImGui::GetContentRegionAvail().x >= 640.0f;
  if (side_by_side &&
      ImGui::BeginTable("##DialogueColumns", 2, ImGuiTableFlags_Resizable,
                        ImVec2(0.0f, height))) {
    ImGui::TableSetupColumn(tr("Loaded"), ImGuiTableColumnFlags_WidthStretch,
                            0.45f);
    ImGui::TableSetupColumn(tr("Editing"), ImGuiTableColumnFlags_WidthStretch,
                            0.55f);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled(tr("Loaded"));
    ImGui::InputTextMultiline("##MessageReference", &reference_view_text_,
                              ImVec2(-1.0f, text_h),
                              ImGuiInputTextFlags_ReadOnly);
    ImGui::TableNextColumn();
    ImGui::TextDisabled(tr("Editing"));
    draw_editor(text_h);
    ImGui::EndTable();
    return;
  }

  ImGui::TextDisabled(tr("Loaded"));
  const float reference_h = std::clamp(height * 0.34f, 64.0f, 140.0f);
  ImGui::InputTextMultiline("##MessageReference", &reference_view_text_,
                            ImVec2(-1.0f, reference_h),
                            ImGuiInputTextFlags_ReadOnly);
  ImGui::TextDisabled(tr("Editing"));
  draw_editor(std::max(48.0f, height - reference_h - label_h * 2.0f));
}

bool MessageEditor::HasMessageSourceLayout() const {
  const auto* project = dependencies_.project;
  return project != nullptr && project->hack_manifest.loaded() &&
         project->hack_manifest.message_layout().source.has_value();
}

bool MessageEditor::ExpandedEditsAreSourceOwned() const {
  return HasMessageSourceLayout();
}

std::pair<int, int> MessageEditor::ExpandedInspectionRange() const {
  const int fallback_start = GetExpandedTextDataStart();
  const int fallback_end = GetExpandedTextDataEnd();
  const auto* project = dependencies_.project;
  if (project == nullptr || !project->hack_manifest.loaded()) {
    return {fallback_start, fallback_end};
  }
  const auto& layout = project->hack_manifest.message_layout();
  if (layout.data_start == 0 || layout.data_end < layout.data_start) {
    return {fallback_start, fallback_end};
  }
  const int start = static_cast<int>(SnesToPc(layout.data_start));
  const int end = static_cast<int>(SnesToPc(layout.data_end));
  if (end < start) {
    return {fallback_start, fallback_end};
  }
  return {start, end};
}

void MessageEditor::InvalidateSourcePreview() {
  if (source_sync_ready_) {
    source_sync_status_ =
        "The draft changed after preview. Preview again before publishing.";
    source_sync_status_error_ = true;
  }
  source_sync_ready_ = false;
  source_sync_changed_ = false;
  source_sync_sha_before_.clear();
  source_sync_draft_sha_.clear();
}

absl::StatusOr<std::filesystem::path>
MessageEditor::ResolveCanonicalBundlePath() const {
  if (!HasMessageSourceLayout() || dependencies_.project->filepath.empty()) {
    return absl::FailedPreconditionError(
        "The project hack manifest does not define messages.source");
  }
  const auto& configured = dependencies_.project->hack_manifest.message_layout()
                               .source->canonical_bundle_path;
  const std::filesystem::path relative(configured);
  if (relative.empty() || relative.is_absolute() || relative.has_root_name()) {
    return absl::InvalidArgumentError(
        "Canonical message bundle must be a project-relative path");
  }
  std::error_code error;
  const std::filesystem::path descriptor =
      std::filesystem::absolute(dependencies_.project->filepath, error);
  if (error) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot resolve project descriptor %s: %s",
                        dependencies_.project->filepath, error.message()));
  }
  const std::filesystem::path root =
      std::filesystem::canonical(descriptor.parent_path(), error);
  if (error) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot resolve project root for %s: %s",
                        dependencies_.project->filepath, error.message()));
  }
  const std::filesystem::path joined = (root / relative).lexically_normal();
  const std::filesystem::path from_root = joined.lexically_relative(root);
  if (from_root.empty() || from_root.is_absolute() ||
      *from_root.begin() == "..") {
    return absl::InvalidArgumentError(
        "Canonical message bundle must stay inside the project root");
  }
  return joined;
}

absl::StatusOr<std::string> MessageEditor::HashFileSha256(
    const std::filesystem::path& path) const {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return absl::NotFoundError(absl::StrFormat(
        "Cannot read canonical message bundle: %s", path.string()));
  }
  const std::string content((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());
  if (!input.good() && !input.eof()) {
    return absl::InternalError(absl::StrFormat(
        "Cannot read canonical message bundle: %s", path.string()));
  }
  return ComputeMessageSourceSha256(content);
}

bool MessageEditor::IsCanonicalBundlePath(const std::string& path) const {
  const auto canonical_or = ResolveCanonicalBundlePath();
  if (!canonical_or.ok() || path.empty()) {
    return false;
  }
  std::error_code error;
  const auto incoming = std::filesystem::weakly_canonical(path, error);
  if (error) {
    return false;
  }
  const auto target =
      std::filesystem::weakly_canonical(canonical_or.value(), error);
  return !error && incoming == target;
}

MessageEditor::ExpandedDraftSnapshot MessageEditor::CaptureExpandedDraft()
    const {
  ExpandedDraftSnapshot snapshot;
  snapshot.messages = expanded_messages_;
  snapshot.vanilla_messages = list_of_texts_;
  snapshot.parsed_messages = parsed_messages_;
  snapshot.base_id = expanded_message_base_id_;
  snapshot.path = expanded_message_path_;
  snapshot.expanded_dirty = dirty_state_.expanded_messages;
  snapshot.vanilla_dirty = dirty_state_.vanilla_messages;
  snapshot.loaded_canonical_sha = source_loaded_canonical_sha_;
  snapshot.preview_ready = source_sync_ready_;
  snapshot.preview_changed = source_sync_changed_;
  snapshot.preview_sha_before = source_sync_sha_before_;
  snapshot.preview_draft_sha = source_sync_draft_sha_;
  snapshot.preview_summary = source_sync_summary_;
  snapshot.preview_status = source_sync_status_;
  snapshot.preview_status_error = source_sync_status_error_;
  return snapshot;
}

void MessageEditor::RestoreExpandedDraft(
    const ExpandedDraftSnapshot& snapshot) {
  expanded_messages_ = snapshot.messages;
  list_of_texts_ = snapshot.vanilla_messages;
  parsed_messages_ = snapshot.parsed_messages;
  expanded_message_base_id_ = snapshot.base_id;
  expanded_message_path_ = snapshot.path;
  dirty_state_.expanded_messages = snapshot.expanded_dirty;
  dirty_state_.vanilla_messages = snapshot.vanilla_dirty;
  source_loaded_canonical_sha_ = snapshot.loaded_canonical_sha;
  source_sync_ready_ = snapshot.preview_ready;
  source_sync_changed_ = snapshot.preview_changed;
  source_sync_sha_before_ = snapshot.preview_sha_before;
  source_sync_draft_sha_ = snapshot.preview_draft_sha;
  source_sync_summary_ = snapshot.preview_summary;
  source_sync_status_ = snapshot.preview_status;
  source_sync_status_error_ = snapshot.preview_status_error;
}

std::string MessageEditor::MessageSourceIncomingPath() const {
  std::error_code error;
  std::filesystem::path directory = std::filesystem::temp_directory_path(error);
  if (error) {
    directory = std::filesystem::current_path(error);
  }
  return (directory / absl::StrFormat("yaze-message-source-sync-%zu.json",
                                      dependencies_.session_id))
      .string();
}

void MessageEditor::PreviewMessageSource() {
  source_sync_ready_ = false;
  source_sync_changed_ = false;
  source_sync_sha_before_.clear();
  source_sync_draft_sha_.clear();
  source_sync_summary_.clear();
  if (!current_parse_errors_.empty()) {
    source_sync_status_ = "Resolve parse errors before previewing source";
    source_sync_status_error_ = true;
    return;
  }
  if (expanded_messages_.empty()) {
    source_sync_status_ = "Load expanded messages before publishing source";
    source_sync_status_error_ = true;
    return;
  }
  if (!HasMessageSourceLayout()) {
    source_sync_status_ =
        "The project hack manifest does not define messages.source";
    source_sync_status_error_ = true;
    return;
  }
  if (source_loaded_canonical_sha_.empty()) {
    source_sync_status_ =
        "Load the canonical expanded bundle before previewing publication";
    source_sync_status_error_ = true;
    return;
  }

  const auto canonical_sha_or = [&]() -> absl::StatusOr<std::string> {
    const auto path_or = ResolveCanonicalBundlePath();
    if (!path_or.ok()) {
      return path_or.status();
    }
    return HashFileSha256(path_or.value());
  }();
  if (!canonical_sha_or.ok()) {
    source_sync_status_ = std::string(canonical_sha_or.status().message());
    source_sync_status_error_ = true;
    return;
  }
  if (canonical_sha_or.value() != source_loaded_canonical_sha_) {
    source_sync_status_ =
        "Canonical source changed on disk after it was loaded. Reload the "
        "canonical bundle before preview. This draft was not published.";
    source_sync_status_error_ = true;
    return;
  }

  const std::string incoming_path = MessageSourceIncomingPath();
  const auto write_status =
      ExportMessageBundleToJson(incoming_path, {}, expanded_messages_);
  if (!write_status.ok()) {
    source_sync_status_ = absl::StrFormat(
        "Could not write the preview bundle: %s", write_status.message());
    source_sync_status_error_ = true;
    return;
  }
  const auto draft_sha_or =
      HashFileSha256(std::filesystem::path(incoming_path));
  if (!draft_sha_or.ok()) {
    source_sync_status_ = std::string(draft_sha_or.status().message());
    source_sync_status_error_ = true;
    return;
  }

  auto result_or = SyncMessageSource(*dependencies_.project, incoming_path,
                                     MessageSourceSyncOptions{});
  if (!result_or.ok()) {
    source_sync_status_ = std::string(result_or.status().message());
    source_sync_status_error_ = true;
    return;
  }

  const MessageSourceSyncResult& result = *result_or;
  source_sync_ready_ = true;
  source_sync_changed_ = result.changed;
  source_sync_sha_before_ = result.source_sha256_before;
  source_sync_draft_sha_ = draft_sha_or.value();
  source_sync_summary_ = absl::StrFormat(
      "%s. %d update%s, %zu / %zu bytes. SHA %s",
      result.changed ? "Source would change" : "Source already matches",
      result.incoming_updates, result.incoming_updates == 1 ? "" : "s",
      result.encoded_size, result.capacity,
      result.proposed_source_sha256.substr(
          0, std::min<size_t>(12, result.proposed_source_sha256.size())));
  source_sync_status_ = source_sync_summary_;
  source_sync_status_error_ = false;
}

void MessageEditor::PublishMessageSource() {
  if (!source_sync_ready_ || source_sync_sha_before_.empty() ||
      source_sync_draft_sha_.empty()) {
    source_sync_status_ = "Preview the publication before writing it";
    source_sync_status_error_ = true;
    return;
  }
  if (!current_parse_errors_.empty() || expanded_messages_.empty() ||
      !HasMessageSourceLayout()) {
    source_sync_status_ = "Publication is no longer valid";
    source_sync_status_error_ = true;
    source_sync_ready_ = false;
    source_sync_draft_sha_.clear();
    return;
  }

  const std::string incoming_path = MessageSourceIncomingPath();
  const auto write_status =
      ExportMessageBundleToJson(incoming_path, {}, expanded_messages_);
  if (!write_status.ok()) {
    source_sync_status_ = absl::StrFormat(
        "Could not write the publication bundle: %s", write_status.message());
    source_sync_status_error_ = true;
    return;
  }
  const auto draft_sha_or =
      HashFileSha256(std::filesystem::path(incoming_path));
  if (!draft_sha_or.ok() || draft_sha_or.value() != source_sync_draft_sha_) {
    source_sync_ready_ = false;
    source_sync_draft_sha_.clear();
    source_sync_status_ =
        "The draft changed after preview. Preview again before publishing. "
        "Nothing was written.";
    source_sync_status_error_ = true;
    return;
  }

  const auto canonical_path_or = ResolveCanonicalBundlePath();
  const auto canonical_sha_or =
      canonical_path_or.ok()
          ? HashFileSha256(canonical_path_or.value())
          : absl::StatusOr<std::string>(canonical_path_or.status());
  if (!canonical_sha_or.ok() ||
      canonical_sha_or.value() != source_sync_sha_before_ ||
      canonical_sha_or.value() != source_loaded_canonical_sha_) {
    source_sync_ready_ = false;
    source_sync_draft_sha_.clear();
    source_sync_status_ =
        "Canonical source changed after preview. Reload the canonical bundle "
        "and preview again. This draft was not published.";
    source_sync_status_error_ = true;
    return;
  }

  MessageSourceSyncOptions options;
  options.write = true;
  options.expected_source_sha256 = source_sync_sha_before_;
  auto result_or =
      SyncMessageSource(*dependencies_.project, incoming_path, options);
  source_sync_ready_ = false;
  source_sync_draft_sha_.clear();
  if (!result_or.ok()) {
    source_sync_status_ = std::string(result_or.status().message());
    source_sync_status_error_ = true;
    return;
  }
  dirty_state_.expanded_messages = false;
  source_loaded_canonical_sha_ = result_or->proposed_source_sha256;
  source_sync_changed_ = false;
  source_sync_status_ =
      result_or->wrote ? "Published the canonical bundle and ASM include. "
                         "Rebuild before the patched ROM shows this text."
                       : "Canonical source already matched; nothing written";
  source_sync_status_error_ = false;
  source_sync_summary_ =
      absl::StrFormat("%s\n%s", result_or->canonical_bundle_path.string(),
                      result_or->generated_asm_include_path.string());
}

MessageEditor::MessageSourcePublishButtons
MessageEditor::DrawMessageSourcePublishButtons() {
  MessageSourcePublishButtons state;
  ImGui::BeginDisabled(expanded_messages_.empty() ||
                       !current_parse_errors_.empty());
  if (ImGui::Button(tr("Preview Publication"))) {
    PreviewMessageSource();
  }
  state.preview_disabled =
      (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!source_sync_ready_ || !source_sync_changed_);
  if (ImGui::Button(tr("Publish"))) {
    ImGui::OpenPopup("Publish message source###PublishMessageSource");
  }
  state.publish_disabled =
      (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
  ImGui::EndDisabled();
  return state;
}

void MessageEditor::DrawMessageSourceSync() {
  ImGui::Separator();
  gui::SectionHeader(ICON_MD_PUBLISH, tr("ASM Source"), gui::GetInfoColor());
  if (!HasMessageSourceLayout()) {
    ImGui::TextWrapped(
        "%s",
        tr("Load a project whose hack manifest defines messages.source to "
           "publish expanded dialogue as a canonical bundle and ASM include."));
    return;
  }

  const auto& source =
      dependencies_.project->hack_manifest.message_layout().source.value();
  ImGui::TextDisabled("%s", source.canonical_bundle_path.c_str());
  ImGui::TextDisabled("%s", source.generated_asm_include_path.c_str());
  ImGui::TextWrapped(
      "%s",
      tr("Preview checks capacity and the current source SHA without writing. "
         "Publish updates the bundle and generated include, not the ROM."));

  DrawMessageSourcePublishButtons();

  if (ImGui::BeginPopupModal("Publish message source###PublishMessageSource",
                             nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped(
        "%s", tr("Write the expanded messages into the canonical bundle and "
                 "generated ASM include. The ROM is not modified."));
    if (!source_sync_summary_.empty()) {
      ImGui::TextWrapped("%s", source_sync_summary_.c_str());
    }
    if (ImGui::Button(tr("Publish now"))) {
      PublishMessageSource();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancel"))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (!source_sync_status_.empty()) {
    const ImVec4 color = source_sync_status_error_ ? gui::GetErrorColor()
                                                   : gui::GetSuccessColor();
    ImGui::TextWrapped("%s", source_sync_status_.c_str());
  }
}

void MessageEditor::DrawMessageHeader() {
  const bool can_navigate = current_parse_errors_.empty();
  ImGui::BeginDisabled(!can_navigate);
  if (ImGui::Button(ICON_MD_NAVIGATE_BEFORE "##prev_message")) {
    SelectAdjacentMessage(-1);
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("Previous message (Alt+Up)");
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!can_navigate);
  if (ImGui::Button(ICON_MD_NAVIGATE_NEXT "##next_message")) {
    SelectAdjacentMessage(1);
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("Next message (Alt+Down)");
  }

  ImGui::SameLine();
  ImGui::Text("$%04X", CurrentDisplayId() & 0xFFFF);
  ImGui::SameLine();
  gui::StatusBadge(
      current_message_is_expanded_ ? tr("Expanded") : tr("Vanilla"),
      current_message_is_expanded_ ? gui::ButtonType::Warning
                                   : gui::ButtonType::Info);
  ImGui::SameLine();
  ImGui::TextDisabled("%s", util::HexLong(current_message_.Address).c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("%zu B", current_message_.Data.size());

  ImGui::SameLine();
  const std::string reference_now = CurrentReferenceText();
  if (!reference_now.empty() && reference_now != message_text_box_.text) {
    gui::StatusBadge(tr("Edited"), gui::ButtonType::Warning);
    ImGui::SameLine();
  }
  if (ImGui::Button(show_reference_ ? tr("Hide reference")
                                    : tr("Show reference"))) {
    show_reference_ = !show_reference_;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Show the text from when this message was loaded.");
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(reference_now == message_text_box_.text ||
                       !current_parse_errors_.empty());
  if (ImGui::Button(tr("Accept"))) {
    AcceptCurrentAsReference();
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip(
        "Make the current draft the loaded reference for this message.");
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SEARCH "##toggle_find")) {
    find_panel_open_ = !find_panel_open_;
    find_focus_search_ = find_panel_open_;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Find and replace (F3 finds the next match)");
  }
  ImGui::SameLine();
  if (ImGui::Button(tr("Optimize"))) {
    OptimizeCurrentMessage();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Replace dictionary phrases in this message with [D:XX] tokens.");
  }
}

void MessageEditor::DrawMessageList() {
  gui::LayoutHelpers::BeginToolbar("##MessageListTools");
  if (gui::LayoutHelpers::ToolbarButton(ICON_MD_UPLOAD)) {
    const std::string path = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!path.empty()) {
      ImportMessageBundleFromFile(path);
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Import message bundle");
  }
  ImGui::SameLine();
  if (gui::LayoutHelpers::ToolbarButton(ICON_MD_DOWNLOAD)) {
    const std::string path = util::FileDialogWrapper::ShowSaveFileDialog();
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
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Export message bundle");
  }
  gui::LayoutHelpers::EndToolbar();

  ImGui::SetNextItemWidth(
      std::max(120.0f, ImGui::GetContentRegionAvail().x - 120.0f));
  ImGui::InputTextWithHint("##message_filter", "Filter by text, ID, or address",
                           &list_filter_);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(110.0f);
  ImGui::Combo("##message_bank", &list_bank_filter_,
               "All\0Vanilla\0Expanded\0");

  if (!message_bundle_status_.empty()) {
    const ImVec4 color = message_bundle_status_error_ ? gui::GetErrorColor()
                                                      : gui::GetSuccessColor();
    ImGui::TextWrapped("%s", message_bundle_status_.c_str());
  }

  const std::vector<MessageRow> rows = CollectMessageRows(true);
  const int total = static_cast<int>(CollectMessageRows(false).size());
  ImGui::TextDisabled("%d / %d", static_cast<int>(rows.size()), total);

  if (list_of_texts_.empty() && expanded_messages_.empty()) {
    gui::DrawEmptyState(gui::EmptyStateOptions{ICON_MD_CHAT, "No messages",
                                               "Load a ROM to edit dialogue.",
                                               nullptr, nullptr, true});
    return;
  }
  if (rows.empty()) {
    gui::EmptyStateOptions empty;
    empty.icon = ICON_MD_SEARCH;
    empty.title = "No matches";
    empty.detail = "No messages match this filter.";
    empty.action_label = "Clear filter";
    empty.compact = true;
    empty.on_action = [this]() {
      list_filter_.clear();
      list_bank_filter_ = 0;
    };
    gui::DrawEmptyState(empty);
    return;
  }

  constexpr ImGuiTableFlags kListFlags =
      ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders |
      ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_ScrollY;
  if (!BeginTable("##MessagesTable", 4, kListFlags, ImVec2(0, 0))) {
    return;
  }
  TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 64);
  TableSetupColumn("Bank", ImGuiTableColumnFlags_WidthFixed, 84);
  TableSetupColumn("Contents", ImGuiTableColumnFlags_WidthStretch);
  TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 90);
  TableHeadersRow();

  const int current_id = CurrentDisplayId();
  int target_row = -1;
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    if (rows[i].display_id == current_id &&
        rows[i].expanded == current_message_is_expanded_) {
      target_row = i;
      break;
    }
  }

  if (scroll_list_to_selection_ && target_row < 0) {
    scroll_list_to_selection_ = false;
  }
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(rows.size()));
  if (scroll_list_to_selection_ && target_row >= 0) {
    clipper.IncludeItemByIndex(target_row);
  }
  while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
      const MessageRow& message = rows[row];
      const bool selected = row == target_row;
      ImGui::TableNextRow();
      TableNextColumn();
      ImGui::PushID(row);
      char id_label[16];
      std::snprintf(id_label, sizeof(id_label), "$%04X",
                    message.display_id & 0xFFFF);
      if (ImGui::Selectable(id_label, selected,
                            ImGuiSelectableFlags_SpanAllColumns |
                                ImGuiSelectableFlags_AllowOverlap)) {
        if (!selected && !current_parse_errors_.empty()) {
          replace_status_ = "Resolve parse errors before switching messages";
          replace_status_error_ = true;
          find_panel_open_ = true;
        } else if (!selected) {
          FinalizePendingUndo();
          OpenMessageById(message.display_id);
        }
      }
      gui::ItemContextMenu(nullptr, [this, display_id = message.display_id,
                                     address = message.address]() {
        std::vector<gui::MenuItemSpec> items;
        auto open_item = gui::MenuItemSpec::Conditional(
            "Open Message",
            [this, display_id]() {
              FinalizePendingUndo();
              OpenMessageById(display_id);
            },
            [this]() { return current_parse_errors_.empty(); });
        open_item.icon = ICON_MD_ARROW_FORWARD;
        open_item.separator_after = true;
        items.push_back(std::move(open_item));
        items.push_back(gui::CopyToClipboardItem("Copy Message ID",
                                                 util::HexWord(display_id)));
        if (display_id >= 0 &&
            display_id < static_cast<int>(parsed_messages_.size())) {
          items.push_back(gui::CopyToClipboardItem(
              "Copy Text", parsed_messages_[display_id]));
        }
        items.push_back(
            gui::CopyToClipboardItem("Copy Address", util::HexLong(address)));
        return items;
      });
      if (selected && scroll_list_to_selection_) {
        ImGui::SetScrollHereY(0.25f);
        scroll_list_to_selection_ = false;
      }
      ImGui::PopID();

      TableNextColumn();
      gui::StatusBadge(
          message.expanded ? tr("Expanded") : tr("Vanilla"),
          message.expanded ? gui::ButtonType::Warning : gui::ButtonType::Info);

      TableNextColumn();
      const std::string preview =
          MessageListPreview(ContentsForDisplayId(message.display_id));
      ImGui::TextUnformatted(preview.c_str());

      TableNextColumn();
      ImGui::TextUnformatted(util::HexLong(message.address).c_str());
    }
  }
  EndTable();
}

void MessageEditor::DrawMessageStructure() {
  if (!ImGui::CollapsingHeader(tr("Message Structure"),
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  ImGui::Text(tr("Lines: %d"), message_preview_.text_line + 1);

  int scroll_count = 0;
  int current_line_chars = 0;
  int line_num = 0;
  const ImVec4 line_color = gui::GetSuccessColor();

  for (size_t i = 0; i < current_message_.Data.size(); i++) {
    const uint8_t byte = current_message_.Data[i];
    if (byte == kScrollVertical) {
      scroll_count++;
      ImGui::TextColored(gui::GetInfoColor(),
                         tr("  [V] Scroll at byte %zu (line %d, %d chars)"), i,
                         line_num, current_line_chars);
      current_line_chars = 0;
      line_num++;
    } else if (byte == kLine1) {
      ImGui::TextColored(line_color, tr("  [1] Line 1 at byte %zu"), i);
      current_line_chars = 0;
      line_num = 0;
    } else if (byte == kLine2) {
      ImGui::TextColored(line_color, tr("  [2] Line 2 at byte %zu"), i);
      current_line_chars = 0;
      line_num = 1;
    } else if (byte == kLine3) {
      ImGui::TextColored(line_color, tr("  [3] Line 3 at byte %zu"), i);
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

  ImGui::Separator();
  ImGui::TextDisabled(tr("Line width budget (max ~170px):"));
  const int estimated_line_width = current_line_chars * 8;
  const float width_ratio = static_cast<float>(estimated_line_width) / 170.0f;
  const ImVec4 width_color = (width_ratio > 1.0f)    ? gui::GetErrorColor()
                             : (width_ratio > 0.85f) ? gui::GetWarningColor()
                                                     : gui::GetSuccessColor();
  ImGui::TextColored(width_color, tr("Last line: ~%dpx / 170px (%d chars)"),
                     estimated_line_width, current_line_chars);
}

void MessageEditor::DrawCurrentMessage() {
  DrawMessageHeader();
  DrawFindBar();

  const float available = ImGui::GetContentRegionAvail().y;
  const float max_editor = std::max(96.0f, available - 160.0f);
  editor_pane_height_ = std::clamp(editor_pane_height_, 96.0f, max_editor);

  if (ImGui::BeginChild("##MessageTextPane", ImVec2(0, editor_pane_height_),
                        true)) {
    DrawDialogueTextPane(ImGui::GetContentRegionAvail().y);
  }
  ImGui::EndChild();

  ImGui::InvisibleButton("##message_editor_split", ImVec2(-1.0f, 6.0f));
  if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
  }
  if (ImGui::IsItemActive()) {
    editor_pane_height_ += ImGui::GetIO().MouseDelta.y;
  }

  if (!current_parse_errors_.empty() || !current_parse_warnings_.empty()) {
    if (ImGui::BeginChild("##MessageDiagnostics", ImVec2(0, 72), true)) {
      if (!current_parse_errors_.empty()) {
        ImGui::TextColored(gui::GetErrorColor(), tr("Message parse errors"));
        for (const auto& error : current_parse_errors_) {
          ImGui::BulletText("%s", error.c_str());
        }
      }
      if (!current_parse_warnings_.empty()) {
        ImGui::TextColored(gui::GetWarningColor(),
                           tr("Message parse warnings"));
        for (const auto& warning : current_parse_warnings_) {
          ImGui::BulletText("%s", warning.c_str());
        }
      }
    }
    ImGui::EndChild();
  }
  const auto line_warnings = ValidateMessageLineWidths(message_text_box_.text);
  if (!line_warnings.empty()) {
    ImGui::TextColored(gui::GetWarningColor(), tr("Line width warnings"));
    for (const auto& warning : line_warnings) {
      ImGui::BulletText("%s", warning.c_str());
    }
  }

  gui::MemoryEditorPopup("Message Data", current_message_.Data);

  if (ImGui::BeginChild("##MessagePreview", ImVec2(0, 0), true)) {
    EnsureFontTexturesReady();
    Text(tr("Message Preview"));
    ImGui::SameLine();
    if (Button(tr("View Palette"))) {
      ImGui::OpenPopup("Palette");
    }
    if (ImGui::BeginPopup("Palette")) {
      gui::DisplayPalette(font_preview_colors_, true);
      ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::TextDisabled(tr("Scroll the preview with the mouse wheel"));

    gui::BeginPadding(1);
    BeginChild("CurrentGfxFont", ImVec2(0, 200), true,
               ImGuiWindowFlags_NoScrollWithMouse);
    current_font_gfx16_canvas_.GetConfig().role =
        gui::CanvasRole::kSelectionSource;
    current_font_gfx16_canvas_.DrawBackground();
    gui::EndPadding();
    current_font_gfx16_canvas_.DrawContextMenu();

    if (ImGui::IsWindowHovered()) {
      const float wheel = ImGui::GetIO().MouseWheel;
      if (wheel > 0 && message_preview_.shown_lines > 0) {
        message_preview_.shown_lines--;
      } else if (wheel < 0 && message_preview_.shown_lines <
                                  message_preview_.text_line - 2) {
        message_preview_.shown_lines++;
      }
    }

    const ImVec2 preview_canvas_size = current_font_gfx16_canvas_.canvas_size();
    const float dest_width = std::max(0.0f, preview_canvas_size.x - 8.0f);
    const float dest_height = std::max(0.0f, preview_canvas_size.y - 8.0f);
    float src_height = 0.0f;
    if (dest_width > 0.0f && dest_height > 0.0f) {
      const float src_width =
          std::min(dest_width * 0.5f, static_cast<float>(kCurrentMessageWidth));
      src_height = std::min(dest_height * 0.5f,
                            static_cast<float>(kCurrentMessageHeight));
      current_font_gfx16_canvas_.DrawBitmap(
          current_font_gfx16_bitmap_, ImVec2(0, 0),
          ImVec2(dest_width, dest_height),
          ImVec2(0, message_preview_.shown_lines * 16),
          ImVec2(src_width, src_height));
    }

    {
      ImDrawList* overlay_draw_list = ImGui::GetWindowDrawList();
      const ImVec2 canvas_p0 = current_font_gfx16_canvas_.zero_point();
      const ImVec2 canvas_sz = current_font_gfx16_canvas_.canvas_size();
      const float line_height = 16.0f;
      float scale_y = 1.0f;
      if (dest_height > 0.0f && src_height > 0.0f) {
        scale_y = dest_height / src_height;
      }
      const ImU32 marker_color =
          ImGui::ColorConvertFloat4ToU32(gui::GetInfoColor());
      for (int marker_line : message_preview_.scroll_marker_lines) {
        const float src_y =
            (marker_line - message_preview_.shown_lines) * line_height;
        const float y = canvas_p0.y + src_y * scale_y;
        if (y >= canvas_p0.y && y <= canvas_p0.y + canvas_sz.y) {
          overlay_draw_list->AddLine(ImVec2(canvas_p0.x, y),
                                     ImVec2(canvas_p0.x + canvas_sz.x, y),
                                     marker_color, 1.5f);
          overlay_draw_list->AddText(
              ImVec2(canvas_p0.x + canvas_sz.x + 4, y - 6), marker_color,
              "[V]");
        }
      }
    }

    current_font_gfx16_canvas_.DrawGrid();
    current_font_gfx16_canvas_.DrawOverlay();
    EndChild();
    DrawMessageStructure();
  }
  ImGui::EndChild();
}

void MessageEditor::DrawFontAtlas() {
  gui::SectionHeader(ICON_MD_FONT_DOWNLOAD, tr("Font Atlas"),
                     gui::GetInfoColor());
  EnsureFontTexturesReady();
  font_gfx_canvas_.GetConfig().role = gui::CanvasRole::kSelectionSource;
  gui::BeginCanvas(font_gfx_canvas_, ImVec2(256, 256));
  font_gfx_canvas_.DrawBitmap(font_gfx_bitmap_, 0, 0, 2.0f);
  font_gfx_canvas_.DrawTileSelector(16, 32);
  gui::EndCanvas(font_gfx_canvas_);
  ImGui::TextDisabled(
      tr("2bpp dialogue font at $070000. Click a tile to inspect it."));
}

void MessageEditor::DrawExpandedMessageSettings() {
  ImGui::Separator();
  gui::SectionHeader(ICON_MD_LIBRARY_ADD, tr("Expanded Messages"),
                     gui::GetWarningColor());

  if (HasMessageSourceLayout()) {
    ImGui::TextWrapped(
        "%s",
        tr("Expanded dialogue loads automatically from the project bundle. "
           "Reload it after resolving a source error. Inspecting the built ROM "
           "uses the "
           "manifest data range and does not replace the canonical source."));
    if (ImGui::Button(tr("Load canonical source"))) {
      const auto status = LoadCanonicalExpandedSource();
      if (!status.ok()) {
        message_bundle_status_ = std::string(status.message());
        message_bundle_status_error_ = true;
      }
    }
    ImGui::SameLine();
  }
  if (ImGui::Button(HasMessageSourceLayout() ? tr("Inspect built ROM")
                                             : tr("Load from ROM"))) {
    auto status = LoadExpandedMessagesFromRom();
    if (!status.ok()) {
      LOG_WARN("MessageEditor", "Load from ROM: %s",
               std::string(status.message()).c_str());
      message_bundle_status_ =
          absl::StrFormat("Load from ROM failed: %s", status.message());
      message_bundle_status_error_ = true;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(tr("Load from File"))) {
    std::string path = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!path.empty()) {
      const auto status = LoadExpandedMessagesFromBin(path);
      if (!status.ok()) {
        message_bundle_status_ = std::string(status.message());
        message_bundle_status_error_ = true;
        if (auto* popup_manager = dependencies_.popup_manager) {
          popup_manager->Show("Error");
        }
      }
    }
  }

  if (expanded_messages_.empty()) {
    ImGui::TextDisabled(
        tr("No expanded bank loaded. Vanilla dialogue is still editable."));
    return;
  }

  ImGui::Text(tr("Source: %s"), expanded_message_path_.c_str());
  ImGui::Text(tr("Messages: %lu"), expanded_messages_.size());

  const auto inspection_range = ExpandedInspectionRange();
  const int capacity = inspection_range.second - inspection_range.first + 1;
  const int used = CalculateExpandedBankUsage();
  const int remaining = capacity - used;
  const float usage_ratio =
      capacity > 0 ? static_cast<float>(used) / static_cast<float>(capacity)
                   : 1.0f;
  const ImVec4 capacity_color = (usage_ratio < 0.75f)   ? gui::GetSuccessColor()
                                : (usage_ratio < 0.90f) ? gui::GetWarningColor()
                                                        : gui::GetErrorColor();
  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, capacity_color);
  char capacity_label[64];
  std::snprintf(capacity_label, sizeof(capacity_label), "%d / %d bytes", used,
                capacity);
  ImGui::ProgressBar(std::clamp(usage_ratio, 0.0f, 1.0f), ImVec2(-1, 0),
                     capacity_label);
  ImGui::PopStyleColor();
  ImGui::TextDisabled(tr("%d bytes free"), remaining);

  if (ImGui::Button(tr("Add New Message"))) {
    MessageData new_message;
    new_message.ID = expanded_messages_.back().ID + 1;
    new_message.Address =
        expanded_messages_.back().Address +
        static_cast<int>(expanded_messages_.back().Data.size());
    expanded_messages_.push_back(new_message);
    MarkDomainDirty(SaveDomain::kExpandedMessages);
    const int display_id = expanded_message_base_id_ + new_message.ID;
    if (display_id >= 0 &&
        static_cast<size_t>(display_id) >= parsed_messages_.size()) {
      parsed_messages_.resize(display_id + 1);
    }
    if (current_parse_errors_.empty()) {
      FinalizePendingUndo();
      OpenMessageById(display_id);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(tr("Export to JSON"))) {
    std::string path = util::FileDialogWrapper::ShowSaveFileDialog();
    if (!path.empty()) {
      PRINT_IF_ERROR(ExportMessagesToJson(path, expanded_messages_));
    }
  }

  DrawMessageSourceSync();
}

void MessageEditor::DrawTokenInserter(const std::vector<TextElement>& elements,
                                      std::string& filter, bool use_parameter) {
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##token_filter", "Filter tokens", &filter);
  if (!BeginTable("##TokenInserter", 2, kMessageTableFlags, ImVec2(0, 0))) {
    return;
  }
  TableSetupColumn("Token", ImGuiTableColumnFlags_WidthFixed, 110);
  TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
  TableHeadersRow();
  for (const auto& text_element : elements) {
    if (!filter.empty() &&
        !ContainsCaseInsensitive(text_element.GenericToken, filter) &&
        !ContainsCaseInsensitive(text_element.Description, filter)) {
      continue;
    }
    ImGui::TableNextRow();
    TableNextColumn();
    ImGui::PushID(text_element.ID);
    const std::string token =
        use_parameter ? text_element.GetParamToken(command_parameter_)
                      : text_element.GenericToken;
    if (ImGui::Selectable(text_element.GenericToken.c_str(), false,
                          ImGuiSelectableFlags_SpanAllColumns)) {
      InsertTokenAtCursor(token);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Insert %s", token.c_str());
    }
    ImGui::PopID();
    TableNextColumn();
    TextWrapped("%s", text_element.Description.c_str());
  }
  EndTable();
}

void MessageEditor::DrawTextCommands() {
  gui::InputHexByte("Command Parameter", &command_parameter_);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Argument byte for commands such as [W:02] or [SPD:00].");
  }
  DrawTokenInserter(TextCommands, command_filter_, true);
}

void MessageEditor::DrawSpecialCharacters() {
  DrawTokenInserter(SpecialChars, symbol_filter_, false);
}

void MessageEditor::DrawDictionary() {
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##dictionary_filter", "Filter dictionary",
                           &dictionary_filter_);
  std::vector<int> visible;
  visible.reserve(message_preview_.all_dictionaries_.size());
  for (int i = 0;
       i < static_cast<int>(message_preview_.all_dictionaries_.size()); ++i) {
    const auto& dictionary = message_preview_.all_dictionaries_[i];
    if (!dictionary_filter_.empty() &&
        !ContainsCaseInsensitive(dictionary.Contents, dictionary_filter_) &&
        !ContainsCaseInsensitive(dictionary.Token, dictionary_filter_)) {
      continue;
    }
    visible.push_back(i);
  }
  if (visible.empty()) {
    ImGui::TextDisabled(tr("No dictionary entries match."));
    return;
  }
  if (!BeginTable("##Dictionary", 3,
                  kMessageTableFlags | ImGuiTableFlags_ScrollY, ImVec2(0, 0))) {
    return;
  }
  TableSetupColumn("Token", ImGuiTableColumnFlags_WidthFixed, 72);
  TableSetupColumn("Text", ImGuiTableColumnFlags_WidthStretch);
  TableSetupColumn("Len", ImGuiTableColumnFlags_WidthFixed, 40);
  TableHeadersRow();

  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(visible.size()));
  while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
      const auto& dictionary = message_preview_.all_dictionaries_[visible[row]];
      ImGui::TableNextRow();
      TableNextColumn();
      ImGui::PushID(dictionary.ID);
      if (ImGui::Selectable(dictionary.Token.c_str(), false,
                            ImGuiSelectableFlags_SpanAllColumns)) {
        InsertTokenAtCursor(dictionary.Token);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Insert %s", dictionary.Token.c_str());
      }
      ImGui::PopID();
      TableNextColumn();
      Text("%s", dictionary.Contents.c_str());
      TableNextColumn();
      Text("%d", dictionary.Length);
    }
  }
  EndTable();
}

void MessageEditor::DrawMessageTools() {
  if (!ImGui::BeginTabBar("##MessageTools")) {
    return;
  }
  if (ImGui::BeginTabItem(tr("Commands"))) {
    DrawTextCommands();
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem(tr("Symbols"))) {
    DrawSpecialCharacters();
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem(tr("Dictionary"))) {
    DrawDictionary();
    ImGui::EndTabItem();
  }
  ImGui::EndTabBar();
}
void MessageEditor::UpdateCurrentMessageFromText(const std::string& text) {
  PushUndoSnapshot();

  MarkDomainDirty(current_message_is_expanded_ ? SaveDomain::kExpandedMessages
                                               : SaveDomain::kVanillaMessages);

  auto parse_result = ParseMessageToDataWithDiagnostics(text);
  current_parse_errors_ = parse_result.errors;
  current_parse_warnings_ = parse_result.warnings;
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

  const ExpandedDraftSnapshot snapshot = CaptureExpandedDraft();
  const bool rom_dirty_before = rom_ && rom_->dirty();
  const bool importing_canonical = IsCanonicalBundlePath(path);
  std::string canonical_sha;
  if (importing_canonical) {
    const auto sha_or = HashFileSha256(std::filesystem::path(path));
    if (!sha_or.ok()) {
      message_bundle_status_ = std::string(sha_or.status().message());
      message_bundle_status_error_ = true;
      return;
    }
    canonical_sha = sha_or.value();
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
    int pos = ExpandedInspectionRange().first;
    for (auto& message : expanded_messages_) {
      message.Address = pos;
      pos += static_cast<int>(message.Data.size()) + 1;
    }
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
  if (errors > 0) {
    RestoreExpandedDraft(snapshot);
    if (rom_) {
      rom_->set_dirty(rom_dirty_before);
    }
    return;
  }
  int current_display_id = current_message_index_;
  if (current_message_is_expanded_) {
    current_display_id = expanded_message_base_id_ + current_message_index_;
  }
  if (current_display_id >= 0) {
    OpenMessageById(current_display_id);
  }
  if (vanilla_updated > 0) {
    MarkDomainDirty(SaveDomain::kVanillaMessages);
  }
  if (expanded_modified) {
    if (importing_canonical) {
      source_loaded_canonical_sha_ = canonical_sha;
      dirty_state_.expanded_messages = false;
      InvalidateSourcePreview();
      expanded_message_path_ = path;
    } else {
      source_loaded_canonical_sha_.clear();
      MarkDomainDirty(SaveDomain::kExpandedMessages);
    }
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
    std::optional<size_t> expected_message_count;
    if (dependencies_.project &&
        dependencies_.project->hack_manifest.loaded()) {
      const int manifest_count =
          dependencies_.project->hack_manifest.message_layout().vanilla_count;
      if (manifest_count > 0) {
        expected_message_count = static_cast<size_t>(manifest_count);
      }
    }
    ASSIGN_OR_RETURN(
        const VanillaMessageSavePlan vanilla_plan,
        BuildVanillaMessageSavePlan(list_of_texts_, expected_message_count));

    plan.saves_vanilla_messages = true;
    for (const auto& write : vanilla_plan.writes()) {
      plan.writes.push_back(
          {SaveDomain::kVanillaMessages, write.start(), write.bytes()});
    }
  }

  if (include_expanded_messages && dirty_state_.expanded_messages &&
      !expanded_messages_.empty()) {
    const auto range = ExpandedInspectionRange();
    const int start = range.first;
    const int end = range.second;
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
    return absl::PermissionDeniedError(kOracleExpandedRomWriteRefusal);
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
  if (domain == SaveDomain::kExpandedMessages) {
    InvalidateSourcePreview();
  }
  if (rom_ && !(domain == SaveDomain::kExpandedMessages &&
                ExpandedEditsAreSourceOwned())) {
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

absl::Status MessageEditor::LoadCanonicalExpandedSource() {
  if (!HasMessageSourceLayout()) {
    return absl::FailedPreconditionError(
        "The project hack manifest does not define messages.source");
  }
  if (dirty_state_.expanded_messages || dirty_state_.vanilla_messages ||
      dirty_state_.font_widths || pending_undo_before_.has_value() ||
      !current_parse_errors_.empty()) {
    return absl::FailedPreconditionError(
        "The Message editor has pending edits; canonical source was not "
        "loaded");
  }
  auto source_or = LoadMessageSource(*dependencies_.project);
  if (!source_or.ok()) {
    return source_or.status();
  }
  // Commit only after the complete source bank validates. This does not import
  // vanilla messages, set ROM dirty, write a preview, or publish source files.
  expanded_message_base_id_ = source_or->first_expanded_id;
  expanded_messages_ = std::move(source_or->messages);
  expanded_message_path_ = source_or->canonical_bundle_path.string();
  source_loaded_canonical_sha_ = std::move(source_or->source_sha256);
  parsed_messages_.resize(expanded_message_base_id_ +
                          expanded_messages_.size());
  for (const auto& message : expanded_messages_) {
    parsed_messages_[expanded_message_base_id_ + message.ID] =
        message.RawString;
  }
  InvalidateSourcePreview();
  NoteLoadedMessages();
  message_bundle_status_ =
      absl::StrFormat("Loaded %zu expanded messages from project source: %s",
                      expanded_messages_.size(), expanded_message_path_);
  message_bundle_status_error_ = false;
  if (current_message_is_expanded_) {
    OpenMessageById(expanded_message_base_id_ + current_message_index_);
  }
  return absl::OkStatus();
}

absl::Status MessageEditor::LoadExpandedMessagesFromBin(
    const std::string& path) {
  const ExpandedDraftSnapshot snapshot = CaptureExpandedDraft();
  const bool rom_dirty_before = rom_ && rom_->dirty();
  expanded_message_base_id_ = ResolveExpandedMessageBaseId();
  parsed_messages_.resize(
      static_cast<size_t>(std::max(expanded_message_base_id_, 0)));
  expanded_messages_.clear();
  std::vector<std::string> parsed_expanded;
  std::string mutable_path = path;
  const auto status =
      LoadExpandedMessages(mutable_path, parsed_expanded, expanded_messages_,
                           message_preview_.all_dictionaries_);
  if (!status.ok() || expanded_messages_.empty()) {
    RestoreExpandedDraft(snapshot);
    if (rom_) {
      rom_->set_dirty(rom_dirty_before);
    }
    if (!status.ok()) {
      return status;
    }
    return absl::NotFoundError("No expanded messages found in file");
  }
  parsed_messages_.insert(parsed_messages_.end(), parsed_expanded.begin(),
                          parsed_expanded.end());
  expanded_message_path_ = path;
  source_loaded_canonical_sha_.clear();
  NoteLoadedMessages();
  MarkDomainDirty(SaveDomain::kExpandedMessages);
  return absl::OkStatus();
}

absl::Status MessageEditor::LoadExpandedMessagesFromRom() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  const ExpandedDraftSnapshot snapshot = CaptureExpandedDraft();
  const bool rom_dirty_before = rom_ && rom_->dirty();
  const auto range = ExpandedInspectionRange();
  const int end = std::min(range.second, static_cast<int>(rom_->size()) - 1);
  auto loaded = ReadExpandedTextData(rom_->mutable_data(), range.first, end);
  if (loaded.empty()) {
    RestoreExpandedDraft(snapshot);
    if (rom_) {
      rom_->set_dirty(rom_dirty_before);
    }
    return absl::NotFoundError(
        "No expanded messages found in ROM at the manifest data range");
  }

  expanded_message_base_id_ = ResolveExpandedMessageBaseId();
  parsed_messages_.resize(
      static_cast<size_t>(std::max(expanded_message_base_id_, 0)));
  expanded_messages_ = std::move(loaded);
  auto parsed_expanded =
      ParseMessageData(expanded_messages_, message_preview_.all_dictionaries_);
  for (const auto& msg : expanded_messages_) {
    if (msg.ID >= 0 && msg.ID < static_cast<int>(parsed_expanded.size())) {
      parsed_messages_.push_back(parsed_expanded[msg.ID]);
    }
  }

  expanded_message_path_ = "(ROM inspection)";
  source_loaded_canonical_sha_.clear();
  dirty_state_.expanded_messages = false;
  InvalidateSourcePreview();
  NoteLoadedMessages();
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
  // Edit > Find is a one-shot menu action. The bar itself is drawn every
  // frame from the message editor panel so it stays open across frames.
  find_panel_open_ = true;
  find_focus_search_ = true;
  if (dependencies_.window_manager) {
    dependencies_.window_manager->OpenWindow(dependencies_.session_id,
                                             "message.message_editor");
  }
  return absl::OkStatus();
}

int MessageEditor::ReplaceCurrentMatch() {
  if (search_text_.empty()) {
    return 0;
  }

  std::string& text = message_text_box_.text;
  const size_t cursor = static_cast<size_t>(
      std::clamp(editor_cursor_, 0, static_cast<int>(text.size())));
  const size_t from = find_has_match_ ? find_match_start_ : cursor;
  auto match = FindNextTextMatch(text, search_text_, from, case_sensitive_,
                                 match_whole_word_);
  if (!match && from > 0) {
    match = FindNextTextMatch(text, search_text_, 0, case_sensitive_,
                              match_whole_word_);
  }
  if (!match) {
    return 0;
  }

  text.replace(*match, search_text_.size(), replace_text_);
  pending_selection_start_ = static_cast<int>(*match);
  pending_selection_end_ =
      pending_selection_start_ + static_cast<int>(replace_text_.size());
  editor_cursor_ = pending_selection_end_;
  find_has_match_ = false;
  ImGui::ClearActiveID();
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
