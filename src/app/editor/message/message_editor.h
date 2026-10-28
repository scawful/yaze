#ifndef YAZE_APP_EDITOR_MESSAGE_EDITOR_H
#define YAZE_APP_EDITOR_MESSAGE_EDITOR_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/editor.h"
#include "app/editor/message/message_data.h"
#include "app/editor/message/message_preview.h"
#include "app/editor/message/message_undo_actions.h"
#include "app/editor/message/panels/message_editor_panels.h"
#include "app/gfx/core/bitmap.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/style.h"
#include "rom/rom.h"

namespace yaze {
namespace editor {

class AuthoringSaveWorkflowTestPeer;
class MessageEditorTestPeer;
class MessageEditorSaveTestPeer;
class MessageEditorPublishGuardTestPeer;
class MessageEditorListPanelTestPeer;

constexpr int kGfxFont = 0x70000;  // 2bpp format
constexpr int kCharactersWidth = 0x74ADF;
constexpr int kNumMessages = 396;
constexpr int kFontGfxMessageSize = 128;
constexpr int kFontGfxMessageDepth =
    8;  // Fixed: Must be 8 for indexed palette mode
constexpr int kFontGfx16Size = 172 * 4096;

constexpr uint8_t kBlockTerminator = 0x80;
constexpr uint8_t kMessageBankChangeId = 0x80;

class MessageEditor : public Editor {
 public:
  explicit MessageEditor(Rom* rom = nullptr) : rom_(rom) {
    type_ = EditorType::kMessage;
  }

  explicit MessageEditor(Rom* rom, const EditorDependencies& deps)
      : MessageEditor(rom) {
    dependencies_ = deps;
  }

  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() override;
  void SetGameData(zelda3::GameData* game_data) override;

  void DrawMessageList();
  void DrawCurrentMessage();
  bool IsWriterWorkspace() const;
  void DrawPreviewPanel();
  void DrawFontAtlas();
  void DrawTextCommands();
  void DrawSpecialCharacters();
  void DrawExpandedMessageSettings();
  void ExportForReview();
  void DrawDictionary();
  void DrawMessageTools();
  void DrawMessagePreview();
  void UpdateCurrentMessageFromText(const std::string& text);
  void ContributeStatus(StatusBar* status_bar) override;

  // Jump/navigation helper used by panels (e.g. Story Event Graph).
  // Accepts vanilla IDs (0..N) and expanded IDs (expanded_base_id..).
  bool OpenMessageById(int display_id);

  absl::Status Save() override;
  absl::Status BeginSaveTransaction() override;
  void RollbackSaveTransaction() override;
  void CommitSaveTransaction() override;
  absl::Status SaveExpandedMessages();
  absl::Status LoadExpandedMessagesFromRom();
  absl::Status LoadCanonicalExpandedSource();
  bool HasUnpublishedMessageSourceChanges() const;
  int CalculateExpandedBankUsage() const;

  absl::Status Cut() override;
  absl::Status Copy() override;
  absl::Status Paste() override;
  absl::Status Undo() override;
  absl::Status Redo() override;
  absl::Status Find() override;
  // A pending text edit is committed to history by Undo(), so it counts.
  bool CanUndo() const override {
    return pending_undo_before_.has_value() || undo_manager_.CanUndo();
  }
  bool CanFind() const override { return true; }
  void Delete();
  void SelectAll();

  void set_rom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }

 private:
  friend class AuthoringSaveWorkflowTestPeer;
  friend class MessageEditorTestPeer;
  friend class MessageEditorSaveTestPeer;
  friend class MessageEditorPublishGuardTestPeer;
  friend class MessageEditorListPanelTestPeer;

  enum class SaveDomain {
    kFontWidths,
    kVanillaMessages,
    kExpandedMessages,
  };

  struct PlannedWrite {
    SaveDomain domain;
    uint32_t start;
    std::vector<uint8_t> bytes;

    uint32_t end() const { return start + static_cast<uint32_t>(bytes.size()); }
  };

  struct SavePlan {
    std::vector<PlannedWrite> writes;
    std::vector<int> expanded_message_addresses;
    bool saves_font_widths = false;
    bool saves_vanilla_messages = false;
    bool saves_expanded_messages = false;
  };

  struct DirtyState {
    bool font_widths = false;
    bool vanilla_messages = false;
    bool expanded_messages = false;
  };

  absl::StatusOr<SavePlan> BuildSavePlan(bool include_font_widths,
                                         bool include_vanilla_messages,
                                         bool include_expanded_messages) const;
  absl::Status ValidateSavePlan(const SavePlan& plan) const;
  absl::Status ApplySavePlan(const SavePlan& plan);
  absl::Status SaveDirtyDomains(bool include_font_widths,
                                bool include_vanilla_messages,
                                bool include_expanded_messages);
  void MarkDomainDirty(SaveDomain domain);
  void RecordSavedDomains(const SavePlan& plan);
  void ClearSavedDomains(const DirtyState& saved);
  void SyncCurrentExpandedMessageAddress();

  bool case_sensitive_ = false;
  bool match_whole_word_ = false;
  std::string search_text_;
  std::string replace_text_;
  std::string replace_status_;
  bool replace_status_error_ = false;
  std::string list_filter_;
  std::string command_filter_;
  std::string symbol_filter_;
  std::string dictionary_filter_;
  std::vector<std::string> baseline_messages_;
  std::string reference_view_text_;
  bool show_reference_ = true;
  bool project_source_load_attempted_ = false;
  bool source_sync_ready_ = false;
  bool source_sync_changed_ = false;
  std::string source_sync_sha_before_;
  std::string source_sync_draft_sha_;
  std::string source_loaded_canonical_sha_;
  std::string source_sync_summary_;
  std::string source_sync_status_;
  bool source_sync_status_error_ = false;
  int list_bank_filter_ = 0;
  bool find_panel_open_ = false;
  bool find_focus_search_ = false;
  bool find_has_match_ = false;
  size_t find_match_start_ = 0;
  bool scroll_list_to_selection_ = false;
  int editor_cursor_ = 0;
  int pending_selection_start_ = -1;
  int pending_selection_end_ = -1;
  float editor_pane_height_ = 180.0f;
  uint8_t command_parameter_ = 0;

  struct MessageRow {
    int display_id = 0;
    bool expanded = false;
    uint32_t address = 0;
  };

  std::vector<MessageRow> CollectMessageRows(bool apply_filter) const;
  int CurrentDisplayId() const;
  std::string_view ContentsForDisplayId(int display_id) const;
  void DrawFindBar();
  void DrawMessageHeader();
  void DrawMessageStructure();
  void DrawTokenInserter(const std::vector<TextElement>& elements,
                         std::string& filter, bool use_parameter);
  void InsertTokenAtCursor(const std::string& token);
  void SelectAdjacentMessage(int delta);
  void NoteLoadedMessages();
  void AcceptCurrentAsReference();
  std::string CurrentReferenceText() const;
  void DrawDialogueTextPane(float height);
  bool HasMessageSourceLayout() const;
  void DrawExpandedSourceBanner();
  void LoadCanonicalExpandedSourceFromUi();
  absl::Status ExportReviewBundleToFile(const std::string& path) const;
  bool ExpandedEditsAreSourceOwned() const;
  std::pair<int, int> ExpandedInspectionRange() const;
  std::string MessageSourceIncomingPath() const;
  void InvalidateSourcePreview();
  absl::StatusOr<std::filesystem::path> ResolveCanonicalBundlePath() const;
  absl::StatusOr<std::string> HashFileSha256(
      const std::filesystem::path& path) const;
  bool IsCanonicalBundlePath(const std::string& path) const;
  struct ExpandedDraftSnapshot {
    std::vector<MessageData> messages;
    std::vector<MessageData> vanilla_messages;
    std::vector<std::string> parsed_messages;
    int base_id = 0;
    std::string path;
    bool expanded_dirty = false;
    bool vanilla_dirty = false;
    std::string loaded_canonical_sha;
    bool preview_ready = false;
    bool preview_changed = false;
    std::string preview_sha_before;
    std::string preview_draft_sha;
    std::string preview_summary;
    std::string preview_status;
    bool preview_status_error = false;
  };
  ExpandedDraftSnapshot CaptureExpandedDraft() const;
  void RestoreExpandedDraft(const ExpandedDraftSnapshot& snapshot);
  absl::Status LoadExpandedMessagesFromBin(const std::string& path);
  void PreviewMessageSource();
  void PublishMessageSource();
  struct MessageSourcePublishButtons {
    bool preview_disabled = false;
    bool publish_disabled = false;
  };
  MessageSourcePublishButtons DrawMessageSourcePublishButtons();
  void DrawMessageSourceSync();
  bool AdvanceFind(bool forward);
  bool ActivateFindMatch(const MessageRow& row, size_t offset, bool wrapped);
  void OptimizeCurrentMessage();
  void HandleMessageShortcuts();

  // Replace helpers
  int ReplaceCurrentMatch();
  int ReplaceAllMatches();

  std::array<uint8_t, 0x4000> raw_font_gfx_data_;
  std::vector<std::string> parsed_messages_;
  std::vector<MessageData> list_of_texts_;
  std::vector<MessageData> expanded_messages_;

  MessageData current_message_;
  MessagePreview message_preview_;

  gfx::Bitmap font_gfx_bitmap_;
  gfx::Bitmap current_font_gfx16_bitmap_;
  gfx::SnesPalette font_preview_colors_;

  gui::Canvas font_gfx_canvas_{"##FontGfxCanvas", ImVec2(256, 256)};
  gui::Canvas current_font_gfx16_canvas_{"##CurrentMessageGfx",
                                         ImVec2(172 * 2, 4096)};
  gui::TextBox message_text_box_;
  Rom* rom_;
  Rom expanded_message_bin_;
  std::string expanded_message_path_;
  int expanded_message_base_id_ = 0;
  int current_message_index_ = 0;
  bool current_message_is_expanded_ = false;
  DirtyState dirty_state_;
  DirtyState transaction_dirty_snapshot_;
  DirtyState transaction_saved_domains_;
  std::vector<int> transaction_expanded_address_snapshot_;
  bool save_transaction_active_ = false;

  // Undo/Redo - delegates to UndoManager (inherited from Editor)
  std::optional<MessageSnapshot> pending_undo_before_;
  void PushUndoSnapshot();
  void FinalizePendingUndo();
  void ApplySnapshot(const MessageSnapshot& snapshot);

  // Panel visibility states
  bool show_message_list_ = false;
  bool show_message_editor_ = false;
  bool show_font_atlas_ = false;
  bool show_dictionary_ = false;

  void ResolveFontPalette();
  gfx::SnesPalette BuildFallbackFontPalette() const;
  void LoadFontGraphics();
  void RefreshFontAtlasBitmap(const std::vector<uint8_t>& font_data);
  void ApplyFontPalette();
  void EnsureFontTexturesReady();
  void ImportMessageBundleFromFile(const std::string& path);
  int ResolveExpandedMessageBaseId() const;

  bool font_graphics_loaded_ = false;
  std::string message_bundle_status_;
  bool message_bundle_status_error_ = false;
  std::vector<std::string> current_parse_errors_;
  std::vector<std::string> current_parse_warnings_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MESSAGE_EDITOR_H
