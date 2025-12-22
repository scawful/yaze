# Message Editor Implementation Roadmap

**Status**: Active Development  
**Owner (Agent ID)**: imgui-frontend-engineer  
**Last Updated**: 2025-11-25  
**Next Review**: 2025-12-02  
**Coordination Board Entry**: link when claimed
**Related Docs**:
- `docs/internal/architecture/message_system.md` (Gemini's architecture vision)
- `docs/internal/plans/message_system_improvement_plan.md` (Gemini's feature proposals)

---

## Executive Summary

This roadmap bridges Gemini's architectural vision with practical implementation steps for completing the Message Editor. The current implementation has the **core foundation** in place (message parsing, dictionary system, preview rendering) but lacks several key features proposed in Gemini's plan, particularly around **JSON import/export**, **translation workflows**, and **theme integration**.

---

## Current State Analysis

### What's Working (Completed Features)

#### Core Data Layer ✅
- **MessageData**: Full implementation with raw/parsed representations
- **DictionaryEntry**: Compression system with dictionary optimization
- **TextElement**: Command and special character parsing
- **Character Encoding**: Complete CharEncoder table (0x00-0x66)
- **ROM Reading**: `ReadAllTextData()` successfully loads all 396 messages
- **ROM Writing**: `Save()` handles two-bank text data with overflow detection

#### Message Preview System ✅
- **MessagePreview**: Live rendering of messages as they appear in-game
- **Font Graphics**: 2BPP font tiles loaded and displayed at 0x70000
- **Character Widths**: Proportional font support via width table at 0x74ADF
- **Preview Bitmap**: Real-time message rendering with proper palette support

#### Editor UI ✅
- **Card System**: Four dockable cards (Message List, Editor, Font Atlas, Dictionary)
- **Message List**: Table view with ID, contents, and address columns
- **Text Editor**: Multiline input with live preview updates
- **Command Insertion**: Buttons to insert text commands and special characters
- **Dictionary Display**: Read-only view of all 97 dictionary entries
- **Expanded Messages**: Basic support for loading external message bins

#### Testing Coverage ✅
- **Unit Tests**: 20+ tests covering parsing, encoding, dictionary optimization
- **Integration Tests**: ROM-dependent tests verify actual game data
- **Command Parsing**: Regression tests for argument handling bugs

#### CLI Integration ✅
- **Message List**: `z3ed message list --format json --range 0-100`
- **Message Read**: `z3ed message read --id 5 --format json`
- **Message Search**: `z3ed message search --query "Link"`
- **Message Stats**: `z3ed message stats --format json`

### What's Missing (Gaps vs. Gemini's Vision)

#### 1. JSON Import/Export ❌ (HIGH PRIORITY)
**Status**: Not implemented
**Gemini's Vision**:
```json
[
  {
    "id": 0,
    "address": 917504,
    "text": "[W:00][SPD:00]Welcome to [D:05]...",
    "context": "Uncle dying in sewers"
  }
]
```

**Current Gap**:
- No `SerializeMessages()` or `DeserializeMessages()` in `MessageData`
- No UI for export/import operations
- No context field for translator notes
- CLI has JSON output but not JSON input

**Impact**: Cannot version control text, cannot use external editors, cannot collaborate with translators

---

#### 2. Translation Workspace ❌ (MEDIUM PRIORITY)
**Status**: Not implemented
**Gemini's Vision**: Side-by-side view with reference ROM/JSON and editable translation

**Current Gap**:
- No reference text display
- No side-by-side layout
- No translation progress tracking
- No language-specific dictionary optimization

**Impact**: Manual translation workflows are tedious and error-prone

---

#### 3. Search & Replace ⚠️ (PARTIAL)
**Status**: Stub implementation exists
**Gemini's Vision**: Regex support, batch replace across all messages

**Current Implementation**:
- `Find()` method exists in `MessageEditor` (lines 574-600)
- Basic UI skeleton present (search input, case sensitivity toggle)
- **Missing**: Replace functionality, regex support, "Find All", multi-message operations

**Impact**: Global text edits require manual per-message changes

---

#### 4. Theme Integration ❌ (LOW PRIORITY - UI POLISH)
**Status**: Not implemented
**Current Issues**:
- No hardcoded `ImVec4` colors found (GOOD!)
- Not using `AgentUITheme` system for consistency
- Missing semantic color names for message editor components

**Impact**: Message Editor UI may not match rest of application theme

---

#### 5. Expanded ROM Support ⚠️ (PARTIAL)
**Status**: Basic implementation exists
**Gemini's Vision**: Repointing text blocks to expanded ROM space (Banks 10+), automatic bank switching

**Current Implementation**:
- Can load expanded message bins (lines 322-334)
- Can save expanded messages (lines 497-508)
- **Missing**: Repointing logic, bank management, automatic overflow handling

**Impact**: Cannot support large translation projects that exceed vanilla space

---

#### 6. Scripting Integration ❌ (FUTURE)
**Status**: Not planned
**Gemini's Vision**: Lua/Python API for procedural text generation

**Current Gap**: No scripting hooks in message system

**Impact**: Low - nice-to-have for advanced users

---

## Architectural Decisions Required

### Decision 1: JSON Schema Design
**Question**: What fields should the JSON export include?

**Proposal**:
```json
{
  "version": "1.0",
  "rom_name": "zelda3.sfc",
  "messages": [
    {
      "id": 0,
      "address": 917504,
      "address_hex": "0xE0000",
      "text": "[W:00][SPD:00]Welcome...",
      "context": "Optional translator note",
      "dictionary_optimized": true,
      "expanded": false
    }
  ],
  "dictionary": [
    {
      "id": 0,
      "token": "[D:00]",
      "contents": "the"
    }
  ]
}
```

**Trade-offs**:
- Verbose but human-readable
- Includes metadata for validation
- Context field for translator workflow

**Status**: ✅ RECOMMENDED

---

### Decision 2: Translation Workspace Layout
**Question**: How should reference vs. translation be displayed?

**Option A**: Side-by-side split pane
```
┌────────────────┬────────────────┐
│ Reference      │ Translation    │
│ (English)      │ (Spanish)      │
│ [Read-only]    │ [Editable]     │
│                │                │
│ Message 0:     │ Message 0:     │
│ "Welcome to    │ "Bienvenido a  │
│  Hyrule"       │  Hyrule"       │
└────────────────┴────────────────┘
```

**Option B**: Top-bottom with context panel
```
┌────────────────────────────────┐
│ Reference: "Welcome to Hyrule" │
│ Context: Uncle's dying words   │
├────────────────────────────────┤
│ Translation:                   │
│ [Editable text box]            │
└────────────────────────────────┘
```

**Recommendation**: Option A for large screens, Option B for narrow windows

**Status**: ⚠️ NEEDS USER FEEDBACK

---

### Decision 3: Dictionary Auto-Optimization
**Question**: Should we auto-generate optimal dictionary for new languages?

**Challenges**:
- Dictionary optimization is NP-hard (longest common substring problem)
- Need to preserve ROM space constraints (97 entries max)
- Different languages have different common phrases

**Proposal**:
1. Provide "Analyze Translation" button that suggests optimal dictionary
2. Let user accept/reject suggestions
3. Preserve manual dictionary entries

**Status**: ⚠️ NEEDS RESEARCH

---

## Implementation Priority Matrix

### Phase 1: Foundation (Sprint 1-2 weeks)
**Goal**: JSON import/export with UI integration

#### Task 1.1: Implement JSON Serialization
**Location**: `src/app/editor/message/message_data.h`, `message_data.cc`
**Priority**: P0 (Blocker for translation workflow)
**Estimated Effort**: 3 days

**Implementation**:
```cpp
// In MessageData
nlohmann::json SerializeToJson() const;
static absl::StatusOr<MessageData> DeserializeFromJson(const nlohmann::json& j);

// Free functions
absl::Status ExportMessagesToJson(
    const std::vector<MessageData>& messages,
    const std::vector<DictionaryEntry>& dictionary,
    const std::string& output_path);

absl::StatusOr<std::vector<MessageData>> ImportMessagesFromJson(
    const std::string& input_path);
```

**Dependencies**: nlohmann/json (already in project via CPM)

**Acceptance Criteria**:
- [ ] Export all 396 messages to valid JSON
- [ ] Import JSON and verify byte-for-byte ROM equivalence
- [ ] Handle malformed JSON with clear error messages
- [ ] Preserve dictionary optimization
- [ ] Include context field in schema

---

#### Task 1.2: Add Export/Import UI
**Location**: `src/app/editor/message/message_editor.cc`
**Priority**: P0
**Estimated Effort**: 2 days

**UI Additions**:
```cpp
void MessageEditor::DrawExportImportPanel() {
  if (ImGui::Button("Export to JSON")) {
    std::string path = util::FileDialogWrapper::ShowSaveFileDialog("json");
    PRINT_IF_ERROR(ExportMessagesToJson(list_of_texts_,
                                        message_preview_.all_dictionaries_,
                                        path));
  }

  if (ImGui::Button("Import from JSON")) {
    std::string path = util::FileDialogWrapper::ShowOpenFileDialog();
    auto result = ImportMessagesFromJson(path);
    if (result.ok()) {
      list_of_texts_ = result.value();
      RefreshMessageList();
    }
  }
}
```

**Acceptance Criteria**:
- [ ] File dialogs open with correct filters
- [ ] Progress indicator for large exports
- [ ] Confirmation dialog on import (warns about overwriting)
- [ ] Error popup on import failure with details

---

#### Task 1.3: CLI JSON Import Support
**Location**: `src/cli/handlers/game/message.cc`
**Priority**: P1
**Estimated Effort**: 1 day

**Implementation**:
```bash
z3ed message import --json messages.json --rom zelda3.sfc --output zelda3_translated.sfc
```

**Acceptance Criteria**:
- [ ] Import JSON and write to ROM
- [ ] Validate JSON schema before import
- [ ] Verify ROM size constraints
- [ ] Dry-run mode (validate without writing)

---

### Phase 2: Translation Workflow (Sprint 2-3 weeks)
**Goal**: Side-by-side translation UI

#### Task 2.1: Add Translation Mode Card
**Location**: `src/app/editor/message/message_editor.h`, `message_editor.cc`
**Priority**: P1
**Estimated Effort**: 5 days

**New Components**:
```cpp
class TranslationWorkspace {
 public:
  void Initialize(Rom* reference_rom, Rom* translation_rom);
  void DrawUI();
  void LoadReferenceFromJson(const std::string& path);

 private:
  void DrawSideBySideView();
  void DrawProgressTracker();
  void UpdateTranslationProgress();

  std::vector<MessageData> reference_messages_;
  std::vector<MessageData> translation_messages_;
  std::map<int, bool> translation_complete_flags_;
  Rom* reference_rom_ = nullptr;
  Rom* translation_rom_ = nullptr;
};
```

**UI Mockup**:
```
┌─────────────────────────────────────────────────┐
│ Translation Progress: 42/396 (10.6%)           │
├──────────────────────┬──────────────────────────┤
│ Reference (EN)       │ Translation (ES)         │
├──────────────────────┼──────────────────────────┤
│ Message 0:           │ Message 0:               │
│ "Welcome to Hyrule"  │ [Editable input box]     │
│                      │                          │
│ Dictionary: [D:05]   │ Dictionary: [D:05]       │
├──────────────────────┴──────────────────────────┤
│ [Previous] [Next] [Mark Complete] [Skip]       │
└─────────────────────────────────────────────────┘
```

**Acceptance Criteria**:
- [ ] Load reference ROM or JSON
- [ ] Display messages side-by-side
- [ ] Track translation progress (per-message completion)
- [ ] Keyboard shortcuts for navigation (Ctrl+N, Ctrl+P)
- [ ] Auto-save translated ROM on completion

---

#### Task 2.2: Context/Notes System
**Location**: `src/app/editor/message/message_data.h`
**Priority**: P2
**Estimated Effort**: 2 days

**Schema Addition**:
```cpp
struct MessageData {
  // ... existing fields ...
  std::string context;  // Translator notes, scene context
  std::string screenshot_path;  // Optional screenshot reference

  nlohmann::json SerializeToJson() const {
    return {
      {"id", ID},
      {"address", Address},
      {"text", RawString},
      {"context", context},
      {"screenshot", screenshot_path}
    };
  }
};
```

**UI Addition**:
```cpp
void MessageEditor::DrawContextPanel() {
  ImGui::InputTextMultiline("Context Notes", &current_message_.context);
  if (!current_message_.screenshot_path.empty()) {
    ImGui::Image(LoadScreenshot(current_message_.screenshot_path));
  }
}
```

**Acceptance Criteria**:
- [ ] Context field persists in JSON export/import
- [ ] Context displayed in translation workspace
- [ ] Optional screenshot attachment (stored as relative path)

---

### Phase 3: Search & Replace (Sprint 3-1 week)
**Goal**: Complete Find/Replace implementation

#### Task 3.1: Implement Replace Functionality
**Location**: `src/app/editor/message/message_editor.cc`
**Priority**: P2
**Estimated Effort**: 2 days

**Implementation**:
```cpp
absl::Status MessageEditor::Replace(const std::string& find_text,
                                    const std::string& replace_text,
                                    bool case_sensitive,
                                    bool whole_word,
                                    bool all_messages) {
  int replaced_count = 0;

  if (all_messages) {
    for (auto& message : list_of_texts_) {
      replaced_count += ReplaceInMessage(message, find_text, replace_text,
                                        case_sensitive, whole_word);
    }
  } else {
    replaced_count += ReplaceInMessage(current_message_, find_text,
                                       replace_text, case_sensitive, whole_word);
  }

  return absl::OkStatus();
}
```

**UI Updates**:
```cpp
void MessageEditor::DrawFindReplacePanel() {
  static char find_text[256] = "";
  static char replace_text[256] = "";

  ImGui::InputText("Find", find_text, IM_ARRAYSIZE(find_text));
  ImGui::InputText("Replace", replace_text, IM_ARRAYSIZE(replace_text));

  ImGui::Checkbox("Case Sensitive", &case_sensitive_);
  ImGui::Checkbox("Whole Word", &match_whole_word_);
  ImGui::Checkbox("All Messages", &replace_all_messages_);

  if (ImGui::Button("Replace")) {
    PRINT_IF_ERROR(Replace(find_text, replace_text, case_sensitive_,
                          match_whole_word_, false));
  }

  if (ImGui::Button("Replace All")) {
    PRINT_IF_ERROR(Replace(find_text, replace_text, case_sensitive_,
                          match_whole_word_, true));
  }
}
```

**Acceptance Criteria**:
- [ ] Replace in current message
- [ ] Replace in all messages
- [ ] Case-sensitive/insensitive options
- [ ] Whole word matching
- [ ] Undo support (requires history stack)

---

#### Task 3.2: Add Regex Support
**Location**: `src/app/editor/message/message_editor.cc`
**Priority**: P3 (Nice-to-have)
**Estimated Effort**: 2 days

**Implementation**:
```cpp
absl::Status MessageEditor::ReplaceRegex(const std::string& pattern,
                                         const std::string& replacement,
                                         bool all_messages) {
  std::regex regex_pattern;
  try {
    regex_pattern = std::regex(pattern);
  } catch (const std::regex_error& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid regex: %s", e.what()));
  }

  // Perform replacement...
}
```

**Acceptance Criteria**:
- [ ] Regex validation with error messages
- [ ] Capture group support ($1, $2, etc.)
- [ ] Preview matches before replacement

---

### Phase 4: UI Polish (Sprint 4-1 week)
**Goal**: Theme integration and UX improvements

#### Task 4.1: Integrate AgentUITheme
**Location**: `src/app/editor/message/message_editor.cc`
**Priority**: P3
**Estimated Effort**: 1 day

**Implementation**:
```cpp
void MessageEditor::DrawMessageList() {
  const auto& theme = AgentUI::GetTheme();

  AgentUI::PushPanelStyle();
  ImGui::PushStyleColor(ImGuiCol_Header, theme.panel_bg_darker);
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, theme.accent_color);

  // ... table rendering ...

  ImGui::PopStyleColor(2);
  AgentUI::PopPanelStyle();
}
```

**Acceptance Criteria**:
- [ ] All panels use `AgentUI::PushPanelStyle()`
- [ ] Section headers use `AgentUI::RenderSectionHeader()`
- [ ] Buttons use `AgentUI::StyledButton()` where appropriate
- [ ] Color scheme matches rest of editor

---

#### Task 4.2: Add Keyboard Shortcuts
**Location**: `src/app/editor/message/message_editor.cc`
**Priority**: P3
**Estimated Effort**: 1 day

**Shortcuts**:
- `Ctrl+F`: Open Find/Replace
- `Ctrl+E`: Export to JSON
- `Ctrl+I`: Import from JSON
- `Ctrl+S`: Save ROM
- `Ctrl+N`: Next message (in translation mode)
- `Ctrl+P`: Previous message (in translation mode)

**Implementation**:
```cpp
void MessageEditor::HandleKeyboardShortcuts() {
  if (ImGui::IsKeyPressed(ImGuiKey_F) &&
      ImGui::GetIO().KeyCtrl) {
    show_find_replace_ = true;
  }

  // ... other shortcuts ...
}
```

**Acceptance Criteria**:
- [ ] Shortcuts don't conflict with global shortcuts
- [ ] Shortcuts displayed in tooltips
- [ ] Configurable shortcuts (future enhancement)

---

## Test Strategy

### Unit Tests
**Location**: `test/unit/message_data_test.cc` (new file)
```cpp
TEST(MessageDataTest, SerializeToJson_BasicMessage) {
  MessageData msg;
  msg.ID = 0;
  msg.Address = 0xE0000;
  msg.RawString = "Hello World";
  msg.context = "Test message";

  auto json = msg.SerializeToJson();

  EXPECT_EQ(json["id"], 0);
  EXPECT_EQ(json["text"], "Hello World");
  EXPECT_EQ(json["context"], "Test message");
}

TEST(MessageDataTest, DeserializeFromJson_RoundTrip) {
  MessageData original;
  original.ID = 5;
  original.RawString = "[W:00][K]Test";

  auto json = original.SerializeToJson();
  auto result = MessageData::DeserializeFromJson(json);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().ID, 5);
  EXPECT_EQ(result.value().RawString, "[W:00][K]Test");
}
```

### Integration Tests
**Location**: `test/integration/message_export_test.cc` (new file)
```cpp
TEST_F(MessageRomTest, ExportImport_RoundTrip) {
  // Export all messages to JSON
  std::string json_path = "/tmp/messages.json";
  EXPECT_OK(ExportMessagesToJson(list_of_texts_, dictionary_, json_path));

  // Import back
  auto imported = ImportMessagesFromJson(json_path);
  ASSERT_TRUE(imported.ok());

  // Verify identical
  EXPECT_EQ(imported.value().size(), list_of_texts_.size());
  for (size_t i = 0; i < list_of_texts_.size(); ++i) {
    EXPECT_EQ(imported.value()[i].RawString, list_of_texts_[i].RawString);
  }
}
```

### E2E Tests
**Location**: `test/e2e/message_editor_workflow_test.cc` (new file)
```cpp
TEST_F(MessageEditorE2ETest, TranslationWorkflow) {
  // Open translation workspace
  EXPECT_OK(ClickButton("Translation Mode"));

  // Load reference ROM
  EXPECT_OK(OpenFileDialog("reference_rom.sfc"));

  // Navigate to message 0
  EXPECT_EQ(GetCurrentMessageID(), 0);

  // Edit translation
  EXPECT_OK(SetTextBoxValue("Bienvenido a Hyrule"));

  // Mark complete
  EXPECT_OK(ClickButton("Mark Complete"));

  // Verify progress
  EXPECT_EQ(GetTranslationProgress(), "1/396");
}
```

---

## Dependencies & Risks

### External Dependencies
1. **nlohmann/json**: Already integrated via CPM ✅
2. **ImGui Test Engine**: Available for E2E tests ✅
3. **File Dialog**: `util::FileDialogWrapper` already exists ✅

### Technical Risks

#### Risk 1: JSON Schema Evolution
**Impact**: Breaking changes to JSON format
**Mitigation**:
- Include version number in schema
- Implement forward/backward compatibility
- Provide migration tool for old exports

#### Risk 2: Dictionary Auto-Optimization Complexity
**Impact**: Algorithm may be too slow for real-time use
**Mitigation**:
- Run optimization in background thread
- Provide progress indicator
- Allow cancellation

#### Risk 3: Large ROM Size with Expanded Messages
**Impact**: May exceed bank boundaries
**Mitigation**:
- Implement repointing logic early (Phase 5)
- Warn user when approaching limits
- Suggest dictionary optimization

---

## Success Metrics

### Quantitative Metrics
- [ ] 100% message export/import success rate (no data loss)
- [ ] JSON schema supports all 396 vanilla messages
- [ ] Translation workspace reduces edit time by 50% vs. current workflow
- [ ] Search/Replace handles batch operations in <1 second
- [ ] 90%+ test coverage for new code

### Qualitative Metrics
- [ ] Translator feedback: "Translation workflow is intuitive"
- [ ] No hardcoded colors in Message Editor
- [ ] UI matches yaze style guide
- [ ] Documentation complete for all new features

---

## Timeline Estimate

| Phase | Duration | Effort (Dev Days) |
|-------|----------|-------------------|
| Phase 1: JSON Export/Import | 2 weeks | 6 days |
| Phase 2: Translation Workspace | 3 weeks | 9 days |
| Phase 3: Search & Replace | 1 week | 4 days |
| Phase 4: UI Polish | 1 week | 2 days |
| **Total** | **7 weeks** | **21 dev days** |

**Note**: Timeline assumes single developer working full-time. Adjust for part-time work or team collaboration.

---

## Future Enhancements (Post-MVP)

1. **Scripting API** (Gemini's vision)
   - Expose MessageData to Lua/Python
   - Allow procedural text generation
   - Useful for randomizers

2. **Cloud Translation Integration**
   - Google Translate API for quick drafts
   - DeepL API for quality translations
   - Requires API key management

3. **Collaborative Editing**
   - Real-time multi-user translation
   - Conflict resolution for concurrent edits
   - Requires network infrastructure

4. **ROM Patch Generation**
   - Export as `.ips` or `.bps` patch files
   - Useful for distribution without ROM sharing
   - Requires patch generation library

5. **Message Validation**
   - Check for overlong messages (exceeds textbox width)
   - Verify all messages have terminators
   - Flag unused dictionary entries

---

## Open Questions

1. **Q**: Should we support multiple translation languages simultaneously?
   **A**: TBD - May require multi-ROM workspace UI

2. **Q**: How should we handle custom dictionary entries for expanded ROMs?
   **A**: TBD - Need research into ROM space allocation

3. **Q**: Should translation progress be persisted?
   **A**: TBD - Could store in `.yaze` project file

4. **Q**: Do we need undo/redo for message editing?
   **A**: TBD - ImGui InputTextMultiline has built-in undo, may be sufficient

---

## Conclusion

The Message Editor has a **solid foundation** with core parsing, preview, and UI systems in place. The main gaps are **JSON export/import** (P0), **translation workspace** (P1), and **search/replace** (P2).

**Recommended Next Steps**:
1. Start with Phase 1 (JSON export/import) - this unblocks all translation workflows
2. Get user feedback on translation workspace mockups before Phase 2
3. Defer theme integration to Phase 4 - not blocking functionality

**Estimated Effort**: ~7 weeks to MVP, ~21 dev days total.

**Success Criteria**: Translator can export messages to JSON, edit in external tool, and re-import without data loss. Side-by-side translation workspace reduces manual comparison time by 50%.
