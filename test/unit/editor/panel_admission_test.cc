// Panel admission audit.
//
// Registration does not make a window first-class. Every registered window
// must declare a WindowPresentationPolicy; CoreWorkspace and default-visible
// windows must be in FirstClassPanelAllowlist(); embedded and diagnostic
// windows must name their host. These tests pin the rules with mock panels and
// then apply them to the windows a real EditorManager registers, so a new
// panel that skips the decision fails here with instructions.
//
// scripts/audit_panel_policies.py covers the panel classes this test cannot
// instantiate without a ROM, and the GUI budget test
// (test/e2e/panel_budget_test.cc) audits the full app with a ROM.

#include "app/editor/system/workspace/panel_admission.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "app/editor/editor_manager.h"
#include "app/editor/layout/layout_presets.h"
#include "app/editor/session_types.h"
#include "app/editor/system/commands/command_palette.h"
#include "app/editor/system/session/session_coordinator.h"
#include "app/editor/system/workspace/resource_panel.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "editor_test_support.h"
#include "imgui/imgui.h"
#include "panel_demotions.h"

namespace yaze::editor {
namespace {

class MockPolicyPanel final : public WindowContent {
 public:
  MockPolicyPanel(std::string id, std::string category,
                  std::optional<WindowPresentationPolicy> policy,
                  bool visible_by_default = false)
      : id_(std::move(id)),
        category_(std::move(category)),
        policy_(std::move(policy)),
        visible_by_default_(visible_by_default) {}

  std::string GetId() const override { return id_; }
  std::string GetDisplayName() const override { return "Mock " + id_; }
  std::string GetIcon() const override { return "ICON_MOCK"; }
  std::string GetEditorCategory() const override { return category_; }
  void Draw(bool* /*p_open*/) override {}
  bool IsVisibleByDefault() const override { return visible_by_default_; }
  WindowPresentationPolicy GetPresentationPolicy() const override {
    return policy_.has_value() ? *policy_
                               : WindowContent::GetPresentationPolicy();
  }

 private:
  std::string id_;
  std::string category_;
  std::optional<WindowPresentationPolicy> policy_;
  bool visible_by_default_;
};

class MockResourcePolicyPanel final : public ResourceWindowContent {
 public:
  int GetResourceId() const override { return 7; }
  std::string GetResourceType() const override { return "thing"; }
  std::string GetIcon() const override { return "ICON_MOCK"; }
  std::string GetEditorCategory() const override { return "Test"; }
  void Draw(bool* /*p_open*/) override {}
};

std::vector<PanelAdmissionFinding> AuditOne(
    const std::string& id, std::optional<WindowPresentationPolicy> policy,
    bool visible_by_default = false) {
  WorkspaceWindowManager wm;
  wm.RegisterSession(0);
  wm.SetActiveSession(0);
  wm.RegisterWindowContent(std::make_unique<MockPolicyPanel>(
      id, "Test", std::move(policy), visible_by_default));
  return AuditPanelAdmission(wm, 0);
}

bool AnyFindingMentions(const std::vector<PanelAdmissionFinding>& findings,
                        const std::string& text) {
  return std::any_of(findings.begin(), findings.end(),
                     [&](const PanelAdmissionFinding& finding) {
                       return finding.ToString().find(text) !=
                              std::string::npos;
                     });
}

TEST(PanelPolicyTest, NamedFactoriesDeclareAndDefaultDoesNot) {
  EXPECT_FALSE(WindowPresentationPolicy().declared);
  // A panel that overrides nothing inherits the undeclared default.
  MockPolicyPanel inherits("test.inherits", "Test", std::nullopt);
  EXPECT_FALSE(inherits.GetPresentationPolicy().declared);
  EXPECT_TRUE(WindowPresentationPolicy::CoreWorkspace().declared);
  EXPECT_TRUE(WindowPresentationPolicy::EmbeddedTool().declared);
  EXPECT_TRUE(WindowPresentationPolicy::OptionalPopOut().declared);
  EXPECT_TRUE(WindowPresentationPolicy::Diagnostic().declared);

  const auto hosted =
      WindowPresentationPolicy::EmbeddedTool("cap").WithHost("Host Surface");
  EXPECT_TRUE(hosted.declared);
  EXPECT_EQ(hosted.role, WindowPresentationRole::kEmbeddedTool);
  EXPECT_EQ(hosted.required_capability, "cap");
  EXPECT_EQ(hosted.host, "Host Surface");
  EXPECT_FALSE(hosted.list_in_window_browser);
}

TEST(PanelPolicyTest, UndeclaredPanelStillBehavesAsListedPopOut) {
  const WindowPresentationPolicy undeclared;
  EXPECT_EQ(undeclared.role, WindowPresentationRole::kOptionalPopOut);
  EXPECT_TRUE(undeclared.list_in_window_browser);
  EXPECT_TRUE(undeclared.allow_popout);
}

TEST(PanelAdmissionAuditTest, ReportsPanelsThatDeclareNothing) {
  const auto findings = AuditOne("test.undeclared", std::nullopt);
  ASSERT_EQ(findings.size(), 1u);
  EXPECT_EQ(findings[0].window_id, "test.undeclared");
  EXPECT_NE(findings[0].problem.find("no presentation policy"),
            std::string::npos);
  // The message must tell the author what to do.
  for (const char* hint :
       {"GetPresentationPolicy", "EmbeddedTool", "OptionalPopOut", "Diagnostic",
        "WithHost", "FirstClassPanelAllowlist"}) {
    EXPECT_NE(findings[0].fix.find(hint), std::string::npos) << hint;
  }
}

TEST(PanelAdmissionAuditTest, AcceptsDeclaredHiddenPopOut) {
  EXPECT_TRUE(
      AuditOne("test.popout", WindowPresentationPolicy::OptionalPopOut())
          .empty());
}

TEST(PanelAdmissionAuditTest, EmbeddedAndDiagnosticMustNameAHost) {
  auto findings =
      AuditOne("test.embedded", WindowPresentationPolicy::EmbeddedTool());
  ASSERT_EQ(findings.size(), 1u);
  EXPECT_NE(findings[0].problem.find("names no host"), std::string::npos);
  EXPECT_NE(findings[0].fix.find("WithHost"), std::string::npos);

  findings =
      AuditOne("test.diagnostic", WindowPresentationPolicy::Diagnostic());
  ASSERT_EQ(findings.size(), 1u);
  EXPECT_NE(findings[0].problem.find("names no host"), std::string::npos);

  EXPECT_TRUE(AuditOne("test.embedded_ok",
                       WindowPresentationPolicy::EmbeddedTool().WithHost(
                           "Test inspector"))
                  .empty());
  EXPECT_TRUE(AuditOne("test.diagnostic_ok",
                       WindowPresentationPolicy::Diagnostic().WithHost(
                           "Command palette"))
                  .empty());
}

TEST(PanelAdmissionAuditTest, CoreWorkspaceNeedsAnAllowlistEntry) {
  const auto findings =
      AuditOne("test.core", WindowPresentationPolicy::CoreWorkspace());
  ASSERT_EQ(findings.size(), 1u);
  EXPECT_NE(findings[0].problem.find("CoreWorkspace"), std::string::npos);
  EXPECT_NE(findings[0].fix.find("panel_admission.cc"), std::string::npos);

  // An allowlisted id is admitted as Core.
  ASSERT_NE(FindFirstClassPanelEntry("dungeon.workbench"), nullptr);
  EXPECT_TRUE(
      AuditOne("dungeon.workbench", WindowPresentationPolicy::CoreWorkspace())
          .empty());
}

TEST(PanelAdmissionAuditTest, DefaultVisibleNeedsAnAllowlistEntry) {
  const auto findings =
      AuditOne("test.visible", WindowPresentationPolicy::OptionalPopOut(),
               /*visible_by_default=*/true);
  ASSERT_EQ(findings.size(), 1u);
  EXPECT_NE(findings[0].problem.find("visible by default"), std::string::npos);
  EXPECT_TRUE(AnyFindingMentions(findings, "IsVisibleByDefault"));

  EXPECT_TRUE(AuditOne("overworld.canvas",
                       WindowPresentationPolicy::CoreWorkspace(),
                       /*visible_by_default=*/true)
                  .empty());
}

TEST(PanelAdmissionAuditTest, ResourceWindowsDefaultToDeclaredPopOut) {
  WorkspaceWindowManager wm;
  wm.RegisterSession(0);
  wm.SetActiveSession(0);
  wm.RegisterWindowContent(std::make_unique<MockResourcePolicyPanel>());
  const auto* descriptor = wm.GetWindowDescriptor(0, "Test.thing_7");
  ASSERT_NE(descriptor, nullptr);
  EXPECT_TRUE(descriptor->presentation.declared);
  EXPECT_EQ(descriptor->presentation.role,
            WindowPresentationRole::kOptionalPopOut);
  EXPECT_TRUE(AuditPanelAdmission(wm, 0).empty());
}

TEST(PanelAdmissionAuditTest, ContentPolicyFillsAnUndeclaredDescriptor) {
  WorkspaceWindowManager wm;
  wm.RegisterSession(0);
  wm.SetActiveSession(0);

  // Editors pre-register a descriptor (icon, shortcut, priority) and then
  // register the content. The content's declared policy must not be lost.
  wm.RegisterPanel({.card_id = "test.dual",
                    .display_name = "Dual",
                    .icon = "ICON_MOCK",
                    .category = "Test",
                    .priority = 1});
  wm.RegisterWindowContent(std::make_unique<MockPolicyPanel>(
      "test.dual", "Test",
      WindowPresentationPolicy::EmbeddedTool().WithHost("Test inspector")));
  const auto* dual = wm.GetWindowDescriptor(0, "test.dual");
  ASSERT_NE(dual, nullptr);
  EXPECT_EQ(dual->presentation.role, WindowPresentationRole::kEmbeddedTool);
  EXPECT_EQ(dual->presentation.host, "Test inspector");
  EXPECT_FALSE(dual->IsListedInWindowBrowser());

  // A descriptor that already declared a policy keeps it.
  WindowDescriptor declared_first{.card_id = "test.declared_first",
                                  .display_name = "Declared First",
                                  .icon = "ICON_MOCK",
                                  .category = "Test",
                                  .priority = 2};
  declared_first.presentation = WindowPresentationPolicy::OptionalPopOut();
  wm.RegisterPanel(declared_first);
  wm.RegisterWindowContent(std::make_unique<MockPolicyPanel>(
      "test.declared_first", "Test",
      WindowPresentationPolicy::Diagnostic().WithHost("Command palette")));
  const auto* kept = wm.GetWindowDescriptor(0, "test.declared_first");
  ASSERT_NE(kept, nullptr);
  EXPECT_EQ(kept->presentation.role, WindowPresentationRole::kOptionalPopOut);
}

void ExpectUniqueAndExplained(const std::vector<PanelAdmissionEntry>& entries,
                              const char* list_name) {
  std::set<std::string> seen;
  for (const auto& entry : entries) {
    ASSERT_NE(entry.window_id, nullptr);
    ASSERT_NE(entry.reason, nullptr);
    EXPECT_FALSE(std::string(entry.window_id).empty());
    EXPECT_GE(std::string(entry.reason).size(), 20u)
        << list_name << ": " << entry.window_id
        << " needs a real reason, not a placeholder";
    EXPECT_TRUE(seen.insert(entry.window_id).second)
        << list_name << ": duplicate entry " << entry.window_id;
  }
}

TEST(PanelAdmissionAllowlistTest, EntriesAreUniqueAndExplained) {
  ExpectUniqueAndExplained(FirstClassPanelAllowlist(), "FirstClassAllowlist");
  ExpectUniqueAndExplained(DeferredPolicyDeclarations(),
                           "DeferredPolicyDeclarations");
}

TEST(PanelAdmissionAuditTest, DeferredDeclarationsAreToleratedOnlyForTheirIds) {
  // Frozen-directory panels (Overworld) may stay undeclared for now.
  ASSERT_TRUE(IsPolicyDeclarationDeferred("overworld.tile16_editor"));
  EXPECT_TRUE(AuditOne("overworld.tile16_editor", std::nullopt).empty());
  // Any other undeclared window is still reported.
  EXPECT_EQ(AuditOne("test.not_deferred", std::nullopt).size(), 1u);
}

TEST(PanelAdmissionAuditTest, FormatGroupsIdenticalFixesIntoOneBlock) {
  WorkspaceWindowManager wm;
  wm.RegisterSession(0);
  wm.SetActiveSession(0);
  for (const char* id : {"test.a", "test.b", "test.c"}) {
    wm.RegisterWindowContent(
        std::make_unique<MockPolicyPanel>(id, "Test", std::nullopt));
  }
  const auto findings = AuditPanelAdmission(wm, 0);
  ASSERT_EQ(findings.size(), 3u);
  const std::string text = FormatPanelAdmissionFindings(findings);
  for (const char* id : {"test.a", "test.b", "test.c"}) {
    EXPECT_NE(text.find(id), std::string::npos) << id;
  }
  size_t fixes = 0;
  for (size_t pos = text.find("Fix:"); pos != std::string::npos;
       pos = text.find("Fix:", pos + 1)) {
    ++fixes;
  }
  EXPECT_EQ(fixes, 1u) << text;
}

// A first open must not bury the editor's canvas. Each editor's default layout
// opens a handful of windows at most, and every one of them is a reviewed
// first-class window. Adding a default-open window means adding an allowlist
// entry with a reason, not editing this budget.
TEST(PanelAdmissionBudgetTest, DefaultLayoutsOpenAFewAllowlistedWindows) {
  constexpr size_t kMaxDefaultOpenWindows = 3;
  constexpr EditorType kEditors[] = {
      EditorType::kOverworld, EditorType::kDungeon, EditorType::kGraphics,
      EditorType::kPalette,   EditorType::kSprite,  EditorType::kScreen,
      EditorType::kMusic,     EditorType::kMessage, EditorType::kAssembly,
      EditorType::kEmulator,  EditorType::kHex,     EditorType::kAgent,
  };
  for (EditorType type : kEditors) {
    const PanelLayoutPreset preset = LayoutPresets::GetDefaultPreset(type);
    EXPECT_LE(preset.default_visible_panels.size(), kMaxDefaultOpenWindows)
        << preset.name << " opens too many windows on first use";
    for (const auto& window_id : preset.default_visible_panels) {
      EXPECT_NE(FindFirstClassPanelEntry(window_id), nullptr)
          << preset.name << ": '" << window_id
          << "' opens by default but is not in FirstClassPanelAllowlist()";
    }
  }
}

// Applies the rules to what a real EditorManager registers with no ROM:
// EditorManager-owned windows, every ROM-tolerant editor, and any registry
// (REGISTER_PANEL) panels linked into this binary.
class PanelAdmissionRegistryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    renderer_ = std::make_unique<gfx::NullRenderer>();
    manager_ = std::make_unique<EditorManager>();
  }

  void TearDown() override {
    manager_.reset();
    renderer_.reset();
    if (imgui_context_ != nullptr) {
      ImGui::DestroyContext(imgui_context_);
      imgui_context_ = nullptr;
    }
  }

  std::unique_ptr<gfx::NullRenderer> renderer_;
  std::unique_ptr<EditorManager> manager_;
  ImGuiContext* imgui_context_ = nullptr;
};

// Registers every window a ROM-free session can register. Returns the session
// id whose windows to inspect.
size_t RegisterRomFreeWindows(EditorManager* manager) {
  manager->CreateNewSession();

  // The production path: without a ROM only editors that are safe to run
  // empty initialize, exactly as the app does.
  for (size_t i = 0; i < kEditorTypeCount; ++i) {
    EXPECT_TRUE(
        manager->EnsureEditorAssetsLoaded(static_cast<EditorType>(i)).ok());
  }

  // These editors register their descriptors in Initialize() without touching
  // ROM data; run them directly so their windows are audited too. Message and
  // Music are left to the ROM-backed GUI audit (they read ROM bytes or open an
  // audio device while initializing).
  RomSession* session = manager->session_coordinator()->GetActiveRomSession();
  if (session != nullptr) {
    for (EditorType type :
         {EditorType::kDungeon, EditorType::kOverworld, EditorType::kPalette,
          EditorType::kScreen, EditorType::kSprite}) {
      if (Editor* editor = session->editors.GetEditor(type)) {
        editor->Initialize();
      }
    }
  }
  return manager->window_manager().GetActiveSessionId();
}

TEST_F(PanelAdmissionRegistryTest, EveryRegisteredWindowIsAdmitted) {
  ::yaze::test::InitializeWithIsolatedSettings(*manager_, renderer_.get());
  const size_t session_id = RegisterRomFreeWindows(manager_.get());

  const auto& wm = manager_->window_manager();
  const size_t registered = wm.GetWindowsInSession(session_id).size();
  RecordProperty("registered_windows", static_cast<int>(registered));
  ASSERT_GT(registered, 10u) << "the audit saw almost nothing; the test setup "
                                "no longer registers editors";

  const auto findings = AuditPanelAdmission(wm, session_id);
  EXPECT_TRUE(findings.empty()) << findings.size() << " of " << registered
                                << " registered windows fail panel admission:\n"
                                << FormatPanelAdmissionFindings(findings);
}

// A window that leaves the window browser must stay one step away: the
// command palette lists a Show command and running it opens the window.
TEST_F(PanelAdmissionRegistryTest,
       HiddenWindowsStayOpenableFromTheCommandPalette) {
  ::yaze::test::InitializeWithIsolatedSettings(*manager_, renderer_.get());
  const size_t session_id = RegisterRomFreeWindows(manager_.get());
  auto& wm = manager_->window_manager();

  CommandPalette palette;
  palette.RegisterPanelCommands(&wm, session_id);
  const auto commands = palette.GetAllCommands();

  int checked = 0;
  for (const std::string& window_id : wm.GetWindowsInSession(session_id)) {
    const WindowDescriptor* descriptor =
        wm.GetWindowDescriptor(session_id, window_id);
    if (descriptor == nullptr || descriptor->IsListedInWindowBrowser() ||
        !descriptor->IsAdmitted()) {
      continue;
    }
    const std::string command_name = "Show: " + descriptor->display_name;
    const auto command = std::find_if(
        commands.begin(), commands.end(),
        [&](const CommandEntry& entry) { return entry.name == command_name; });
    ASSERT_NE(command, commands.end())
        << window_id << " is hidden from the window browser but has no '"
        << command_name << "' command";
    wm.CloseWindow(session_id, window_id);
    command->callback();
    EXPECT_TRUE(wm.IsWindowOpen(session_id, window_id))
        << window_id << ": '" << command_name << "' did not open the window";
    ++checked;
  }
  RecordProperty("hidden_windows_checked", checked);
  EXPECT_GT(checked, 0);
}

// Each demoted window keeps the role and host it was demoted with, and stays
// out of the window browser. The persisted-visibility half of the contract is
// pinned in user_settings_layout_defaults_test.cc from the same table.
TEST_F(PanelAdmissionRegistryTest, DemotedWindowsKeepTheirDeclaredHost) {
  ::yaze::test::InitializeWithIsolatedSettings(*manager_, renderer_.get());
  const size_t session_id = RegisterRomFreeWindows(manager_.get());
  const auto& wm = manager_->window_manager();

  int found = 0;
  for (const auto& expected : admission::kDemotedWindows) {
    const WindowDescriptor* descriptor =
        wm.GetWindowDescriptor(session_id, expected.window_id);
    if (descriptor == nullptr) {
      // Registered only with a ROM, a project, or another build option.
      continue;
    }
    ++found;
    EXPECT_EQ(descriptor->category, expected.category) << expected.window_id;
    EXPECT_EQ(descriptor->presentation.role, expected.role)
        << expected.window_id;
    EXPECT_NE(descriptor->presentation.host.find(expected.host_contains),
              std::string::npos)
        << expected.window_id << " host is '" << descriptor->presentation.host
        << "'";
    EXPECT_FALSE(descriptor->IsListedInWindowBrowser()) << expected.window_id;
  }
  RecordProperty("demoted_windows_registered", found);
}

}  // namespace
}  // namespace yaze::editor
