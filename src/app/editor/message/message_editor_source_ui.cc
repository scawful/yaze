#include "message_editor.h"

#include <string>

#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui.h"
#include "imgui/imgui_internal.h"
#include "util/i18n/tr.h"

namespace yaze::editor {

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

void MessageEditor::DrawExpandedSourceBanner() {
  if (!expanded_messages_.empty()) {
    return;
  }
  ImGui::TextWrapped("%s", tr("Expanded messages not loaded"));
  if (ImGui::Button(tr("Load canonical source"))) {
    if (dependencies_.window_manager) {
      dependencies_.window_manager->OpenWindow(dependencies_.session_id,
                                               "message.expanded_source");
    }
    if (HasMessageSourceLayout()) {
      LoadCanonicalExpandedSourceFromUi();
    }
  }
}

void MessageEditor::LoadCanonicalExpandedSourceFromUi() {
  const auto status = LoadCanonicalExpandedSource();
  if (!status.ok()) {
    message_bundle_status_ = std::string(status.message());
    message_bundle_status_error_ = true;
  }
}

}  // namespace yaze::editor
