#ifndef YAZE_APP_EDITOR_AGENT_UI_REVIEW_BOARDS_REVIEW_BOARDS_VIEW_H_
#define YAZE_APP_EDITOR_AGENT_UI_REVIEW_BOARDS_REVIEW_BOARDS_VIEW_H_

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/time/time.h"
#include "app/editor/agent/review_board.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gfx/core/bitmap.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

/// Design boards agents want reviewed: structured v2 questions, images, notes,
/// and pinned comments. yaze writes only feedback-<id>.json next to each board.
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
  float GetPreferredWidth() const override { return 1080.0f; }
  float GetPreferredHeight() const override { return 720.0f; }
  void Draw(bool* p_open) override;

  // Command palette & automation entry points.
  void ForceRefresh();
  void SelectNextWaiting();
  void SelectBoardById(const std::string& id);
  void SetBoardsDirectory(const std::filesystem::path& dir);
  void SetDraftAnswer(const std::string& board_id,
                      const std::string& question_id, ReviewBoardAnswer answer);
  bool IsDraftStaleAfterRepublish(const std::string& board_id) const;
  void ReloadDraftFromBoard(const ReviewBoard& board);
  void KeepDraftEditsOnUpdatedBoard(const ReviewBoard& board);

  const std::string& selected_id() const { return selected_id_; }
  ReviewBoardFilter filter() const { return filter_; }
  ReviewBoardSort sort_mode() const { return sort_; }
  const ReviewBoardScan& scan() const { return scan_; }
  const std::vector<int>& visible_indices() const { return visible_; }
  bool HasUnsavedDraft(const ReviewBoard& board) const;
  const ReviewBoardAnswerMap* DraftAnswersFor(
      const std::string& board_id) const;

 private:
  struct BoardDraft {
    ReviewBoardAnswerMap answers;
    ReviewBoardAnswerMap baseline_answers;
    std::unordered_map<std::string, bool> single_other_selected;
    std::unordered_map<std::string, std::string> number_inputs;
    std::string decision = "looks_good";
    std::string baseline_decision = "looks_good";
    std::string draft_board_sha256;
    int category = 0;  // 0 general, 1 layout
    char comment[2048] = {};
    bool initialized = false;
    bool stale_after_republish = false;
  };

  void Rescan(bool reset_watcher);
  void PollIncrementalRescan();
  void SyncCleanDraftsAfterScan();
  void RefreshVisible(const std::string& keep_id);
  bool MatchesBoard(const ReviewBoard& board, ReviewBoardFilter filter) const;
  ReviewBoard* Selected();
  const ReviewBoard* Selected() const;
  void Select(const std::string& id);
  BoardDraft& EnsureDraft(const ReviewBoard& board);

  void EnsureImagesLoaded(const ReviewBoard& board);
  void ReleaseImages();
  void OpenImageViewer(int media_index);
  void DrawImageViewerModal(const ReviewBoard& board);

  void DrawToolbar();
  void DrawList();
  void DrawDetail(ReviewBoard& board);
  void DrawQuestions(const ReviewBoard& board, BoardDraft& draft);
  void DrawOptionThumbnail(const ReviewBoard& board, int media_index);
  void DrawMedia(const ReviewBoard& board);
  void DrawConversation(const ReviewBoard& board);
  void DrawReply(ReviewBoard& board);
  void SubmitFeedback(ReviewBoard& board, BoardDraft& draft);
  void NotifyToast(const std::string& message, bool is_error);

  std::filesystem::path dir_;
  std::string dir_error_;
  bool scanned_ = false;
  absl::Time last_rescan_at_ = absl::InfinitePast();
  ReviewBoardScan scan_;
  ReviewBoardWatcherState watcher_state_;

  ReviewBoardFilter filter_ = ReviewBoardFilter::kWaitingOnYou;
  bool force_select_filter_tab_ = false;
  ReviewBoardSort sort_ = ReviewBoardSort::kNewest;
  char search_query_[256] = {};
  std::string group_filter_;
  std::string author_filter_;
  std::vector<int> visible_;  // sorted indices into scan_.boards
  std::string selected_id_;

  std::unordered_map<std::string, BoardDraft> drafts_;

  // Textures for the selected board; rebuilt when id + board sha change.
  std::string images_for_;
  std::vector<std::unique_ptr<gfx::Bitmap>> images_;
  std::vector<std::string> image_errors_;
  bool fit_width_ = true;

  // Full-image modal viewer state.
  bool open_image_viewer_popup_ = false;
  bool image_viewer_open_ = false;
  int viewer_media_index_ = 0;
  float viewer_zoom_ = 0.0f;  // 0 = fit window, >0 = explicit scale

  std::string status_for_;
  std::string status_;
  bool status_is_error_ = false;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_AGENT_UI_REVIEW_BOARDS_REVIEW_BOARDS_VIEW_H_
