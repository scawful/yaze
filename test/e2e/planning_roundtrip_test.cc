#define IMGUI_DEFINE_MATH_OPERATORS

#include "e2e/planning_roundtrip_test.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/controller.h"
#include "app/editor/editor_manager.h"
#include "app/editor/planning/planning_editor.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gui/core/icons.h"
#include "core/project.h"
#include "core/project_plan.h"
#include "core/project_plan_agent.h"
#include "core/source_artifact_publisher.h"
#include "imgui/imgui_internal.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

namespace {

namespace fs = std::filesystem;
using yaze::Controller;
using yaze::core::PlanJson;
using yaze::core::PlanRouteStep;
using yaze::core::ProjectPlan;
using yaze::editor::EditorType;
using yaze::editor::PlanningEditor;

constexpr const char* kAnnotatedStep = "D4";
constexpr const char* kSecondStep = "D6";
constexpr const char* kDraftStep = "D7";
constexpr const char* kDecisionId = "d4-rescue";
constexpr const char* kD4Note = "GUI e2e: D4 note typed in the Route window";
constexpr const char* kD6Note = "GUI e2e: D6 note typed after an agent update";
constexpr const char* kD7Draft = "GUI e2e: D7 draft kept by a refused save";
constexpr const char* kExternalTitle =
    "Oracle plan (title edited outside yaze)";
constexpr const char* kAgentName = "planning-gui-e2e";
constexpr const char* kStepState = "GUI_E2E_AGENT_UPDATE";
constexpr const char* kDecisionState = "GUI_E2E_DECISION_MARKER";
constexpr const char* kConflict = "The plan file changed outside agent status";
constexpr const char* kReadOnly = "read-only; agents update this";
constexpr const char* kAgentHeader = ICON_MD_SMART_TOY " Agent status";

Controller* GetController(ImGuiTestContext* ctx) {
  return ctx && ctx->Test ? static_cast<Controller*>(ctx->Test->UserData)
                          : nullptr;
}

std::string ReadFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

void WriteFile(const fs::path& path, const std::string& text) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

std::string Sha(const std::string& text) {
  return yaze::core::ComputeSourceArtifactSha256(text);
}

PlanJson ParseJson(const std::string& text) {
  return PlanJson::parse(text, nullptr, /*allow_exceptions=*/false);
}

// JSON-pointer-like paths of every value that differs between the two plans.
void DiffJson(const PlanJson& a, const PlanJson& b, const std::string& at,
              std::vector<std::string>* out) {
  if (a.is_object() && b.is_object()) {
    for (auto it = a.begin(); it != a.end(); ++it) {
      auto other = b.find(it.key());
      if (other == b.end()) {
        out->push_back(at + "/" + it.key());
      } else {
        DiffJson(*it, *other, at + "/" + it.key(), out);
      }
    }
    for (auto it = b.begin(); it != b.end(); ++it) {
      if (a.find(it.key()) == a.end()) {
        out->push_back(at + "/" + it.key());
      }
    }
    return;
  }
  if (a.is_array() && b.is_array()) {
    for (size_t i = 0; i < std::max(a.size(), b.size()); ++i) {
      const std::string path = at + "/" + std::to_string(i);
      if (i >= a.size() || i >= b.size()) {
        out->push_back(path);
      } else {
        DiffJson(a[i], b[i], path, out);
      }
    }
    return;
  }
  if (a != b) {
    out->push_back(at.empty() ? "/" : at);
  }
}

std::vector<std::string> DiffPlans(const std::string& before,
                                   const std::string& after) {
  std::vector<std::string> paths;
  DiffJson(ParseJson(before), ParseJson(after), "", &paths);
  return paths;
}

bool Contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

// The text from `from` up to the next `to`, or "" when either is missing.
std::string Between(const std::string& text, const std::string& from,
                    const std::string& to) {
  const size_t start = text.find(from);
  if (start == std::string::npos) {
    return "";
  }
  const size_t end = text.find(to, start + from.size());
  return end == std::string::npos ? "" : text.substr(start, end - start);
}

std::string FirstLine(const std::string& text) {
  return text.substr(0, text.find('\n'));
}

std::string StepLabel(const PlanRouteStep& step) {
  return step.code.empty() ? step.title : step.code;
}

std::string ObservationState(const ProjectPlan& plan, const std::string& key) {
  const PlanJson* obs = yaze::core::FindPlanAgentObservation(plan, key);
  if (obs == nullptr) {
    return "";
  }
  auto state = obs->find("state");
  return state != obs->end() && state->is_string() ? state->get<std::string>()
                                                   : "";
}

size_t ObservationCount(const ProjectPlan& plan) {
  const PlanJson* section = yaze::core::FindPlanAgentStatus(plan);
  if (section == nullptr) {
    return 0;
  }
  auto obs = section->find("observations");
  return obs != section->end() && obs->is_object() ? obs->size() : 0;
}

// Text every visible window renders during one frame, read from Dear ImGui's
// own log. The agent blocks are plain text with no item IDs to query. Depth 0
// keeps the log from auto-opening tree nodes.
class FrameText {
 public:
  explicit FrameText(ImGuiContext* ui) : ui_(ui) {
    ImGuiContextHook begin;
    begin.Type = ImGuiContextHookType_NewFramePost;
    begin.Callback = &FrameText::OnNewFrame;
    begin.UserData = this;
    begin_hook_ = ImGui::AddContextHook(ui_, &begin);
    ImGuiContextHook end;
    end.Type = ImGuiContextHookType_EndFramePre;
    end.Callback = &FrameText::OnEndFrame;
    end.UserData = this;
    end_hook_ = ImGui::AddContextHook(ui_, &end);
  }
  ~FrameText() {
    ImGui::RemoveContextHook(ui_, begin_hook_);
    ImGui::RemoveContextHook(ui_, end_hook_);
  }
  FrameText(const FrameText&) = delete;
  FrameText& operator=(const FrameText&) = delete;

  std::string Capture(ImGuiTestContext* ctx) {
    text_.clear();
    done_ = false;
    armed_ = true;
    for (int i = 0; i < 10 && !done_; ++i) {
      ctx->Yield();
    }
    armed_ = false;
    return text_;
  }

 private:
  static void OnNewFrame(ImGuiContext* ui, ImGuiContextHook* hook) {
    auto* self = static_cast<FrameText*>(hook->UserData);
    if (!self->armed_ || self->logging_ || ui->LogEnabled) {
      return;
    }
    ui->LogBuffer.clear();
    ImGui::LogToBuffer(0);
    self->logging_ = true;
  }
  static void OnEndFrame(ImGuiContext* ui, ImGuiContextHook* hook) {
    auto* self = static_cast<FrameText*>(hook->UserData);
    if (!self->logging_) {
      return;
    }
    self->logging_ = false;
    if (ui->LogEnabled && (ui->LogFlags & ImGuiLogFlags_OutputBuffer)) {
      self->text_.assign(ui->LogBuffer.c_str());
      ImGui::LogFinish();
      self->done_ = true;
      self->armed_ = false;
    }
  }

  ImGuiContext* ui_;
  ImGuiID begin_hook_ = 0;
  ImGuiID end_hook_ = 0;
  bool armed_ = false;
  bool logging_ = false;
  bool done_ = false;
  std::string text_;
};

// Hash chain and rendered-text snapshots, mirrored to the test log and,
// when YAZE_PLANNING_E2E_EVIDENCE names a directory, to files there.
class Evidence {
 public:
  explicit Evidence(std::string test) : test_(std::move(test)) {
    if (const char* dir = std::getenv("YAZE_PLANNING_E2E_EVIDENCE");
        dir && *dir) {
      dir_ = dir;
      std::error_code ec;
      fs::create_directories(dir_, ec);
    }
  }
  ~Evidence() {
    if (!dir_.empty()) {
      WriteFile(dir_ / (test_ + "-steps.txt"), steps_);
    }
  }
  Evidence(const Evidence&) = delete;
  Evidence& operator=(const Evidence&) = delete;

  void Step(ImGuiTestContext* ctx, const std::string& line) {
    ctx->LogInfo("PlanningRoundtrip: %s", line.c_str());
    steps_ += line + "\n";
  }
  void Snapshot(const std::string& name, const std::string& text) {
    if (!dir_.empty()) {
      WriteFile(dir_ / (test_ + "-" + name + ".txt"), text);
    }
  }

 private:
  std::string test_;
  fs::path dir_;
  std::string steps_;
};

struct Fixture {
  fs::path project;
  fs::path plan;
  PlanningEditor* editor = nullptr;
  ImGuiWindow* overview = nullptr;
  ImGuiWindow* route = nullptr;
};

ImGuiWindow* WaitForPanel(ImGuiTestContext* ctx, Controller* controller,
                          const char* panel_id) {
  auto* manager = controller->editor_manager();
  std::string name;
  for (int frame = 0; frame < 120; ++frame) {
    name = manager->GetWindowManager()->GetWorkspaceWindowName(
        manager->GetCurrentSessionId(), panel_id);
    ImGuiWindow* window =
        name.empty() ? nullptr : ImGui::FindWindowByName(name.c_str());
    if (window != nullptr && window->WasActive) {
      return window;
    }
    ctx->Yield();
  }
  ctx->LogError("Panel %s: no active window named '%s' (category '%s')",
                panel_id, name.c_str(),
                manager->GetWindowManager()->GetActiveCategory().c_str());
  for (ImGuiWindow* w : ctx->UiContext->Windows) {
    const std::string window_name = w->Name;
    if (w->WasActive &&
        (Contains(window_name, "Plan") || Contains(window_name, "Route"))) {
      ctx->LogError("  active window: '%s'", w->Name);
    }
  }
  return nullptr;
}

// Opens YAZE_PLANNING_E2E_PROJECT (when not already open) and the Planning
// editor. Returns false, logging a skip, when the variable is unset.
bool OpenPlanning(ImGuiTestContext* ctx, Controller* controller, Fixture* f) {
  const char* project = std::getenv("YAZE_PLANNING_E2E_PROJECT");
  if (project == nullptr || *project == '\0') {
    ctx->LogWarning(
        "PlanningRoundtrip: SKIPPED; set YAZE_PLANNING_E2E_PROJECT to a "
        "disposable project copy");
    return false;
  }
  f->project = fs::absolute(project);
  auto* manager = controller->editor_manager();
  std::error_code ec;
  if (!fs::equivalent(manager->GetCurrentProject()->filepath, f->project, ec)) {
    const auto status = manager->OpenRomOrProject(f->project.string());
    if (!status.ok()) {
      ctx->LogError("Open %s failed: %s", f->project.string().c_str(),
                    std::string(status.message()).c_str());
      return false;
    }
    ctx->Yield(10);
  }
  f->plan = manager->GetCurrentProject()->PlanningFilePath();
  IM_CHECK_NO_RET(!f->plan.empty());

  // Below 900px wide (SDL's dummy video driver) the compact layout differs
  // from the native default the panel geometry checks assume.
  if (ImGui::GetMainViewport()->WorkSize.x < 900.0f &&
      controller->window_backend() != nullptr) {
    controller->window_backend()->SetSize(1536, 896);
    ctx->Yield(5);
  }
  // The Dashboard picker and the "Switch to Planning Editor" palette command
  // both open Planning through SwitchToEditor.
  if (manager->GetWindowManager()->GetActiveCategory() != "Planning") {
    manager->SwitchToEditor(EditorType::kPlanning);
    ctx->Yield(10);
  }
  IM_CHECK_NO_RET(manager->GetWindowManager()->GetActiveCategory() ==
                  "Planning");

  auto* editors = manager->GetCurrentEditorSet();
  f->editor = editors ? static_cast<PlanningEditor*>(
                            editors->GetEditor(EditorType::kPlanning))
                      : nullptr;
  f->overview = WaitForPanel(ctx, controller, "planning.overview");
  f->route = WaitForPanel(ctx, controller, "planning.route");
  IM_CHECK_NO_RET(f->editor != nullptr);
  IM_CHECK_NO_RET(f->overview != nullptr);
  IM_CHECK_NO_RET(f->route != nullptr);
  if (f->editor == nullptr || f->overview == nullptr || f->route == nullptr) {
    return false;
  }
  for (int frame = 0; frame < 60 && !f->editor->plan().has_value(); ++frame) {
    ctx->Yield();
  }
  IM_CHECK_NO_RET(f->editor->plan().has_value());
  return f->editor->plan().has_value() && !ctx->IsError();
}

void Show(ImGuiTestContext* ctx, ImGuiWindow* window) {
  ctx->WindowFocus(window->ID);
  ctx->Yield(2);
  ctx->SetRef(window);
}

int StepIndex(const Fixture& f, const char* id) {
  return f.editor->plan()->FindStep(id);
}

// Clicks the step's row in the Route window's step list.
void SelectStep(ImGuiTestContext* ctx, const Fixture& f, const char* id) {
  const int index = StepIndex(f, id);
  IM_CHECK(index >= 0);
  const PlanRouteStep& step = f.editor->plan()->route_steps[index];
  const std::string label =
      absl::StrFormat("%d. %s  %s", index + 1, StepLabel(step),
                      step.code.empty() ? "" : step.title);
  Show(ctx, f.route);
  // A child window's ID hashes its full name, so locate the step list child
  // instead of composing a path through the Route window.
  const std::string prefix = std::string(f.route->Name) + "/##step_list_";
  ImGuiWindow* list = nullptr;
  for (ImGuiWindow* w : ctx->UiContext->Windows) {
    if (w->WasActive && std::string(w->Name).rfind(prefix, 0) == 0) {
      list = w;
    }
  }
  IM_CHECK(list != nullptr);
  ctx->SetRef(list->ID);
  ctx->ItemClick(absl::StrFormat("$$%d/%s", index, label).c_str());
  ctx->Yield(2);
}

// Replaces the selected step's note by typing into the Route window's note
// field.
void TypeNote(ImGuiTestContext* ctx, const Fixture& f, const char* id,
              const char* text) {
  SelectStep(ctx, f, id);
  Show(ctx, f.route);
  ctx->ItemClick("##route_layout/step_detail/##note");
  ctx->KeyCharsReplace(text);
  ctx->Yield(2);
  IM_CHECK_STR_EQ(f.editor->plan()->route_steps[StepIndex(f, id)].note.c_str(),
                  text);
  IM_CHECK(f.editor->dirty());
}

// ctx->MenuClick() cannot find ImGui 1.92 menu windows ("<label>###Menu_00").
// Open the menu with a real click, locate its popup, and click the item.
bool ClickMainMenuItem(ImGuiTestContext* ctx, const char* menu,
                       const char* item) {
  ctx->SetRef("DockSpaceWindow");
  ctx->ItemClick((std::string("##MenuBar/") + menu).c_str());
  ctx->Yield(2);
  ImGuiWindow* popup = nullptr;
  const std::string suffix = std::string(menu) + "###Menu_00";
  for (ImGuiWindow* w : ctx->UiContext->Windows) {
    if (w->Active && suffix == w->Name) {
      popup = w;
    }
  }
  if (popup == nullptr) {
    ctx->LogError("Menu '%s' did not open", menu);
    return false;
  }
  ctx->SetRef(popup->ID);
  const ImGuiTestItemInfo target = ctx->ItemInfo(item);
  if (target.ID == 0) {
    return false;
  }
  ctx->MouseMoveToPos(
      ImVec2(ctx->UiContext->IO.MousePos.x, target.RectFull.GetCenter().y));
  ctx->ItemClick(item);
  ctx->Yield(3);
  return !ctx->IsError();
}

bool HasToast(Controller* controller, const std::string& prefix) {
  auto* toasts = controller->editor_manager()->toast_manager();
  for (size_t i = 0; i < toasts->pending_count(); ++i) {
    if (toasts->GetPendingToast(i)->message.rfind(prefix, 0) == 0) {
      return true;
    }
  }
  return false;
}

void SaveProjectFromMenu(ImGuiTestContext* ctx) {
  IM_CHECK(ClickMainMenuItem(ctx, "File", ICON_MD_SAVE " Save Project"));
  ctx->Yield(5);
}

// Writes the agent update and applies it the way agents do: through
// `z3ed plan-agent-apply` when YAZE_PLANNING_E2E_Z3ED names the binary,
// otherwise through the same core call that command makes.
void ApplyAgentUpdate(ImGuiTestContext* ctx, const Fixture& f,
                      const std::string& update_text, Evidence* evidence) {
  const fs::path update_path =
      f.plan.parent_path() / "planning-e2e-update.json";
  WriteFile(update_path, update_text);
  const char* z3ed = std::getenv("YAZE_PLANNING_E2E_Z3ED");
  if (z3ed != nullptr && *z3ed != '\0') {
    const fs::path output = f.plan.parent_path() / "planning-e2e-z3ed.json";
    const std::string command = absl::StrFormat(
        "\"%s\" plan-agent-apply --plan=\"%s\" --update=\"%s\" "
        "--format=json > \"%s\" 2>&1",
        z3ed, f.plan.string(), update_path.string(), output.string());
    const int status = std::system(command.c_str());
    const std::string out = ReadFile(output);
    evidence->Step(ctx,
                   absl::StrFormat("z3ed plan-agent-apply exit=%d", status));
    evidence->Snapshot("z3ed-plan-agent-apply", out);
    IM_CHECK_EQ(status, 0);
    IM_CHECK(Contains(out, "\"written\": true"));
    return;
  }
  auto update = yaze::core::PlanAgentUpdate::Parse(update_text);
  IM_CHECK(update.ok());
  auto result =
      yaze::core::ApplyPlanAgentUpdateToFile(f.plan.string(), *update, false);
  IM_CHECK(result.ok());
  IM_CHECK(result->written);
  evidence->Step(ctx, "agent update applied in-process (no z3ed path set)");
}

// Route window, selected step: the note field, then the read-only agent
// block, then the step's Remove button.
void CheckStepDetail(ImGuiTestContext* ctx, FrameText* frame, const Fixture& f,
                     const char* id, const std::string& note,
                     const std::string& state, Evidence* evidence) {
  SelectStep(ctx, f, id);
  Show(ctx, f.route);
  const std::string text = frame->Capture(ctx);
  evidence->Snapshot(std::string("route-") + id, text);
  const std::string detail =
      Between(text, "{ " + FirstLine(note), "Remove step");
  IM_CHECK(!detail.empty());
  const size_t note_end = detail.find('}');
  IM_CHECK(note_end != std::string::npos);
  const std::string below_note = detail.substr(note_end + 1);
  IM_CHECK(Contains(below_note, kAgentHeader));
  IM_CHECK(Contains(below_note, state));
  IM_CHECK(Contains(below_note, kReadOnly));
  // The agent block is text only: no input field ("{...}" in the log).
  IM_CHECK(below_note.find('{') == std::string::npos);

  const ImGuiTestItemInfo note_field =
      ctx->ItemInfo("##route_layout/step_detail/##note");
  const ImGuiTestItemInfo remove = ctx->ItemInfo(
      "##route_layout/step_detail/" ICON_MD_DELETE " Remove step");
  IM_CHECK(note_field.ID != 0 && remove.ID != 0);
  IM_CHECK((note_field.ItemFlags & ImGuiItemFlags_Disabled) == 0);
  IM_CHECK_GT(remove.RectFull.Min.y - note_field.RectFull.Max.y,
              ImGui::GetTextLineHeight() * 3);
  evidence->Step(
      ctx,
      absl::StrFormat("route %s: note field y=%.0f..%.0f, agent block "
                      "\"%s\" below it, Remove step y=%.0f",
                      id, note_field.RectFull.Min.y, note_field.RectFull.Max.y,
                      state, remove.RectFull.Min.y));
}

// Overview: route strip block for the selected step.
void CheckRouteStrip(ImGuiTestContext* ctx, FrameText* frame, const Fixture& f,
                     const char* id, const std::string& state,
                     Evidence* evidence) {
  SelectStep(ctx, f, id);
  Show(ctx, f.overview);
  const std::string text = frame->Capture(ctx);
  evidence->Snapshot(std::string("overview-strip-") + id, text);
  const auto& plan = *f.editor->plan();
  const PlanRouteStep& step = plan.route_steps[StepIndex(f, id)];
  const std::string strip = Between(
      text, "[ " + StepLabel(plan.route_steps.back()) + " ]", " Decisions (");
  IM_CHECK(Contains(strip, step.title));
  IM_CHECK(Contains(strip, kAgentHeader));
  IM_CHECK(Contains(strip, state));
  evidence->Step(ctx, absl::StrFormat("overview route strip %s shows agent "
                                      "state %s",
                                      id, state));
}

// Overview: the decision row, expanded, shows its agent block.
void CheckDecision(ImGuiTestContext* ctx, FrameText* frame, const Fixture& f,
                   const std::string& state, Evidence* evidence) {
  const auto& decisions = f.editor->plan()->decisions;
  int index = -1;
  for (size_t i = 0; i < decisions.size(); ++i) {
    if (decisions[i].id == kDecisionId) {
      index = static_cast<int>(i);
    }
  }
  IM_CHECK(index >= 0 && index + 1 < static_cast<int>(decisions.size()));
  Show(ctx, f.overview);
  ctx->ItemOpen(absl::StrFormat("$$%d/##decision", index).c_str());
  ctx->Yield(2);
  const std::string text = frame->Capture(ctx);
  evidence->Snapshot("overview-decision", text);
  const std::string row =
      Between(text, decisions[index].title, decisions[index + 1].title);
  IM_CHECK(Contains(row, kAgentHeader));
  IM_CHECK(Contains(row, state));
  IM_CHECK(Contains(row, kReadOnly));
  evidence->Step(ctx, absl::StrFormat("overview decision %s shows agent "
                                      "state %s",
                                      kDecisionId, state));
}

void CheckOverviewCount(ImGuiTestContext* ctx, FrameText* frame,
                        const Fixture& f, size_t count, Evidence* evidence) {
  Show(ctx, f.overview);
  const std::string text = frame->Capture(ctx);
  evidence->Snapshot(absl::StrFormat("overview-%zu-observations", count), text);
  IM_CHECK(Contains(text, kAgentHeader));
  IM_CHECK(Contains(text, absl::StrFormat("%zu observations, updated", count)));
  const ImGuiTestItemInfo node = ctx->ItemInfo("##agent_observations");
  IM_CHECK(node.ID != 0);
  IM_CHECK_STR_EQ(node.DebugLabel,
                  absl::StrFormat("Observations (%zu)", count).c_str());
  evidence->Step(ctx, absl::StrFormat("overview Agent status: %zu "
                                      "observations",
                                      count));
}

std::string StepNote(const std::string& source, const Fixture& f,
                     const char* id) {
  const PlanJson plan = ParseJson(source);
  return plan["route"]["steps"][StepIndex(f, id)].value("note", "");
}

void E2ETest_PlanningSharedWorkflow(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  Fixture f;
  if (!OpenPlanning(ctx, controller, &f)) {
    return;
  }
  Evidence evidence("SharedWorkflow");
  FrameText frame(ctx->UiContext);
  const std::string original = ReadFile(f.plan);
  evidence.Step(ctx, "plan " + f.plan.string());
  evidence.Step(ctx, "0 opened: sha256 " + Sha(original));
  const auto& plan = *f.editor->plan();
  const size_t observations = ObservationCount(plan);
  const std::string d4_state =
      ObservationState(plan, std::string("step:") + kAnnotatedStep);
  IM_CHECK(observations > 0 && !d4_state.empty());
  IM_CHECK(StepIndex(f, kSecondStep) >= 0 && StepIndex(f, kDraftStep) >= 0);

  // a. Overview shows the agent status section.
  CheckOverviewCount(ctx, &frame, f, observations, &evidence);

  // b. D4: read-only agent block below the editable note, and in the strip.
  CheckStepDetail(ctx, &frame, f, kAnnotatedStep,
                  plan.route_steps[StepIndex(f, kAnnotatedStep)].note, d4_state,
                  &evidence);
  CheckRouteStrip(ctx, &frame, f, kAnnotatedStep, d4_state, &evidence);
  if (ctx->IsError()) {
    return;
  }

  // c. Typed note + File > Save Project changes only that note.
  TypeNote(ctx, f, kAnnotatedStep, kD4Note);
  if (ctx->IsError()) {
    return;
  }
  SaveProjectFromMenu(ctx);
  IM_CHECK(!f.editor->dirty());
  IM_CHECK(HasToast(controller, "Project saved successfully"));
  const std::string after_note = ReadFile(f.plan);
  const auto note_diff = DiffPlans(original, after_note);
  IM_CHECK_EQ(note_diff.size(), 1u);
  IM_CHECK_STR_EQ(
      note_diff[0].c_str(),
      absl::StrFormat("/route/steps/%d/note", StepIndex(f, kAnnotatedStep))
          .c_str());
  IM_CHECK_STR_EQ(StepNote(after_note, f, kAnnotatedStep).c_str(), kD4Note);
  evidence.Step(ctx, "c saved D4 note: only " + note_diff[0] +
                         " changed; sha256 " + Sha(after_note));

  // d. Agent update from the new SHA while the editor is open, then a second
  //    typed note; the save keeps the agent change.
  PlanJson update = PlanJson::object();
  update["schema"] = std::string(yaze::core::kPlanAgentUpdateSchema);
  update["base_sha256"] = Sha(after_note);
  update["agent"] = kAgentName;
  update["observed_utc"] = "2026-10-27T23:45:00Z";
  PlanJson step_obs = PlanJson::object();
  step_obs["state"] = kStepState;
  step_obs["summary"] = "Applied while the Planning editor was open";
  step_obs["prs"] = PlanJson::array({320});
  PlanJson decision_obs = PlanJson::object();
  decision_obs["state"] = kDecisionState;
  decision_obs["summary"] = "Decision marker from the GUI roundtrip";
  update["observations"] = PlanJson::object();
  update["observations"][std::string("step:") + kSecondStep] = step_obs;
  update["observations"][std::string("decision:") + kDecisionId] = decision_obs;
  ApplyAgentUpdate(ctx, f, update.dump(1) + "\n", &evidence);
  if (ctx->IsError()) {
    return;
  }
  const std::string after_agent = ReadFile(f.plan);
  for (const auto& path : DiffPlans(after_note, after_agent)) {
    IM_CHECK(path.rfind("/agent_status", 0) == 0);
  }
  IM_CHECK(!f.editor->dirty());
  evidence.Step(ctx,
                "d agent update applied: only /agent_status changed; "
                "sha256 " +
                    Sha(after_agent));

  TypeNote(ctx, f, kSecondStep, kD6Note);
  if (ctx->IsError()) {
    return;
  }
  SaveProjectFromMenu(ctx);
  IM_CHECK(!f.editor->dirty());
  const std::string after_second = ReadFile(f.plan);
  const auto second_diff = DiffPlans(after_agent, after_second);
  IM_CHECK_EQ(second_diff.size(), 1u);
  IM_CHECK_STR_EQ(
      second_diff[0].c_str(),
      absl::StrFormat("/route/steps/%d/note", StepIndex(f, kSecondStep))
          .c_str());
  IM_CHECK(ParseJson(after_second)["agent_status"] ==
           ParseJson(after_agent)["agent_status"]);
  IM_CHECK_STR_EQ(StepNote(after_second, f, kAnnotatedStep).c_str(), kD4Note);
  evidence.Step(
      ctx, "d saved D6 note over the agent update: only " + second_diff[0] +
               " changed, agent_status kept; sha256 " + Sha(after_second));
  CheckOverviewCount(ctx, &frame, f, ObservationCount(*f.editor->plan()),
                     &evidence);
  CheckStepDetail(ctx, &frame, f, kSecondStep, kD6Note, kStepState, &evidence);
  CheckRouteStrip(ctx, &frame, f, kSecondStep, kStepState, &evidence);
  CheckDecision(ctx, &frame, f, kDecisionState, &evidence);
  if (ctx->IsError()) {
    return;
  }

  // e. An outside title edit refuses the save and keeps the draft; Reload
  //    then shows the file.
  const std::string old_title =
      "\"title\": " + PlanJson(f.editor->plan()->title).dump();
  std::string external = after_second;
  const size_t title_at = external.find(old_title);
  IM_CHECK(title_at != std::string::npos);
  external.replace(title_at, old_title.size(),
                   "\"title\": " + PlanJson(kExternalTitle).dump());
  WriteFile(f.plan, external);
  evidence.Step(ctx, "e external title edit: sha256 " + Sha(external));

  TypeNote(ctx, f, kDraftStep, kD7Draft);
  if (ctx->IsError()) {
    return;
  }
  SaveProjectFromMenu(ctx);
  IM_CHECK(f.editor->dirty());
  IM_CHECK_STR_EQ(
      f.editor->plan()->route_steps[StepIndex(f, kDraftStep)].note.c_str(),
      kD7Draft);
  IM_CHECK_STR_EQ(Sha(ReadFile(f.plan)).c_str(), Sha(external).c_str());
  IM_CHECK(HasToast(controller,
                    std::string("Failed to save project: ") + kConflict));
  Show(ctx, f.overview);
  const std::string refused = frame.Capture(ctx);
  evidence.Snapshot("overview-refused-save", refused);
  IM_CHECK(Contains(refused, kConflict));
  IM_CHECK(Contains(refused, "unsaved"));
  evidence.Step(ctx, "e save refused with \"" + std::string(kConflict) +
                         "...\"; draft kept; file unchanged");

  Show(ctx, f.overview);
  ctx->ItemClick(ICON_MD_REFRESH " Reload");
  ctx->Yield(2);
  ctx->ItemClick("//Discard plan edits?/Discard and reload");
  ctx->Yield(3);
  IM_CHECK(!f.editor->dirty());
  IM_CHECK_STR_EQ(f.editor->plan()->title.c_str(), kExternalTitle);
  IM_CHECK_STR_EQ(
      f.editor->plan()->route_steps[StepIndex(f, kDraftStep)].note.c_str(),
      StepNote(external, f, kDraftStep).c_str());
  Show(ctx, f.overview);
  const std::string reloaded = frame.Capture(ctx);
  evidence.Snapshot("overview-reloaded", reloaded);
  IM_CHECK(Contains(reloaded, kExternalTitle));
  IM_CHECK(!Contains(reloaded, kConflict));
  CheckStepDetail(ctx, &frame, f, kAnnotatedStep, kD4Note, d4_state, &evidence);
  IM_CHECK_STR_EQ(Sha(ReadFile(f.plan)).c_str(), Sha(external).c_str());
  evidence.Step(ctx,
                "e reload shows the external title, D4/D6 notes and "
                "agent status; sha256 " +
                    Sha(external));
}

void E2ETest_PlanningReopenPersists(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  Fixture f;
  if (!OpenPlanning(ctx, controller, &f)) {
    return;
  }
  Evidence evidence("ReopenPersists");
  FrameText frame(ctx->UiContext);
  const std::string source = ReadFile(f.plan);
  evidence.Step(ctx, "f reopened in a new process: sha256 " + Sha(source));
  const auto& plan = *f.editor->plan();
  IM_CHECK_STR_EQ(plan.title.c_str(), kExternalTitle);
  CheckOverviewCount(ctx, &frame, f, ObservationCount(plan), &evidence);
  CheckStepDetail(ctx, &frame, f, kAnnotatedStep, kD4Note,
                  ObservationState(plan, std::string("step:") + kAnnotatedStep),
                  &evidence);
  CheckStepDetail(ctx, &frame, f, kSecondStep, kD6Note, kStepState, &evidence);
  CheckDecision(ctx, &frame, f, kDecisionState, &evidence);
  Show(ctx, f.overview);
  IM_CHECK(Contains(frame.Capture(ctx), kExternalTitle));
  IM_CHECK(!f.editor->dirty());
  IM_CHECK_STR_EQ(Sha(ReadFile(f.plan)).c_str(), Sha(source).c_str());
  evidence.Step(ctx,
                "f notes, title and agent status persisted; opening "
                "wrote nothing");
}

}  // namespace

namespace yaze {
namespace test {
namespace e2e {

void RegisterPlanningRoundtripTests(ImGuiTestEngine* engine,
                                    Controller* controller) {
  ImGuiTest* shared =
      IM_REGISTER_TEST(engine, "PlanningRoundtrip", "SharedWorkflow");
  shared->TestFunc = E2ETest_PlanningSharedWorkflow;
  shared->UserData = controller;
  ImGuiTest* reopen =
      IM_REGISTER_TEST(engine, "PlanningRoundtrip", "ReopenPersists");
  reopen->TestFunc = E2ETest_PlanningReopenPersists;
  reopen->UserData = controller;
}

}  // namespace e2e
}  // namespace test
}  // namespace yaze
