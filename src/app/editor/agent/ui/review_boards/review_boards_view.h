#ifndef YAZE_APP_EDITOR_AGENT_UI_REVIEW_BOARDS_REVIEW_BOARDS_VIEW_H_
#define YAZE_APP_EDITOR_AGENT_UI_REVIEW_BOARDS_REVIEW_BOARDS_VIEW_H_

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "app/editor/agent/review_board.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gfx/core/bitmap.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

/// Design boards agents want reviewed: images, notes and pinned comments.
/// The user answers with "Looks good", "Needs changes" or a comment; yaze
/// writes only feedback-<id>.json next to each board.
class ReviewBoardsView : public WindowContent {
 public:
  ReviewBoardsView() = default;
  ~ReviewBoardsView() override;

  std::string GetId() const override { return "agent.review_boards"; }
  std::string GetDisplayName() const override { return "Review Boards"; }
  std::string GetIcon() const override { return ICON_MD_RATE_REVIEW; }
  std::string GetEditorCategory() const override { return "Agent"; }
  WindowScope GetScope() const override { return WindowScope::kGlobal; }
  WindowPresentationPolicy GetPresentationPolicy() const override {
    return WindowPresentationPolicy::OptionalPopOut();
  }
  int GetPriority() const override { return 15; }
  float GetPreferredWidth() const override { return 720.0f; }
  float GetPreferredHeight() const override { return 640.0f; }
  void Draw(bool* p_open) override;

 private:
  enum class Filter { kNeedsReview, kCurrent, kSuperseded, kStale, kAll };

  void Rescan();
  // Rebuilds visible_ for filter_; keep_id stays listed even if answered.
  void RefreshVisible(const std::string& keep_id);
  bool Matches(const ReviewBoard& board, Filter filter) const;
  ReviewBoard* Selected();
  void Select(const std::string& id);
  void SelectNextNeedingReview();

  void DrawToolbar();
  void DrawList();
  void DrawDetail(ReviewBoard& board);
  void DrawMedia(const ReviewBoard& board);
  void DrawConversation(const ReviewBoard& board);
  void DrawReply(ReviewBoard& board);
  void Answer(ReviewBoard& board, const char* decision);

  void ReleaseImages();

  std::filesystem::path dir_;
  std::string dir_error_;
  bool scanned_ = false;
  ReviewBoardScan scan_;
  Filter filter_ = Filter::kNeedsReview;
  std::vector<int> visible_;  // indices into scan_.boards for filter_
  std::string selected_id_;

  // Textures for the selected board; rebuilt when id + board sha change.
  std::string images_for_;
  std::vector<std::unique_ptr<gfx::Bitmap>> images_;
  std::vector<std::string> image_errors_;
  bool fit_width_ = true;

  char comment_[1024] = {};
  int category_ = 0;  // 0 general, 1 layout
  std::string status_for_;
  std::string status_;
  bool status_is_error_ = false;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_AGENT_UI_REVIEW_BOARDS_REVIEW_BOARDS_VIEW_H_
