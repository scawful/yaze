#include "app/editor/agent/ui/review_boards/review_boards_view.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/core/panel_registration.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/empty_state.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "util/i18n/translator.h"

namespace yaze::editor {
namespace {

using i18n::tr;

constexpr const char* kFilterLabels[] = {"Needs review", "Current",
                                         "Superseded", "Stale", "All"};
constexpr const char* kFilterIcons[] = {ICON_MD_RADIO_BUTTON_UNCHECKED,
                                        ICON_MD_RATE_REVIEW, ICON_MD_HISTORY,
                                        ICON_MD_SCHEDULE, ICON_MD_INBOX};
constexpr const char* kCategories[] = {"general", "layout"};

bool IsStale(const ReviewBoard& board) {
  return absl::StartsWith(board.status, "stale");
}

// One icon, color and label per board state. Material icons only: the list,
// the conversation and the answer bar all read from here.
struct StateLook {
  const char* icon;
  ImVec4 color;
  const char* label;
};

StateLook DecisionLook(const std::string& decision) {
  if (decision == "looks_good")
    return {ICON_MD_CHECK_CIRCLE, gui::GetSuccessColor(), "Looks good"};
  if (decision == "needs_changes")
    return {ICON_MD_EDIT_NOTE, gui::GetWarningColor(), "Needs changes"};
  return {ICON_MD_CHAT_BUBBLE_OUTLINE, gui::GetInfoColor(), "Comment"};
}

StateLook BoardLook(const ReviewBoard& board, const std::string& decision) {
  if (!decision.empty())
    return DecisionLook(decision);
  if (board.status == "current")
    return {ICON_MD_RADIO_BUTTON_UNCHECKED, gui::GetAccentColor(),
            "Needs review"};
  if (IsStale(board))
    return {ICON_MD_SCHEDULE, gui::GetTextDisabledVec4(), "Stale"};
  return {ICON_MD_HISTORY, gui::GetTextDisabledVec4(), "Superseded"};
}

std::string SubjectLine(const ReviewBoard& board) {
  if (board.subject_kind.empty())
    return {};
  std::string kind = board.subject_kind;
  kind[0] = static_cast<char>(std::toupper(kind[0]));
  std::string line = kind;
  if (!board.subject_id.empty())
    line += " " + board.subject_id;
  if (!board.subject_label.empty())
    line += " - " + board.subject_label;
  return line;
}

}  // namespace

ReviewBoardsView::~ReviewBoardsView() {
  ReleaseImages();
}

void ReviewBoardsView::ReleaseImages() {
  for (auto& bitmap : images_) {
    if (bitmap)
      gfx::Arena::Get().RetireBitmap(*bitmap);
  }
  images_.clear();
  image_errors_.clear();
  images_for_.clear();
}

void ReviewBoardsView::Rescan() {
  scanned_ = true;
  dir_error_.clear();
  if (dir_.empty()) {
    auto dir = DefaultReviewBoardsDirectory();
    if (!dir.ok()) {
      dir_error_ = std::string(dir.status().message());
      scan_ = {};
      visible_.clear();
      return;
    }
    dir_ = *dir;
  }
  scan_ = ScanReviewBoards(dir_);
  // A board whose sha changed gets new textures on the next draw.
  ReleaseImages();
  RefreshVisible({});
  if (!Selected() && !visible_.empty())
    selected_id_ = scan_.boards[visible_.front()].id;
}

void ReviewBoardsView::RefreshVisible(const std::string& keep_id) {
  visible_.clear();
  for (int i = 0; i < static_cast<int>(scan_.boards.size()); ++i) {
    if (Matches(scan_.boards[i], filter_) ||
        (!keep_id.empty() && scan_.boards[i].id == keep_id)) {
      visible_.push_back(i);
    }
  }
}

bool ReviewBoardsView::Matches(const ReviewBoard& board, Filter filter) const {
  switch (filter) {
    case Filter::kNeedsReview:
      return board.status == "current" && ReviewBoardDecision(board).empty();
    case Filter::kCurrent:
      return board.status == "current";
    case Filter::kSuperseded:
      return board.status == "superseded";
    case Filter::kStale:
      return IsStale(board);
    case Filter::kAll:
      return true;
  }
  return true;
}

ReviewBoard* ReviewBoardsView::Selected() {
  for (auto& board : scan_.boards) {
    if (board.id == selected_id_)
      return &board;
  }
  return nullptr;
}

void ReviewBoardsView::Select(const std::string& id) {
  if (id == selected_id_)
    return;
  selected_id_ = id;
  comment_[0] = '\0';
  status_.clear();
}

void ReviewBoardsView::SelectNextNeedingReview() {
  const auto& boards = scan_.boards;
  int start = 0;
  for (int i = 0; i < static_cast<int>(boards.size()); ++i) {
    if (boards[i].id == selected_id_) {
      start = i + 1;
      break;
    }
  }
  for (int n = 0; n < static_cast<int>(boards.size()); ++n) {
    const auto& board = boards[(start + n) % boards.size()];
    if (Matches(board, Filter::kNeedsReview)) {
      Select(board.id);
      return;
    }
  }
}

void ReviewBoardsView::Draw(bool* /*p_open*/) {
  if (!scanned_)
    Rescan();
  DrawToolbar();
  ImGui::Separator();

  if (!dir_error_.empty()) {
    gui::EmptyStateOptions empty;
    empty.icon = ICON_MD_FOLDER_OPEN;
    empty.title = tr("Review boards folder unavailable");
    empty.detail = dir_error_.c_str();
    empty.action_label = tr("Retry");
    empty.on_action = [this]() {
      Rescan();
    };
    gui::DrawEmptyState(empty);
    return;
  }
  if (scan_.boards.empty()) {
    const std::string detail = absl::StrFormat(
        "Agents add boards with yaze_editor_control.py board-write. "
        "Folder: %s",
        dir_.string());
    gui::EmptyStateOptions empty;
    empty.icon = ICON_MD_RATE_REVIEW;
    empty.title = tr("No review boards yet");
    empty.detail = detail.c_str();
    empty.action_label = tr("Refresh");
    empty.on_action = [this]() {
      Rescan();
    };
    gui::DrawEmptyState(empty);
    return;
  }

  const float list_width = std::max(200.0f, ImGui::GetFontSize() * 17.0f);
  if (ImGui::BeginChild("##boards_list", ImVec2(list_width, 0),
                        ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX)) {
    DrawList();
  }
  ImGui::EndChild();
  ImGui::SameLine();
  // The answer bar stays pinned under the scrolling detail, so the user can
  // answer without scrolling past images and comments.
  ImGui::BeginGroup();
  ReviewBoard* board = Selected();
  const ImGuiStyle& style = ImGui::GetStyle();
  const float reply_height =
      board
          ? style.ItemSpacing.y + 1.0f +                     // separator
                ImGui::GetFrameHeightWithSpacing() * 2.0f +  // header, buttons
                ImGui::GetTextLineHeight() * 3.0f +
                style.FramePadding.y * 2.0f + style.ItemSpacing.y +  // comment
                ImGui::GetTextLineHeightWithSpacing()  // status line
          : 0.0f;
  if (ImGui::BeginChild("##board_detail", ImVec2(0, -reply_height))) {
    if (board) {
      DrawDetail(*board);
    } else {
      gui::DrawEmptyState(gui::EmptyNoSelection());
    }
  }
  ImGui::EndChild();
  if (board) {
    DrawReply(*board);
  }
  ImGui::EndGroup();
}

void ReviewBoardsView::DrawToolbar() {
  int counts[5] = {};
  for (const auto& board : scan_.boards) {
    for (int f = 0; f < 5; ++f) {
      if (Matches(board, static_cast<Filter>(f)))
        ++counts[f];
    }
  }
  const int current = static_cast<int>(filter_);
  const std::string preview =
      absl::StrFormat("%s %s (%d)", kFilterIcons[current],
                      tr(kFilterLabels[current]), counts[current]);
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 11.0f);
  if (ImGui::BeginCombo("##board_filter", preview.c_str())) {
    for (int f = 0; f < 5; ++f) {
      const std::string label = absl::StrFormat(
          "%s %s (%d)", kFilterIcons[f], tr(kFilterLabels[f]), counts[f]);
      if (ImGui::Selectable(label.c_str(), f == static_cast<int>(filter_))) {
        filter_ = static_cast<Filter>(f);
        RefreshVisible({});
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  if (gui::ThemedIconButton(ICON_MD_REFRESH,
                            tr("Read the boards folder again"))) {
    Rescan();
  }
  ImGui::SameLine();
  ImGui::TextDisabled("%s", dir_.string().c_str());
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s",
                      tr("yaze writes only feedback-<id>.json in each board "
                         "folder. Boards come from agents."));
  }
  if (!scan_.errors.empty()) {
    ImGui::SameLine();
    gui::StyleColorGuard warn(ImGuiCol_Text, gui::GetWarningColor());
    ImGui::Text("%s %d skipped", ICON_MD_WARNING,
                static_cast<int>(scan_.errors.size()));
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      for (size_t i = 0; i < scan_.errors.size() && i < 12; ++i)
        ImGui::TextUnformatted(scan_.errors[i].c_str());
      if (scan_.errors.size() > 12)
        ImGui::Text("...and %d more",
                    static_cast<int>(scan_.errors.size() - 12));
      ImGui::EndTooltip();
    }
  }
}

void ReviewBoardsView::DrawList() {
  if (visible_.empty()) {
    if (filter_ == Filter::kNeedsReview) {
      gui::StyleColorGuard done(ImGuiCol_Text, gui::GetSuccessColor());
      ImGui::TextWrapped("%s %s", ICON_MD_TASK_ALT,
                         tr("Every current board has an answer."));
    } else {
      ImGui::TextDisabled("%s", tr("No boards in this view."));
    }
    return;
  }
  ImDrawList* draw = ImGui::GetWindowDrawList();
  // Fixed icon column so titles line up whatever the glyph width.
  const float title_x =
      ImGui::GetFontSize() * 1.25f + ImGui::GetStyle().ItemInnerSpacing.x;
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(visible_.size()));
  while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
      const ReviewBoard& board = scan_.boards[visible_[row]];
      const StateLook look = BoardLook(board, ReviewBoardDecision(board));
      const std::string& title = board.title.empty() ? board.id : board.title;
      ImGui::PushID(visible_[row]);
      const ImVec2 pos = ImGui::GetCursorScreenPos();
      if (ImGui::Selectable("##board", board.id == selected_id_)) {
        Select(board.id);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s %s\n%s\n%s", look.icon, tr(look.label),
                          board.group.c_str(), board.id.c_str());
      }
      // Only the icon takes the state color; old boards dim the title too.
      draw->AddText(pos, ImGui::GetColorU32(look.color), look.icon);
      draw->AddText(
          ImVec2(pos.x + title_x, pos.y),
          ImGui::GetColorU32(board.status == "current" ? ImGuiCol_Text
                                                       : ImGuiCol_TextDisabled),
          title.c_str());
      ImGui::PopID();
    }
  }
}

void ReviewBoardsView::DrawDetail(ReviewBoard& board) {
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(board.title.empty() ? board.id.c_str()
                                             : board.title.c_str());
  ImGui::PopTextWrapPos();
  const std::string subject = SubjectLine(board);
  if (!subject.empty()) {
    ImGui::TextDisabled("%s %s", ICON_MD_PLACE, subject.c_str());
  }
  ImGui::TextDisabled("%s  %s  %s", board.author.c_str(),
                      board.created_at.c_str(), board.group.c_str());

  if (board.status != "current") {
    gui::StyleColorGuard warn(ImGuiCol_Text, gui::GetWarningColor());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::Text("%s %s%s%s", ICON_MD_HISTORY, board.status.c_str(),
                board.status_reason.empty() ? "" : ": ",
                board.status_reason.c_str());
    ImGui::PopTextWrapPos();
  }
  auto link_row = [this](const char* label,
                         const std::vector<std::string>& ids) {
    if (ids.empty())
      return;
    ImGui::TextDisabled("%s", label);
    for (const auto& id : ids) {
      ImGui::SameLine();
      if (ImGui::SmallButton(id.c_str())) {
        // Only the view changes; `board` stays valid (no rescan here).
        filter_ = Filter::kAll;
        RefreshVisible({});
        Select(id);
      }
    }
  };
  link_row(tr("Replaces:"), board.supersedes);
  link_row(tr("Replaced by:"), board.superseded_by);

  if (!board.feedback_error.empty()) {
    gui::StyleColorGuard error(ImGuiCol_Text, gui::GetErrorColor());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::Text("%s %s", ICON_MD_ERROR, board.feedback_error.c_str());
    ImGui::PopTextWrapPos();
  }

  ImGui::Separator();
  if (!board.why.empty()) {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(board.why.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
  }
  DrawMedia(board);
  if (!board.details.empty() &&
      ImGui::CollapsingHeader(tr("Details"), ImGuiTreeNodeFlags_None)) {
    for (const auto& [label, text] : board.details) {
      ImGui::TextDisabled("%s", label.c_str());
      ImGui::SameLine();
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextUnformatted(text.c_str());
      ImGui::PopTextWrapPos();
    }
  }
  DrawConversation(board);
}

void ReviewBoardsView::DrawMedia(const ReviewBoard& board) {
  const std::string key = board.id + "@" + board.board_sha256;
  if (key != images_for_) {
    ReleaseImages();
    images_for_ = key;
    for (size_t i = 0; i < board.media.size(); ++i) {
      auto bitmap = std::make_unique<gfx::Bitmap>();
      std::string error;
      if (board.media[i].kind == "image") {
        auto image = LoadReviewBoardImage(board, i);
        if (image.ok() && image->rgba) {
          bitmap->Create(image->width, image->height, 32, *image->rgba);
          gfx::Arena::Get().QueueTextureCommand(
              gfx::Arena::TextureCommandType::CREATE, bitmap.get());
        } else if (!image.ok()) {
          error = std::string(image.status().message());
        }
      }
      images_.push_back(std::move(bitmap));
      image_errors_.push_back(std::move(error));
    }
  }
  if (board.media.empty())
    return;

  ImGui::Checkbox(tr("Fit width"), &fit_width_);
  for (size_t i = 0; i < board.media.size(); ++i) {
    const auto& media = board.media[i];
    ImGui::PushID(static_cast<int>(i));
    if (media.kind != "image") {
      ImGui::TextDisabled("%s %s", ICON_MD_VIDEOCAM, media.path.c_str());
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("Videos are listed, not played, in yaze."));
    } else if (!image_errors_[i].empty()) {
      gui::StyleColorGuard error(ImGuiCol_Text, gui::GetErrorColor());
      ImGui::PushTextWrapPos(0.0f);
      ImGui::Text("%s %s: %s", ICON_MD_BROKEN_IMAGE, media.path.c_str(),
                  image_errors_[i].c_str());
      ImGui::PopTextWrapPos();
    } else if (images_[i] && images_[i]->texture()) {
      const float w = static_cast<float>(images_[i]->width());
      const float h = static_cast<float>(images_[i]->height());
      float scale = 1.0f;
      if (fit_width_ && w > 0.0f)
        // Small pixel art scales up too; off shows actual pixels.
        scale = std::min(4.0f, ImGui::GetContentRegionAvail().x / w);
      const ImVec2 size(w * scale, h * scale);
      const ImVec2 origin = ImGui::GetCursorScreenPos();
      ImGui::Image((ImTextureID)(intptr_t)images_[i]->texture(), size);
      // Dashboard pins are placed on the first image.
      if (i == 0) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImU32 fill = ImGui::GetColorU32(gui::GetAccentColor());
        const ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
        const ImU32 back = ImGui::GetColorU32(ImGuiCol_WindowBg);
        const float radius = ImGui::GetFontSize() * 0.6f;
        int number = 0;
        for (const auto& comment : board.comments) {
          ++number;
          if (!comment.has_pin)
            continue;
          const ImVec2 at(origin.x + comment.pin_x * size.x,
                          origin.y + comment.pin_y * size.y);
          draw->AddCircleFilled(at, radius + 1.5f, back);
          draw->AddCircleFilled(at, radius, fill);
          const std::string n = std::to_string(number);
          const ImVec2 ts = ImGui::CalcTextSize(n.c_str());
          draw->AddText(ImVec2(at.x - ts.x * 0.5f, at.y - ts.y * 0.5f), text,
                        n.c_str());
        }
      }
    } else {
      ImGui::TextDisabled("%s %s", ICON_MD_IMAGE, tr("Loading image..."));
    }
    if (!media.caption.empty()) {
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextDisabled("%s", media.caption.c_str());
      ImGui::PopTextWrapPos();
    }
    ImGui::Spacing();
    ImGui::PopID();
  }
}

void ReviewBoardsView::DrawConversation(const ReviewBoard& board) {
  if (board.comments.empty() && board.feedback.empty())
    return;
  ImGui::SeparatorText(tr("Comments"));
  int number = 0;
  for (const auto& comment : board.comments) {
    ++number;
    if (comment.has_pin) {
      ImGui::TextDisabled("%s%d", ICON_MD_PLACE, number);
    } else {
      ImGui::TextDisabled("%s", ICON_MD_SPEAKER_NOTES);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s %s", comment.author.c_str(), comment.at.c_str());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(comment.text.c_str());
    ImGui::PopTextWrapPos();
  }
  for (const auto& entry : board.feedback) {
    const StateLook look = DecisionLook(entry.decision);
    {
      gui::StyleColorGuard color(ImGuiCol_Text, look.color);
      ImGui::Text("%s %s", look.icon, tr(look.label));
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s  %s", entry.category.c_str(), entry.at_utc.c_str());
    if (!entry.comment.empty()) {
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextUnformatted(entry.comment.c_str());
      ImGui::PopTextWrapPos();
    }
  }
}

void ReviewBoardsView::DrawReply(ReviewBoard& board) {
  ImGui::Separator();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(tr("Your answer"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.0f);
  ImGui::Combo("##board_kind", &category_, kCategories, 2);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tr("Kind of feedback: general or layout."));
  }
  const std::string last = ReviewBoardDecision(board);
  if (!last.empty()) {
    const StateLook look = DecisionLook(last);
    ImGui::SameLine();
    {
      gui::StyleColorGuard color(ImGuiCol_Text, look.color);
      ImGui::Text("%s %s: %s", look.icon, tr("Current answer"), tr(look.label));
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", tr("The newest Looks good or Needs changes "
                                 "answer counts. Comments do not change it."));
    }
  }
  ImGui::InputTextMultiline("##board_comment", comment_, sizeof(comment_),
                            ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 3));

  const bool has_comment = comment_[0] != '\0';
  if (gui::SuccessButton(
          absl::StrFormat("%s %s", ICON_MD_CHECK_CIRCLE, tr("Looks good, next"))
              .c_str())) {
    Answer(board, "looks_good");
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tr("Saves Looks good (and the comment, if any), "
                               "then opens the next board needing review."));
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!has_comment);
  if (gui::ThemedButton(
          absl::StrFormat("%s %s", ICON_MD_EDIT_NOTE, tr("Needs changes"))
              .c_str())) {
    Answer(board, "needs_changes");
  }
  ImGui::EndDisabled();
  if (!has_comment &&
      ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("%s", tr("Write what should change first."));
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!has_comment);
  if (gui::ThemedButton(
          absl::StrFormat("%s %s", ICON_MD_CHAT_BUBBLE_OUTLINE, tr("Comment"))
              .c_str())) {
    Answer(board, "comment");
  }
  ImGui::EndDisabled();
  if (!has_comment &&
      ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("%s", tr("Write a comment first."));
  }

  if (!status_.empty() && status_for_ == board.id) {
    gui::StyleColorGuard color(ImGuiCol_Text, status_is_error_
                                                  ? gui::GetErrorColor()
                                                  : gui::GetSuccessColor());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::Text("%s %s", status_is_error_ ? ICON_MD_ERROR : ICON_MD_CHECK,
                status_.c_str());
    ImGui::PopTextWrapPos();
  }
}

void ReviewBoardsView::Answer(ReviewBoard& board, const char* decision) {
  ReviewBoardFeedbackEntry entry;
  entry.decision = decision;
  entry.category = kCategories[category_];
  entry.comment = comment_;
  entry.at_utc =
      absl::FormatTime("%Y-%m-%dT%H:%M:%SZ", absl::Now(), absl::UTCTimeZone());
  const std::string id = board.id;
  const absl::Status status = WriteReviewBoardFeedback(board, entry);
  if (!status.ok()) {
    status_for_ = id;
    status_ = std::string(status.message());
    status_is_error_ = true;
    return;
  }
  comment_[0] = '\0';
  const std::string saved = absl::StrFormat(
      "Saved \"%s\" to %s", tr(DecisionLook(entry.decision).label),
      ReviewBoardFeedbackPath(board).filename().string());
  if (entry.decision == "looks_good") {
    SelectNextNeedingReview();
  }
  // The answered board stays listed until Refresh, so the row does not jump.
  RefreshVisible(id);
  status_for_ = selected_id_;
  status_ = selected_id_ == id ? saved : saved + " - opened the next board";
  status_is_error_ = false;
}

REGISTER_PANEL(ReviewBoardsView);

}  // namespace yaze::editor
