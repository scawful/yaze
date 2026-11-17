#ifndef YAZE_APP_EDITOR_AGENT_UI_REVIEW_BOARDS_REVIEW_BOARDS_VIEW_H_
#define YAZE_APP_EDITOR_AGENT_UI_REVIEW_BOARDS_REVIEW_BOARDS_VIEW_H_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/time/time.h"
#include "app/editor/agent/review_board.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gfx/core/bitmap.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze::editor {

inline constexpr float kReviewBoardViewerMinZoom = 0.10f;    // 10% physical
inline constexpr float kReviewBoardViewerMaxZoom = 16.0f;    // 1600% physical
inline constexpr float kReviewBoardViewerWheelStep = 1.25f;  // x1.25 per notch

struct ReviewBoardViewerState {
  bool fit_mode = true;
  float zoom = 1.0f;  // Physical pixels per image pixel (1.0 = 100%).
  ImVec2 pan = ImVec2(0.0f, 0.0f);  // Logical offset from canvas top-left.
};

struct ReviewBoardViewerLayout {
  float physical_zoom = 1.0f;
  ImVec2 logical_scale = ImVec2(1.0f, 1.0f);
  ImVec2 drawn_logical_size = ImVec2(0.0f, 0.0f);
  int target_physical_width = 1;
  int target_physical_height = 1;
  ImVec2 pan = ImVec2(0.0f, 0.0f);
  bool use_nearest_sampling = true;
  int zoom_percent = 100;
};

struct ReviewBoardRgbaMip {
  int width = 0;
  int height = 0;
  std::shared_ptr<const std::vector<uint8_t>> rgba;
};

// Pure helpers for DPI-aware zoom/pan geometry and area-filtered RGBA
// downscaling (unit-tested directly in review_board_test.cc).
ImVec2 SanitizeReviewBoardFramebufferScale(ImVec2 fb_scale);
float ComputeReviewBoardFitZoom(ImVec2 image_size, ImVec2 avail_logical,
                                ImVec2 fb_scale);
ImVec2 ClampReviewBoardViewerPan(ImVec2 pan, ImVec2 drawn_logical_size,
                                 ImVec2 avail_logical);
ReviewBoardViewerLayout ResolveReviewBoardViewerLayout(
    const ReviewBoardViewerState& state, ImVec2 image_size,
    ImVec2 avail_logical, ImVec2 fb_scale);
ReviewBoardViewerState ZoomReviewBoardAroundScreenPos(
    const ReviewBoardViewerState& state, float target_physical_zoom,
    ImVec2 anchor_screen_pos, ImVec2 canvas_p0, ImVec2 image_size,
    ImVec2 avail_logical, ImVec2 fb_scale);
ReviewBoardViewerState StepReviewBoardWheelZoom(
    const ReviewBoardViewerState& state, float wheel_y,
    ImVec2 anchor_screen_pos, ImVec2 canvas_p0, ImVec2 image_size,
    ImVec2 avail_logical, ImVec2 fb_scale);
ReviewBoardViewerState ToggleReviewBoardViewerFitOrActual(
    const ReviewBoardViewerState& state, ImVec2 anchor_screen_pos,
    ImVec2 canvas_p0, ImVec2 image_size, ImVec2 avail_logical, ImVec2 fb_scale);
bool ShouldHandleReviewBoardCanvasWheel(bool canvas_hovered,
                                        bool want_text_input, float wheel_y);

std::vector<uint8_t> BoxDownscaleRgba(std::span<const uint8_t> src_rgba,
                                      int src_w, int src_h, int dst_w,
                                      int dst_h);
std::vector<ReviewBoardRgbaMip> BuildReviewBoardMipChain(
    int width, int height, std::shared_ptr<const std::vector<uint8_t>> rgba);
std::vector<uint8_t> DownscaleReviewBoardFromMips(
    const std::vector<ReviewBoardRgbaMip>& mips, int dst_w, int dst_h);

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

  struct DownscaleCacheEntry {
    size_t media_index = 0;
    int phys_width = 0;
    int phys_height = 0;
    uint64_t last_used_tick = 0;
    std::unique_ptr<gfx::Bitmap> bitmap;
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
  void EnsureImageMipsBuilt(size_t media_index);
  gfx::TextureHandle GetTextureForPhysicalSize(size_t media_index,
                                               int target_phys_w,
                                               int target_phys_h,
                                               bool* out_nearest_sampling);
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

  // Textures and lazy mip chains for the selected board; rebuilt when id +
  // board sha change.
  std::string images_for_;
  std::vector<std::unique_ptr<gfx::Bitmap>> images_;
  std::vector<ReviewBoardImage> image_sources_;
  std::vector<std::vector<ReviewBoardRgbaMip>> image_mips_;
  std::vector<DownscaleCacheEntry> downscale_cache_;
  uint64_t downscale_cache_tick_ = 0;
  std::vector<std::string> image_errors_;
  bool fit_width_ = true;

  // Full-image modal viewer state.
  bool open_image_viewer_popup_ = false;
  bool image_viewer_open_ = false;
  int viewer_media_index_ = 0;
  ReviewBoardViewerState viewer_state_;
  ImVec2 viewer_last_canvas_avail_ = ImVec2(0.0f, 0.0f);
  ImVec2 viewer_last_modal_size_ = ImVec2(0.0f, 0.0f);
  bool viewer_dragging_ = false;
  ImGuiMouseButton viewer_drag_button_ = ImGuiMouseButton_Left;
  ImVec2 viewer_drag_start_pan_ = ImVec2(0.0f, 0.0f);

  std::string status_for_;
  std::string status_;
  bool status_is_error_ = false;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_AGENT_UI_REVIEW_BOARDS_REVIEW_BOARDS_VIEW_H_
