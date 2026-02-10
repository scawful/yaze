#include "app/editor/code/memory_editor.h"

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

absl::Status MemoryEditor::Update() {
  if (!active_) {
    return absl::OkStatus();
  }

  DrawToolbar();
  ImGui::Separator();

  ImGui::Begin("Hex Editor", &active_);
  if (ImGui::Button("Compare Rom")) {
    auto file_name = util::FileDialogWrapper::ShowOpenFileDialog();
    PRINT_IF_ERROR(comparison_rom_.LoadFromFile(file_name));
    show_compare_rom_ = true;
  }

  static uint64_t convert_address = 0;
  gui::InputHex("SNES to PC", (int*)&convert_address, 6, 200.f);
  SameLine();
  Text("%x", SnesToPc(convert_address));

  BEGIN_TABLE("Memory Comparison", 2, ImGuiTableFlags_Resizable);
  SETUP_COLUMN("Source")
  SETUP_COLUMN("Dest")

  NEXT_COLUMN()
  Text("%s", rom()->filename().data());
  memory_widget_.DrawContents(rom()->mutable_data(), rom()->size());

  NEXT_COLUMN()
  if (show_compare_rom_) {
    comparison_widget_.SetComparisonData(rom()->data());
    ImGui::BeginGroup();
    ImGui::BeginChild("Comparison ROM");
    Text("%s", comparison_rom_.filename().data());
    comparison_widget_.DrawContents(comparison_rom_.mutable_data(),
                                    comparison_rom_.size());
    ImGui::EndChild();
    ImGui::EndGroup();
  }
  END_TABLE()

  ImGui::End();

  return absl::OkStatus();
}

void MemoryEditor::DrawToolbar() {
  // Modern compact toolbar with icon-only buttons
  const float pad = gui::LayoutHelpers::GetButtonPadding();
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(pad, pad * 0.67f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(pad * 0.67f, pad * 0.67f));

  if (ImGui::Button(ICON_MD_LOCATION_SEARCHING " Jump")) {
    ImGui::OpenPopup("JumpToAddress");
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Jump to specific address");
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SEARCH " Search")) {
    ImGui::OpenPopup("SearchPattern");
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Search for hex pattern");
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_BOOKMARK " Bookmarks")) {
    ImGui::OpenPopup("Bookmarks");
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Manage address bookmarks");
  }

  ImGui::SameLine();
  ImGui::Text(ICON_MD_MORE_VERT);
  ImGui::SameLine();

  // Show current address
  if (current_address_ != 0) {
    ImGui::TextColored(gui::GetInfoColor(),
                       ICON_MD_LOCATION_ON " 0x%06X", current_address_);
  }

  ImGui::PopStyleVar(2);
  ImGui::Separator();

  DrawJumpToAddressPopup();
  DrawSearchPopup();
  DrawBookmarksPopup();
}

void MemoryEditor::DrawJumpToAddressPopup() {
  if (ImGui::BeginPopupModal("JumpToAddress", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(gui::GetInfoColor(),
                       ICON_MD_LOCATION_SEARCHING " Jump to Address");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##jump_addr", jump_address_,
                         IM_ARRAYSIZE(jump_address_),
                         ImGuiInputTextFlags_CharsHexadecimal |
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
      // Parse and jump on Enter key
      unsigned int addr;
      if (sscanf(jump_address_, "%X", &addr) == 1) {
        current_address_ = addr;
        memory_widget_.GotoAddrAndHighlight(addr, addr + 1);
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::TextDisabled("Format: 0x1C800 or 1C800");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button(ICON_MD_CHECK " Go", ImVec2(120, 0))) {
      unsigned int addr;
      if (sscanf(jump_address_, "%X", &addr) == 1) {
        current_address_ = addr;
        memory_widget_.GotoAddrAndHighlight(addr, addr + 1);
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CANCEL " Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void MemoryEditor::DrawSearchPopup() {
  if (ImGui::BeginPopupModal("SearchPattern", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(gui::GetSuccessColor(),
                       ICON_MD_SEARCH " Search Hex Pattern");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::SetNextItemWidth(300);
    if (ImGui::InputText("##search_pattern", search_pattern_,
                         IM_ARRAYSIZE(search_pattern_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      // TODO: Implement search
      ImGui::CloseCurrentPopup();
    }
    ImGui::TextDisabled("Use ?? for wildcard (e.g. FF 00 ?? 12)");
    ImGui::Spacing();

    // Quick preset patterns
    ImGui::Text(ICON_MD_LIST " Quick Patterns:");
    if (ImGui::SmallButton("LDA")) {
      snprintf(search_pattern_, sizeof(search_pattern_), "A9 ??");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("STA")) {
      snprintf(search_pattern_, sizeof(search_pattern_), "8D ?? ??");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("JSR")) {
      snprintf(search_pattern_, sizeof(search_pattern_), "20 ?? ??");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button(ICON_MD_SEARCH " Search", ImVec2(120, 0))) {
      // TODO: Implement search using hex-search handler
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CANCEL " Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void MemoryEditor::DrawBookmarksPopup() {
  if (ImGui::BeginPopupModal("Bookmarks", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(gui::GetWarningColor(),
                       ICON_MD_BOOKMARK " Memory Bookmarks");
    ImGui::Separator();
    ImGui::Spacing();

    if (bookmarks_.empty()) {
      ImGui::TextDisabled(ICON_MD_INFO " No bookmarks yet");
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      if (ImGui::Button(ICON_MD_ADD " Add Current Address", ImVec2(250, 0))) {
        Bookmark new_bookmark;
        new_bookmark.address = current_address_;
        new_bookmark.name =
            absl::StrFormat("Bookmark %zu", bookmarks_.size() + 1);
        new_bookmark.description = "User-defined bookmark";
        bookmarks_.push_back(new_bookmark);
      }
    } else {
      // Bookmarks table
      ImGui::BeginChild("##bookmarks_list", ImVec2(0, 300), true);
      if (ImGui::BeginTable("##bookmarks_table", 3,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed,
                                100);
        ImGui::TableSetupColumn("Description",
                                ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < bookmarks_.size(); ++i) {
          const auto& bm = bookmarks_[i];
          ImGui::PushID(static_cast<int>(i));

          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          if (ImGui::Selectable(bm.name.c_str(), false,
                                ImGuiSelectableFlags_SpanAllColumns)) {
            current_address_ = bm.address;
            memory_widget_.GotoAddrAndHighlight(bm.address, bm.address + 1);
            ImGui::CloseCurrentPopup();
          }

          ImGui::TableNextColumn();
          ImGui::TextColored(gui::GetInfoColor(), "0x%06X",
                             bm.address);

          ImGui::TableNextColumn();
          ImGui::TextDisabled("%s", bm.description.c_str());

          ImGui::PopID();
        }
        ImGui::EndTable();
      }
      ImGui::EndChild();

      ImGui::Spacing();
      if (ImGui::Button(ICON_MD_ADD " Add Bookmark", ImVec2(150, 0))) {
        Bookmark new_bookmark;
        new_bookmark.address = current_address_;
        new_bookmark.name =
            absl::StrFormat("Bookmark %zu", bookmarks_.size() + 1);
        new_bookmark.description = "User-defined bookmark";
        bookmarks_.push_back(new_bookmark);
      }
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_CLEAR_ALL " Clear All", ImVec2(150, 0))) {
        bookmarks_.clear();
      }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button(ICON_MD_CLOSE " Close", ImVec2(250, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

}  // namespace editor
}  // namespace yaze
