#include "message_editor.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/message/message_list_query.h"
#include "app/editor/message/message_source_sync.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/empty_state.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/file_util.h"
#include "util/hex.h"
#include "util/i18n/tr.h"

namespace yaze::editor {

namespace {

constexpr const char* kNewReviewerPopup = "New reviewer##message_review";

std::optional<std::string> ReadTextFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return std::nullopt;
  }
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
}

// Literal newlines are layout help in the editor only; the encoder ignores
// them, so they do not count as a change to reviewed text.
std::string ReviewTextSha(std::string_view text) {
  std::string flat;
  flat.reserve(text.size());
  for (char c : text) {
    if (c != '\n' && c != '\r') {
      flat.push_back(c);
    }
  }
  return ComputeMessageSourceSha256(flat);
}

std::string NowUtc() {
  return absl::FormatTime("%Y-%m-%dT%H:%M:%SZ", absl::Now(),
                          absl::UTCTimeZone());
}

std::string DisplayPath(const std::filesystem::path& path,
                        const std::optional<std::filesystem::path>& root) {
  if (root.has_value()) {
    std::error_code error;
    const auto relative = std::filesystem::relative(path, *root, error);
    if (!error && !relative.empty() && *relative.begin() != "..") {
      return relative.generic_string();
    }
  }
  return path.string();
}

}  // namespace

std::optional<std::filesystem::path> MessageEditor::ProjectRootPath() const {
  const auto* project = dependencies_.project;
  if (project == nullptr || project->filepath.empty()) {
    return std::nullopt;
  }
  std::error_code error;
  const auto descriptor = std::filesystem::absolute(project->filepath, error);
  if (error) {
    return std::nullopt;
  }
  const auto root = std::filesystem::canonical(descriptor.parent_path(), error);
  if (error) {
    return std::nullopt;
  }
  return root;
}

std::optional<std::filesystem::path> MessageEditor::ReviewDirectory() const {
  if (HasMessageSourceLayout()) {
    const auto bundle = ResolveCanonicalBundlePath();
    if (bundle.ok()) {
      return bundle->parent_path() / "review";
    }
  }
  if (auto root = ProjectRootPath()) {
    return *root / "message_review";
  }
  return std::nullopt;
}

void MessageEditor::EnsureReviewContextLoaded() {
  const std::string project_path =
      dependencies_.project ? dependencies_.project->filepath : std::string();
  if (review_context_loaded_ && project_path == review_context_project_path_) {
    return;
  }
  if (project_path != review_context_project_path_) {
    review_ = {};
    review_path_.clear();
    review_comment_id_ = -1;
    review_status_.clear();
  }
  ReloadReviewContext();
}

void MessageEditor::ReloadReviewContext() {
  review_context_loaded_ = true;
  review_context_project_path_ =
      dependencies_.project ? dependencies_.project->filepath : std::string();
  catalog_.Clear();

  std::vector<std::string> problems;
  if (HasMessageSourceLayout()) {
    const auto bundle = ResolveCanonicalBundlePath();
    if (bundle.ok()) {
      const auto registry_path =
          bundle->parent_path() / "message_registry.json";
      if (auto text = ReadTextFile(registry_path)) {
        auto ranges = ParseMessageRegistry(*text);
        if (ranges.ok()) {
          catalog_.SetRanges(*std::move(ranges));
        } else {
          problems.push_back(std::string(ranges.status().message()));
        }
      }
    }
  }
  if (auto root = ProjectRootPath()) {
    for (const char* relative : {"Core/messages.org", "messages.org"}) {
      if (auto text = ReadTextFile(*root / relative)) {
        catalog_.SetTitles(ParseMessageOrgTitles(*text));
        break;
      }
    }
  }
  catalog_status_ =
      catalog_.empty()
          ? std::string()
          : absl::StrFormat("%zu registry ranges, %zu script titles",
                            catalog_.range_count(), catalog_.title_count());
  if (!problems.empty()) {
    catalog_status_ =
        absl::StrCat(catalog_status_, catalog_status_.empty() ? "" : "; ",
                     absl::StrJoin(problems, "; "));
  }

  if (review_path_.empty()) {
    if (auto directory = ReviewDirectory()) {
      const auto files = ListMessageReviewFiles(*directory);
      if (files.size() == 1) {
        OpenReviewFile(files.front());
      }
    }
  }
}

void MessageEditor::OpenReviewFile(const std::filesystem::path& path) {
  auto loaded = LoadMessageReviewFile(path);
  if (!loaded.ok()) {
    review_status_ = std::string(loaded.status().message());
    review_status_error_ = true;
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show(review_status_, ToastType::kError);
    }
    return;
  }
  review_ = *std::move(loaded);
  if (review_.reviewer.empty()) {
    review_.reviewer = path.stem().string();
  }
  review_path_ = path;
  review_comment_id_ = -1;
  review_status_ = absl::StrFormat("Reviewing as %s (%zu notes)",
                                   review_.reviewer, review_.entries.size());
  review_status_error_ = false;
}

void MessageEditor::CreateReviewFile(const std::string& reviewer) {
  const std::string stem = ReviewFileStem(reviewer);
  const auto directory = ReviewDirectory();
  if (stem.empty() || !directory.has_value()) {
    review_status_ = stem.empty()
                         ? "Enter a reviewer name with letters or digits"
                         : "Open a project to keep review notes";
    review_status_error_ = true;
    return;
  }
  const auto path = *directory / (stem + ".json");
  std::error_code error;
  if (std::filesystem::exists(path, error)) {
    OpenReviewFile(path);
    return;
  }
  review_ = {};
  review_.reviewer = reviewer;
  review_path_ = path;
  review_comment_id_ = -1;
  SaveReview();
}

void MessageEditor::SaveReview() {
  if (review_path_.empty()) {
    return;
  }
  const auto status = SaveMessageReviewFile(review_path_, review_);
  if (!status.ok()) {
    review_status_ = absl::StrFormat("Review not saved: %s", status.message());
    review_status_error_ = true;
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show(review_status_, ToastType::kError);
    }
    return;
  }
  review_status_ = absl::StrFormat(
      "Saved %s at %s", DisplayPath(review_path_, ProjectRootPath()),
      absl::FormatTime("%H:%M", absl::Now(), absl::LocalTimeZone()));
  review_status_error_ = false;
}

void MessageEditor::SetReviewStatus(int display_id, ReviewStatus status) {
  if (review_path_.empty()) {
    return;
  }
  ReviewEntry& entry = review_.entries[display_id];
  entry.status = status;
  if (display_id == review_comment_id_) {
    entry.comment = review_comment_buffer_;
  }
  entry.text_sha256 = ReviewTextSha(ContentsForDisplayId(display_id));
  entry.updated = NowUtc();
  if (entry.empty()) {
    review_.entries.erase(display_id);
  }
  SaveReview();
}

void MessageEditor::CommitReviewComment(int display_id) {
  if (review_path_.empty()) {
    return;
  }
  const auto* existing = review_.Find(display_id);
  const std::string previous = existing ? existing->comment : std::string();
  if (previous == review_comment_buffer_) {
    return;
  }
  ReviewEntry& entry = review_.entries[display_id];
  entry.comment = review_comment_buffer_;
  entry.text_sha256 = ReviewTextSha(ContentsForDisplayId(display_id));
  entry.updated = NowUtc();
  if (entry.empty()) {
    review_.entries.erase(display_id);
  }
  SaveReview();
}

std::string MessageEditor::SceneLabelFor(int display_id) const {
  return catalog_.SceneLabel(display_id);
}

bool MessageEditor::IsDisplayIdEdited(int display_id) const {
  if (display_id < 0 ||
      display_id >= static_cast<int>(baseline_messages_.size()) ||
      display_id >= static_cast<int>(parsed_messages_.size())) {
    return false;
  }
  return baseline_messages_[display_id] != parsed_messages_[display_id];
}

bool MessageEditor::IsReviewStale(int display_id) const {
  const auto* entry = review_.Find(display_id);
  return entry != nullptr && !entry->text_sha256.empty() &&
         entry->text_sha256 != ReviewTextSha(ContentsForDisplayId(display_id));
}

std::vector<int> MessageEditor::EditedDisplayIds() const {
  std::vector<int> ids;
  const int count = static_cast<int>(
      std::min(baseline_messages_.size(), parsed_messages_.size()));
  for (int id = 0; id < count; ++id) {
    if (baseline_messages_[id] != parsed_messages_[id]) {
      ids.push_back(id);
    }
  }
  return ids;
}

bool MessageEditor::RowPassesReviewFilter(const MessageRow& row) const {
  const int id = row.display_id;
  const auto* entry = review_.Find(id);
  const ReviewStatus status = entry ? entry->status : ReviewStatus::kUnreviewed;
  switch (static_cast<ReviewListFilter>(list_review_filter_)) {
    case ReviewListFilter::kToReview:
      return status == ReviewStatus::kUnreviewed &&
             !IsPlaceholderMessageText(ContentsForDisplayId(id));
    case ReviewListFilter::kNeedsChanges:
      return status == ReviewStatus::kNeedsChanges;
    case ReviewListFilter::kApproved:
      return status == ReviewStatus::kApproved;
    case ReviewListFilter::kChangedSinceReview:
      return IsReviewStale(id);
    case ReviewListFilter::kEditedThisSession:
      return IsDisplayIdEdited(id);
    case ReviewListFilter::kPlaceholders:
      return IsPlaceholderMessageText(ContentsForDisplayId(id)) ||
             catalog_.IsReservedSlot(id);
    case ReviewListFilter::kAll:
      break;
  }
  return true;
}

void MessageEditor::DrawReviewListFilter() {
  const char* labels[] = {
      tr("All messages"),
      tr("To review"),
      tr("Needs changes"),
      tr("Approved"),
      tr("Changed since review"),
      tr("Edited this session"),
      tr("Placeholders and reserved"),
  };
  ImGui::SetNextItemWidth(190.0f);
  ImGui::Combo("##message_review_filter", &list_review_filter_, labels,
               IM_ARRAYSIZE(labels));
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "%s", tr("Show messages by review status. Review status comes from "
                 "the review file chosen in Dialogue Review."));
  }
}

void MessageEditor::DrawReviewCell(int display_id) {
  const auto* entry = review_.Find(display_id);
  const bool placeholder =
      IsPlaceholderMessageText(ContentsForDisplayId(display_id));
  const bool stale = IsReviewStale(display_id);
  if (entry && entry->status == ReviewStatus::kApproved) {
    ImGui::TextColored(gui::GetSuccessColor(), ICON_MD_CHECK_CIRCLE);
  } else if (entry && entry->status == ReviewStatus::kNeedsChanges) {
    ImGui::TextColored(gui::GetWarningColor(), ICON_MD_FLAG);
  } else if (entry && !entry->comment.empty()) {
    ImGui::TextColored(gui::GetInfoColor(), ICON_MD_EDIT_NOTE);
  } else if (placeholder) {
    ImGui::TextDisabled(ICON_MD_HOURGLASS_EMPTY);
  } else {
    ImGui::TextUnformatted("");
  }
  if (stale) {
    ImGui::SameLine(0.0f, 2.0f);
    ImGui::TextColored(gui::GetInfoColor(), ICON_MD_UPDATE);
  }
  if (!ImGui::IsItemHovered() || (!entry && !placeholder)) {
    return;
  }
  ImGui::BeginTooltip();
  if (entry) {
    ImGui::TextUnformatted(ReviewStatusLabel(entry->status));
    if (!entry->comment.empty()) {
      ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
      ImGui::TextUnformatted(entry->comment.c_str());
      ImGui::PopTextWrapPos();
    }
    if (stale) {
      ImGui::TextColored(gui::GetInfoColor(), "%s",
                         tr("Text changed since this review"));
    }
  } else {
    ImGui::TextUnformatted(tr("Placeholder: no dialogue written yet"));
  }
  ImGui::EndTooltip();
}

void MessageEditor::DrawReviewerPicker() {
  const auto directory = ReviewDirectory();
  if (!directory.has_value()) {
    ImGui::TextDisabled("%s",
                        tr("Open the project (.yaze) to keep review notes."));
    return;
  }
  bool open_new_popup = false;
  const std::string preview = review_path_.empty()
                                  ? std::string(tr("Choose reviewer..."))
                                  : review_.reviewer;
  ImGui::SetNextItemWidth(
      std::max(160.0f, ImGui::GetContentRegionAvail().x -
                           ImGui::CalcTextSize(tr("Reviewer")).x -
                           ImGui::GetStyle().ItemSpacing.x * 2));
  if (ImGui::BeginCombo(tr("Reviewer"), preview.c_str())) {
    for (const auto& file : ListMessageReviewFiles(*directory)) {
      const std::string stem = file.stem().string();
      if (ImGui::Selectable(stem.c_str(), file == review_path_)) {
        OpenReviewFile(file);
      }
    }
    ImGui::Separator();
    if (ImGui::Selectable(ICON_MD_PERSON_ADD " New reviewer...")) {
      open_new_popup = true;
    }
    ImGui::EndCombo();
  }
  if (open_new_popup) {
    ImGui::OpenPopup(kNewReviewerPopup);
  }
  if (ImGui::BeginPopupModal(kNewReviewerPopup, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    if (ImGui::IsWindowAppearing()) {
      new_reviewer_name_.clear();
      ImGui::SetKeyboardFocusHere();
    }
    ImGui::TextUnformatted(tr("Your name or handle"));
    const bool submitted =
        ImGui::InputText("##new_reviewer_name", &new_reviewer_name_,
                         ImGuiInputTextFlags_EnterReturnsTrue);
    const std::string stem = ReviewFileStem(new_reviewer_name_);
    ImGui::TextDisabled(
        "%s",
        stem.empty()
            ? tr("Notes are saved as <name>.json")
            : absl::StrFormat("%s/%s.json",
                              DisplayPath(*directory, ProjectRootPath()), stem)
                  .c_str());
    ImGui::BeginDisabled(stem.empty());
    const bool create =
        ImGui::Button(tr("Create")) || (submitted && !stem.empty());
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) &&
        stem.empty()) {
      ImGui::SetTooltip("%s", tr("Enter a name with letters or digits"));
    }
    if (create) {
      CreateReviewFile(new_reviewer_name_);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Cancel"))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (!review_path_.empty()) {
    ImGui::TextDisabled("%s",
                        DisplayPath(review_path_, ProjectRootPath()).c_str());
  }
}

void MessageEditor::DrawMessageContext(int display_id) {
  const std::string title = catalog_.TitleFor(display_id);
  const auto* range = catalog_.RangeFor(display_id);
  if (title.empty() && range == nullptr) {
    if (!catalog_.empty()) {
      ImGui::TextDisabled("%s", tr("No registry entry or script title."));
    }
    return;
  }
  if (!title.empty()) {
    ImGui::TextWrapped("%s", title.c_str());
  }
  if (range != nullptr) {
    const bool occupied = range->status == "occupied";
    const ImVec4 color =
        occupied ? gui::GetTextSecondaryVec4() : gui::GetWarningColor();
    if (range->start == range->end) {
      ImGui::TextColored(color, "%s  %s  $%03X", range->owner.c_str(),
                         range->status.c_str(), range->start);
    } else {
      ImGui::TextColored(color, "%s  %s  $%03X-$%03X", range->owner.c_str(),
                         range->status.c_str(), range->start, range->end);
    }
    if (!range->purpose.empty()) {
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextDisabled("%s", range->purpose.c_str());
      ImGui::PopTextWrapPos();
    }
  }
}

void MessageEditor::DrawReviewPanel() {
  EnsureReviewContextLoaded();
  if (list_of_texts_.empty() && expanded_messages_.empty()) {
    gui::DrawEmptyState(gui::EmptyNoRom(/*compact=*/true));
    return;
  }

  DrawReviewerPicker();
  ImGui::Separator();

  const int display_id = CurrentDisplayId();
  ImGui::Text("$%04X", display_id & 0xFFFF);
  ImGui::SameLine();
  gui::StatusBadge(
      current_message_is_expanded_ ? tr("Expanded") : tr("Vanilla"),
      current_message_is_expanded_ ? gui::ButtonType::Warning
                                   : gui::ButtonType::Info);
  if (IsDisplayIdEdited(display_id)) {
    ImGui::SameLine();
    gui::StatusBadge(tr("Edited"), gui::ButtonType::Warning);
  }
  DrawMessageContext(display_id);
  if (IsPlaceholderMessageText(ContentsForDisplayId(display_id))) {
    ImGui::TextColored(gui::GetWarningColor(), "%s",
                       tr("Placeholder: this slot has no dialogue yet."));
  }
  ImGui::Separator();

  if (review_path_.empty()) {
    ImGui::TextWrapped("%s", tr("Choose or create a reviewer above to record "
                                "a status and comment for each message."));
  } else {
    if (review_comment_id_ != display_id) {
      const auto* entry = review_.Find(display_id);
      review_comment_buffer_ = entry ? entry->comment : std::string();
      review_comment_id_ = display_id;
    }
    const auto* entry = review_.Find(display_id);
    const ReviewStatus current =
        entry ? entry->status : ReviewStatus::kUnreviewed;
    for (const ReviewStatus status :
         {ReviewStatus::kUnreviewed, ReviewStatus::kApproved,
          ReviewStatus::kNeedsChanges}) {
      if (status != ReviewStatus::kUnreviewed) {
        ImGui::SameLine();
      }
      if (ImGui::RadioButton(tr(ReviewStatusLabel(status)),
                             current == status) &&
          current != status) {
        SetReviewStatus(display_id, status);
      }
    }
    if (IsReviewStale(display_id)) {
      ImGui::TextColored(gui::GetInfoColor(), ICON_MD_UPDATE " %s",
                         tr("The text changed after this review."));
    }
    ImGui::TextUnformatted(tr("Comment"));
    ImGui::InputTextMultiline("##review_comment", &review_comment_buffer_,
                              ImVec2(-1.0f, ImGui::GetTextLineHeight() * 5.0f));
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      CommitReviewComment(display_id);
    }
    if (entry != nullptr && !entry->updated.empty()) {
      ImGui::TextDisabled(tr("Last reviewed %s"), entry->updated.c_str());
    }
  }
  if (!review_status_.empty()) {
    ImGui::TextColored(review_status_error_ ? gui::GetErrorColor()
                                            : gui::GetTextSecondaryVec4(),
                       "%s", review_status_.c_str());
  }

  ImGui::Separator();
  int approved = 0;
  int needs_changes = 0;
  int stale = 0;
  for (const auto& [id, item] : review_.entries) {
    approved += item.status == ReviewStatus::kApproved;
    needs_changes += item.status == ReviewStatus::kNeedsChanges;
    stale += IsReviewStale(id);
  }
  if (!review_path_.empty()) {
    ImGui::Text(tr("Approved %d  |  Needs changes %d  |  Changed since "
                   "review %d"),
                approved, needs_changes, stale);
  }

  const std::vector<int> edited = EditedDisplayIds();
  if (edited.empty()) {
    ImGui::TextDisabled("%s", tr("No messages edited this session."));
  } else {
    std::vector<std::string> labels;
    labels.reserve(edited.size());
    for (int id : edited) {
      labels.push_back(absl::StrFormat("$%04X", id & 0xFFFF));
    }
    const std::string joined = absl::StrJoin(labels, " ");
    ImGui::Text(tr("Edited this session (%zu):"), edited.size());
    ImGui::SameLine();
    if (ImGui::SmallButton(tr("Copy IDs"))) {
      ImGui::SetClipboardText(joined.c_str());
      if (dependencies_.toast_manager) {
        dependencies_.toast_manager->Show(
            absl::StrFormat("Copied %zu message IDs", edited.size()),
            ToastType::kSuccess);
      }
    }
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", joined.c_str());
    ImGui::PopTextWrapPos();
  }

  const bool can_export = current_parse_errors_.empty();
  ImGui::BeginDisabled(!can_export);
  if (gui::PrimaryButton(ICON_MD_IOS_SHARE " Export for review")) {
    ExportForReview();
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip(
        "%s", can_export
                  ? tr("Save every loaded message to a separate bundle file. "
                       "Send it with your review file and the edited IDs.")
                  : tr("Resolve message parse errors before exporting."));
  }
  if (!message_bundle_status_.empty()) {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(message_bundle_status_error_
                           ? gui::GetErrorColor()
                           : gui::GetTextSecondaryVec4(),
                       "%s", message_bundle_status_.c_str());
    ImGui::PopTextWrapPos();
  }
  if (!catalog_status_.empty()) {
    ImGui::TextDisabled("%s", catalog_status_.c_str());
  }
}

void MessageEditor::ExportForReview() {
  const auto path = util::FileDialogWrapper::ShowSaveFileDialog();
  if (path.empty()) {
    return;
  }
  const auto status = ExportReviewBundleToFile(path);
  message_bundle_status_error_ = !status.ok();
  message_bundle_status_ =
      status.ok() ? absl::StrFormat("Exported bundle: %s", path)
                  : absl::StrFormat("Export failed: %s", status.message());
  if (dependencies_.toast_manager) {
    dependencies_.toast_manager->Show(
        message_bundle_status_,
        status.ok() ? ToastType::kSuccess : ToastType::kError);
  }
}

absl::Status MessageEditor::ExportReviewBundleToFile(
    const std::string& path) const {
  if (!current_parse_errors_.empty()) {
    return absl::FailedPreconditionError(
        "Resolve message parse errors before exporting for review");
  }
  if (IsCanonicalBundlePath(path)) {
    return absl::PermissionDeniedError(
        "Export for review requires a separate file. Use Preview and Publish "
        "to update the canonical source.");
  }
  return ExportMessageBundleToJson(path, list_of_texts_, expanded_messages_);
}

}  // namespace yaze::editor
