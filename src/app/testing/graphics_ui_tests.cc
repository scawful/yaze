#include "app/testing/graphics_ui_tests.h"

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/application.h"
#include "app/controller.h"
#include "app/editor/editor_manager.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/registry/content_registry.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/testing/failed_run_quit.h"
#include "core/gfx_sheet_source.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "nlohmann/json.hpp"
#include "util/rom_hash.h"

#if __has_include("imgui_test_engine/imgui_te_context.h")
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#elif __has_include("imgui_te_context.h")
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#else
#error "ImGui Test Engine headers not found"
#endif

namespace yaze::test {
namespace {

namespace fs = std::filesystem;
constexpr char kPaintProject[] = "YAZE_GRAPHICS_SOURCE_ACCEPTANCE_PROJECT";
constexpr char kReopenProject[] = "YAZE_GRAPHICS_SOURCE_REOPEN_PROJECT";
constexpr char kRomHash[] = "YAZE_GRAPHICS_SOURCE_EXPECTED_ROM_SHA256";
constexpr char kSpecHash[] = "YAZE_GRAPHICS_SOURCE_EXPECTED_SPEC_SHA1";
constexpr char kEbPaintProject[] = "YAZE_GRAPHICS_EB_ACCEPTANCE_PROJECT";
constexpr char kEbReopenProject[] = "YAZE_GRAPHICS_EB_REOPEN_PROJECT";
constexpr char kEbRomHash[] = "YAZE_GRAPHICS_EB_EXPECTED_ROM_SHA256";
constexpr char kEbProjectHash[] = "YAZE_GRAPHICS_EB_EXPECTED_PROJECT_SHA256";
constexpr char kEbSpecHash[] = "YAZE_GRAPHICS_EB_EXPECTED_D4_SHA1";
constexpr char kEbAlternateHash[] = "YAZE_GRAPHICS_EB_EXPECTED_STANDARD_SHA1";
// Optional pair for fixtures whose EB art differs from the legacy fixture.
constexpr char kEbSheetHash[] = "YAZE_GRAPHICS_EB_EXPECTED_SHEET_SHA256";
constexpr char kEbEditedSheetHash[] = "YAZE_GRAPHICS_EB_EDITED_SHEET_SHA256";
constexpr char kMixedPaintProject[] =
    "YAZE_GRAPHICS_EB_MIXED_ACCEPTANCE_PROJECT";
constexpr char kMixedReopenProject[] = "YAZE_GRAPHICS_EB_MIXED_REOPEN_PROJECT";
constexpr uint16_t kSheet = 0xDF;
constexpr uint16_t kNeighbor = 0xE4;
constexpr uint8_t kColor = 6;

// What a gated EB run had established when it stopped.
struct GatedProgress {
  std::optional<fs::path> directory;  // owner-marked fixture directory
  std::optional<FixtureOwner> owner;  // verified fixture session
};

std::string ReceiptStem(bool reopen, bool mixed) {
  return std::string(mixed ? "graphics-source-eb-mixed-"
                           : "graphics-source-eb-") +
         (reopen ? "reopen" : "edit");
}

const char* Environment(const char* name) {
  const char* value = std::getenv(name);
  return value && *value ? value : nullptr;
}

bool Hex(const char* value, size_t length) {
  if (!value || std::char_traits<char>::length(value) != length) {
    return false;
  }
  return std::all_of(value, value + length, [](unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
  });
}

bool Inside(const fs::path& child, const fs::path& directory) {
  const auto relative = child.lexically_relative(directory);
  return !relative.empty() && relative != "." && *relative.begin() != "..";
}

std::optional<std::string> Read(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::nullopt;
  }
  std::string bytes{std::istreambuf_iterator<char>(input), {}};
  return input.bad() ? std::nullopt
                     : std::optional<std::string>(std::move(bytes));
}

struct Fixture {
  fs::path project;
  fs::path rom;
  fs::path spec;
  fs::path df;
  fs::path alternate_spec;
  std::string rom_identity;
  std::map<fs::path, std::string> files;
};

absl::StatusOr<Fixture> ObserveFixture(editor::EditorManager& manager,
                                       const fs::path& requested, bool eb,
                                       bool mixed) {
  const auto refuse = [](const char* message) {
    return absl::FailedPreconditionError(message);
  };
  std::error_code ec;
  Fixture result;
  result.project = fs::canonical(requested, ec);
  if (ec || !requested.is_absolute() || requested != result.project ||
      result.project.filename() !=
          (eb ? "project.yaze" : "graphics-source-acceptance.yaze")) {
    return refuse("Select an exact canonical private graphics fixture");
  }
  const auto* current_project = manager.GetCurrentProject();
  auto* data = manager.GetCurrentGameData();
  auto* rom = manager.GetCurrentRom();
  if (!current_project || !current_project->project_opened() ||
      fs::canonical(current_project->filepath, ec) != result.project || ec ||
      current_project->name.find(
          mixed ? "Oracle private EB mixed source terrain Save Build Run"
          : eb  ? "Oracle private EB source Save Build Run"
                : "[Graphics source acceptance]") == std::string::npos ||
      manager.GetProjectRomWritePolicy() != project::RomWritePolicy::kBlock ||
      current_project->hack_manifest.graphics_sheet_layout().pointer_layout ||
      !data || !data->gfx_sheet_source || !rom || !rom->is_loaded() ||
      rom->dirty()) {
    return refuse("Require the selected clean legacy source-overlay project");
  }
  if (eb) {
    if (current_project->rom_metadata.role != project::RomRole::kDev ||
        current_project->rom_metadata.expected_hash.empty()) {
      return refuse("Require the private development ROM identity");
    }
    result.rom_identity = current_project->rom_metadata.expected_hash;
  }
  const auto directory = result.project.parent_path();
  const auto source_root = fs::canonical(
      current_project->GetAbsolutePath(current_project->code_folder), ec);
  if (ec || !Inside(source_root, directory)) {
    return refuse("Fixture code folder must be private beneath its project");
  }
  result.rom = fs::canonical(rom->filename(), ec);
  if (ec || result.rom.parent_path() != directory ||
      result.rom.filename() != (eb ? "rom" : "rom.sfc") ||
      fs::canonical(
          current_project->GetAbsolutePath(current_project->rom_filename),
          ec) != result.rom ||
      ec) {
    return refuse("Require the fixture's actual private rom.sfc");
  }
  const auto& source = *data->gfx_sheet_source;
  if (!source.CheckBinding(*current_project).ok() || !source.CheckDisk().ok() ||
      !source.sheets().contains(eb ? 0xEB : kSheet) ||
      (!eb && !source.sheets().contains(kNeighbor))) {
    return refuse("Require current bound DF..E4 source inputs");
  }
  result.spec = fs::canonical(source.spec_path(), ec);
  if (ec ||
      result.spec !=
          source_root / (eb ? "Data/graphics/d4_zora_sprite_sheets.json"
                            : "Data/graphics/source_sprite_sheets.json")) {
    return refuse("Require the canonical fixture source spec");
  }
  result.df = fs::canonical(source.sheets().at(eb ? 0xEB : kSheet).path, ec);
  if (ec || result.df.extension() != ".txt" ||
      !Inside(result.df, result.spec.parent_path())) {
    return refuse("Require a private text-backed DF source");
  }
  if (eb) {
    result.alternate_spec = fs::canonical(
        source_root / "Data/graphics/source_sprite_sheets.json", ec);
    if (ec ||
        result.alternate_spec !=
            source_root / "Data/graphics/source_sprite_sheets.json" ||
        result.df != source_root / "Data/graphics/sky_overworld/gEB.txt") {
      return refuse("Require canonical EB source and both private specs");
    }
    const auto alternate = Read(result.alternate_spec);
    if (!alternate || !Hex(Environment(kEbAlternateHash), 40) ||
        core::GfxSheetSource::SpecSha1(*alternate) !=
            Environment(kEbAlternateHash)) {
      return refuse("Supply the actual standard spec hash");
    }
    result.files.emplace(result.alternate_spec, *alternate);
  }
  for (const auto& [name, expected] : source.inputs()) {
    const auto path = fs::canonical(name, ec);
    const auto actual = Read(path);
    if (ec || !Inside(path, source_root) || !actual || *actual != expected ||
        fs::equivalent(path, result.rom, ec) || ec ||
        fs::equivalent(path, result.project, ec) || ec) {
      return refuse("Fixture source identity or private containment failed");
    }
    result.files.emplace(path, *actual);
  }
  for (const auto& path :
       {result.project, result.rom, result.spec, result.df}) {
    const auto bytes = Read(path);
    if (!bytes) {
      return refuse("Cannot read an actual fixture input");
    }
    result.files[path] = *bytes;
  }
  if (!current_project->hack_manifest_file.empty()) {
    const auto manifest = fs::canonical(
        current_project->GetAbsolutePath(current_project->hack_manifest_file),
        ec);
    const auto bytes = Read(manifest);
    if (ec || !Inside(manifest, directory) || !bytes) {
      return refuse("Fixture manifest must be private and readable");
    }
    result.files.emplace(manifest, *bytes);
  }
  const auto* rom_hash = Environment(eb ? kEbRomHash : kRomHash);
  const auto* spec_hash = Environment(eb ? kEbSpecHash : kSpecHash);
  if (!Hex(rom_hash, 64) || !Hex(spec_hash, 40) ||
      core::ComputeSourceArtifactSha256(result.files.at(result.rom)) !=
          rom_hash ||
      core::GfxSheetSource::SpecSha1(result.files.at(result.spec)) !=
          spec_hash ||
      (eb && (!Hex(Environment(kEbProjectHash), 64) ||
              core::ComputeSourceArtifactSha256(result.files.at(
                  result.project)) != Environment(kEbProjectHash)))) {
    return refuse("Supply actual selected ROM/spec hashes; no defaults exist");
  }
  if (eb) {
    for (const auto& [path, bytes] : result.files) {
      (void)bytes;
      if (!fs::is_regular_file(path, ec) || ec ||
          fs::hard_link_count(path, ec) != 1 || ec) {
        return refuse(
            "EB fixture files must be private regular single-link files");
      }
    }
  }
  const std::string_view loaded(reinterpret_cast<const char*>(rom->data()),
                                rom->size());
  if (loaded != result.files.at(result.rom)) {
    return refuse("Loaded ROM differs from the pinned fixture body");
  }
  return result;
}

// Registry entries can outlive their last submitted ImGui item. Match the
// actual current window and require a unique visible enabled item this frame.
ImGuiID Control(ImGuiTestContext* ctx, ImGuiWindow* window, const char* type,
                const char* label) {
  ImGuiID result = 0;
  const auto normalized = gui::WidgetIdRegistry::NormalizeLabel(label);
  for (const auto& entry : gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
    const auto& info = entry.second;
    if (info.type != type || info.label != normalized || !info.enabled) {
      continue;
    }
    const auto item = ctx->ItemInfo(info.imgui_id, ImGuiTestOpFlags_NoError);
    if (item.ID == 0 || item.Window != window ||
        item.RectClipped.GetWidth() <= 0 || item.RectClipped.GetHeight() <= 0) {
      continue;
    }
    if (result != 0) {
      return 0;
    }
    result = item.ID;
  }
  return result;
}

std::vector<ImRect> ActiveToasts(ImGuiTestContext* ctx) {
  std::vector<ImRect> result;
  for (ImGuiWindow* window : ctx->UiContext->Windows) {
    if ((window->Active || window->WasActive) &&
        std::string_view(window->Name).starts_with("##toast_")) {
      result.push_back(window->Rect());
    }
  }
  return result;
}

// Toasts are pinned under the menu bar, cannot be moved aside by the test
// engine, hold their TTL while hovered, and reflow upward as earlier ones
// expire. Before a real click on `id`, park the mouse inside the item's window
// off every toast and wait for any toast covering the item to expire. The
// bound is the longest pending TTL; only a newly shown toast can raise it.
void WaitForToastsToClear(ImGuiTestContext* ctx, editor::ToastManager& toasts,
                          ImGuiID id) {
  const auto item = ctx->ItemInfo(id);
  IM_CHECK(item.ID != 0 && item.Window != nullptr);
  const auto covered = [&] {
    const auto rects = ActiveToasts(ctx);
    return std::any_of(rects.begin(), rects.end(), [&](const ImRect& toast) {
      return toast.Overlaps(item.RectClipped);
    });
  };
  if (!covered())
    return;
  for (size_t i = 0; i < toasts.pending_count(); ++i) {
    const auto* toast = toasts.GetPendingToast(i);
    ctx->LogInfo("Covering toast candidate %.1fs: %s", toast->ttl_seconds,
                 toast->message.c_str());
  }
  const auto& inner = item.Window->InnerRect;
  const ImVec2 park(inner.Min.x + 4.0f, inner.Max.y - 4.0f);
  for (const auto& toast : ActiveToasts(ctx))
    IM_CHECK(!toast.Contains(park));
  ctx->MouseMoveToPos(park);
  using Clock = std::chrono::steady_clock;
  auto deadline = Clock::now();
  std::optional<std::chrono::system_clock::time_point> newest;
  while (covered()) {
    const ImVec2 mouse = ctx->UiContext->IO.MousePos;
    for (const auto& toast : ActiveToasts(ctx))
      IM_CHECK(!toast.Contains(mouse));
    const auto now = Clock::now();
    const auto& history = toasts.GetHistory();
    const auto shown = history.empty() ? std::chrono::system_clock::time_point{}
                                       : history.front().timestamp;
    if (newest != shown) {
      newest = shown;
      float remaining = 0.0f;
      for (size_t i = 0; i < toasts.pending_count(); ++i)
        remaining = std::max(remaining, toasts.GetPendingToast(i)->ttl_seconds);
      deadline = std::max(
          deadline, now + std::chrono::duration_cast<Clock::duration>(
                              std::chrono::duration<float>(remaining + 1.0f)));
    }
    IM_CHECK(now < deadline);
    ctx->Yield(1);
  }
}

// The same lookup, Paint tool and OwMap click used by the maintained
// overworld Build/Run test. Model reads below are observations only.
void MixedTerrain(ImGuiTestContext* ctx, editor::EditorManager* manager,
                  bool reopen) {
  constexpr int kMap = 0xA0, kX = 0, kY = 128;
  manager->OpenEditorAndPanelsFromFlags(
      "Overworld", "overworld.canvas,overworld.tile16_selector");
  const auto canvas_name = manager->window_manager().GetWorkspaceWindowName(
      manager->GetCurrentSessionId(), "overworld.canvas");
  ctx->SetRef(canvas_name.c_str());
  editor::OverworldEditor* overworld = nullptr;
  for (int frame = 0; frame < 200; ++frame) {
    auto* editors = manager->GetCurrentEditorSet();
    overworld = editors ? editors->GetEditorAs<editor::OverworldEditor>(
                              editor::EditorType::kOverworld)
                        : nullptr;
    if (overworld && overworld->overworld().is_loaded() &&
        ctx->ItemExists("**/OwMap"))
      break;
    ctx->Yield(1);
  }
  IM_CHECK(overworld && overworld->overworld().is_loaded());
  IM_CHECK(!overworld->lyra_canvas_.loaded());
  IM_CHECK(kMap < overworld->overworld().materialized_map_count());
  overworld->LocateOverworldTile(kMap, kX, kY);
  ctx->Yield(3);
  auto& model = overworld->overworld();
  IM_CHECK_EQ(model.GetTile(kX, kY), reopen ? 0xD5 : 0x152);
  if (reopen)
    return;

  const auto selector = manager->window_manager().GetWorkspaceWindowName(
      manager->GetCurrentSessionId(), "overworld.tile16_selector");
  ctx->SetRef(selector.c_str());
  IM_CHECK(ctx->GetWindowByRef("") != nullptr);
  ctx->WindowFocus("");
  ctx->Yield(3);
  auto& widgets = gui::WidgetIdRegistry::Instance();
  auto& toasts = *manager->toast_manager();
  const auto visible = [&](const std::string& suffix) -> ImGuiID {
    const std::string tail = "/" + suffix;
    for (const auto& path : widgets.FindWidgets("*" + suffix)) {
      const bool match =
          path == suffix ||
          (path.size() > tail.size() &&
           path.compare(path.size() - tail.size(), tail.size(), tail) == 0);
      const ImGuiID id = match ? widgets.GetWidgetId(path) : 0;
      if (id) {
        const auto info = widgets.GetWidgetInfo(path);
        if (info && !info->window_name.empty())
          ctx->SetRef(info->window_name.c_str());
        if (ctx->ItemExists(id))
          return id;
      }
    }
    return 0;
  };
  const auto lookup = visible("OwBlocksetSelector/button:Lookup");
  IM_CHECK(lookup != 0);
  WaitForToastsToClear(ctx, toasts, lookup);
  ctx->ItemClick(lookup);
  ctx->Yield(2);
  const auto input = visible("OwBlocksetSelector/input:TileID");
  IM_CHECK(input != 0);
  WaitForToastsToClear(ctx, toasts, input);
  ctx->ItemInputValue(input, "D5");
  ctx->Yield(2);
  IM_CHECK_EQ(overworld->current_tile16_id(), 0xD5);
  auto paint = visible("OverworldToolbar/button:Paint");
  const bool overflow = paint == 0;
  if (overflow) {
    const auto tools = visible("OverworldToolbar/button:Tools");
    IM_CHECK(tools != 0);
    WaitForToastsToClear(ctx, toasts, tools);
    ctx->ItemClick(tools);
    ctx->Yield(2);
    paint = visible("OverworldToolbar/button:Paint");
  }
  IM_CHECK(paint != 0);
  WaitForToastsToClear(ctx, toasts, paint);
  ctx->ItemClick(paint);
  if (overflow)
    ctx->KeyPress(ImGuiKey_Escape);
  auto& canvas = overworld->ow_map_canvas();
  canvas.set_global_scale(1.0f);
  overworld->LocateOverworldTile(kMap, kX, kY);
  ctx->Yield(3);
  ctx->SetRef(canvas_name.c_str());
  auto terrain = ctx->ItemInfo("**/OwMap");
  IM_CHECK(terrain.ID != 0 && terrain.Window != nullptr);
  ctx->SetRef(terrain.Window);
  ctx->WindowFocus("");
  ctx->Yield(2);
  terrain = ctx->ItemInfo("**/OwMap");
  const auto origin = canvas.zero_point();
  const auto scroll = canvas.scrolling();
  const float cell = 16.0f * canvas.global_scale();
  const ImVec2 center(origin.x + scroll.x + (kX + 0.5f) * cell,
                      origin.y + scroll.y + (kY + 0.5f) * cell);
  IM_CHECK(terrain.RectClipped.Contains(center));
  const auto undo = overworld->undo_manager().UndoStackSize();
  ctx->MouseSetViewport(terrain.Window);
  ctx->MouseMoveToPos(center);
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield(3);
  IM_CHECK_EQ(overworld->undo_manager().UndoStackSize(), undo + 1);
  IM_CHECK_EQ(model.GetTile(kX, kY), 0xD5);
  ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Z);
  ctx->Yield(2);
  IM_CHECK_EQ(model.GetTile(kX, kY), 0x152);
  ctx->KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z);
  ctx->Yield(2);
  IM_CHECK_EQ(model.GetTile(kX, kY), 0xD5);
  IM_CHECK(manager->IsCurrentProjectDirty() ||
           manager->GetCurrentRom()->dirty());
}

void RunGraphicsSourceTest(ImGuiTestContext* ctx, bool reopen, bool eb,
                           bool mixed = false,
                           GatedProgress* progress = nullptr) {
  const auto sheet = static_cast<uint16_t>(eb ? 0xEB : kSheet);
  const uint8_t initial_color = eb ? 6 : 0;
  const uint8_t color_index = eb ? 7 : kColor;
  const char* requested =
      Environment(mixed ? (reopen ? kMixedReopenProject : kMixedPaintProject)
                  : eb  ? (reopen ? kEbReopenProject : kEbPaintProject)
                        : (reopen ? kReopenProject : kPaintProject));
  IM_CHECK(requested != nullptr);
  IM_CHECK(
      Environment(mixed ? (reopen ? kMixedPaintProject : kMixedReopenProject)
                  : eb ? (reopen ? kEbPaintProject : kEbReopenProject)
                       : (reopen ? kPaintProject : kReopenProject)) == nullptr);
  IM_CHECK(Environment(eb ? kPaintProject : kEbPaintProject) == nullptr);
  IM_CHECK(Environment(eb ? kReopenProject : kEbReopenProject) == nullptr);
  IM_CHECK(!mixed || (Environment(kEbPaintProject) == nullptr &&
                      Environment(kEbReopenProject) == nullptr));
  IM_CHECK(mixed || (Environment(kMixedPaintProject) == nullptr &&
                     Environment(kMixedReopenProject) == nullptr));
  const auto& config = Application::Instance().GetConfig();
  IM_CHECK(!config.headless && !config.service_mode);
  IM_CHECK(!config.enable_api && !config.editor_control &&
           !config.enable_test_harness);
  IM_CHECK(Environment("YAZE_LYRA_TERRAIN_ACCEPTANCE_PROJECT") == nullptr);
  IM_CHECK(Environment("YAZE_LYRA_TERRAIN_REOPEN_PROJECT") == nullptr);
  IM_CHECK(Environment("YAZE_GUI_BOAT_PROJECT") == nullptr);

  if (eb) {
    ctx->EngineIO->ConfigWatchdogKillTest = 300.0f;
    // The owner creates this phase marker only after observing the owned PID
    // and isolated window. No UI interaction is permitted while waiting.
    const fs::path requested_path(requested);
    IM_CHECK(requested_path.is_absolute());
    std::error_code ec;
    const auto project = fs::canonical(requested_path, ec);
    IM_CHECK(!ec && project == requested_path);
    const auto marker = project.parent_path() / "graphics-source-ui.ready";
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (!fs::exists(marker, ec) && !ec &&
           std::chrono::steady_clock::now() < deadline) {
      ctx->Yield(1);
    }
    IM_CHECK(!ec && fs::exists(marker));
    const auto status = fs::symlink_status(marker, ec);
    IM_CHECK(!ec && fs::is_regular_file(status) && !fs::is_symlink(status));
    IM_CHECK(fs::canonical(marker, ec) == marker && !ec);
    IM_CHECK(Read(marker) ==
             std::optional<std::string>(reopen ? "reopen\n" : "edit\n"));
    if (progress) {
      progress->directory = project.parent_path();
    }
  }

  editor::EditorManager* manager = nullptr;
  for (int frame = 0; frame < 200; ++frame) {
    auto* controller = Application::Instance().GetController();
    manager = controller ? controller->editor_manager() : nullptr;
    auto* data = manager ? manager->GetCurrentGameData() : nullptr;
    if (manager && manager->GetCurrentProject() &&
        manager->GetCurrentProject()->project_opened() && data &&
        data->gfx_sheet_source && data->sheet_store.HasSheet(sheet)) {
      break;
    }
    ctx->Yield(1);
  }
  IM_CHECK(manager != nullptr);
  const auto observed = ObserveFixture(*manager, requested, eb, mixed);
  if (!observed.ok()) {
    IM_ERRORF("Graphics fixture refused: %s",
              observed.status().ToString().c_str());
    return;
  }
  const Fixture fixture = *observed;
  const auto session = manager->GetCurrentSessionId();
  if (progress) {
    progress->owner = FixtureOwner{session, fixture.project};
  }
  auto* data = manager->GetCurrentGameData();
  IM_CHECK(data && data->sheet_store.HasSheet(sheet));
  if (!eb) {
    IM_CHECK(data->sheet_store.HasSheet(kNeighbor));
  }
  IM_CHECK(!data->sheet_store.HasSheet(0xFF));
  IM_CHECK(data->sheet_store.Sheet(sheet).size() == 4096);
  if (!eb) {
    IM_CHECK(data->sheet_store.Sheet(kNeighbor).size() == 4096);
  }
  IM_CHECK(!manager->IsCurrentProjectDirty());

  std::map<uint16_t, std::vector<uint8_t>> sheets_before;
  for (uint16_t id = 0; id < 0xFF; ++id) {
    if (data->sheet_store.HasSheet(id)) {
      const auto pixels = data->sheet_store.Sheet(id);
      sheets_before.emplace(id,
                            std::vector<uint8_t>(pixels.begin(), pixels.end()));
    }
  }
  auto expected = sheets_before.at(sheet);
  if (!eb) {
    IM_CHECK(std::all_of(expected.begin() + (reopen ? 1 : 0), expected.end(),
                         [](uint8_t pixel) { return pixel == 0; }));
    IM_CHECK(std::all_of(sheets_before.at(kNeighbor).begin(),
                         sheets_before.at(kNeighbor).end(),
                         [](uint8_t pixel) { return pixel == 0; }));
  }
  IM_CHECK_EQ(expected.front(), reopen ? color_index : initial_color);
  if (eb) {
    const char* sheet_pin = Environment(kEbSheetHash);
    const char* edited_pin = Environment(kEbEditedSheetHash);
    IM_CHECK((sheet_pin == nullptr) == (edited_pin == nullptr));
    IM_CHECK(!sheet_pin || (Hex(sheet_pin, 64) && Hex(edited_pin, 64)));
    const std::string sheet_hash =
        sheet_pin ? sheet_pin
                  : "061be2cad508703e8f4ec449c62567ed412e6a42406c3e132b2705c"
                    "afb158b91";
    const std::string edited_hash =
        edited_pin ? edited_pin
                   : "1e5fa70f55955b365bc8479901ee6ec4ca1cf12e24860bd3f81d21b"
                     "747e67c58";
    IM_CHECK(core::GfxSheetSource::DecodedSha256(expected) ==
             (reopen ? edited_hash : sheet_hash));
  }

  // Reopen asserts the automatically loaded indexed store before any sheet
  // selection or canvas input. The native owner proves prior PID exit and
  // launches this separately with the actual saved spec hash.
  if (reopen) {
    const auto decoded =
        core::GfxSheetSource::DecodeText(fixture.files.at(fixture.df));
    IM_CHECK(decoded.ok() && *decoded == expected);
    if (mixed) {
      const auto prior_bytes =
          Read(fixture.project.parent_path() /
               "graphics-source-eb-mixed-edit-result.json");
      IM_CHECK(prior_bytes);
      const auto prior = nlohmann::json::parse(*prior_bytes, nullptr, false);
      IM_CHECK(prior.is_object() && prior.value("result", "") == "PASS");
      IM_CHECK(prior.value("project", "") == fixture.project.string());
      IM_CHECK(prior.value("rom_sha256", "") == Environment(kEbRomHash));
      IM_CHECK(prior.value("project_sha256", "") ==
               Environment(kEbProjectHash));
      IM_CHECK(prior.value("d4_spec_sha1", "") == Environment(kEbSpecHash));
      IM_CHECK(prior.value("standard_spec_sha1", "") ==
               Environment(kEbAlternateHash));
      MixedTerrain(ctx, manager, true);
      IM_CHECK(!ctx->IsError());
    }
  }

  manager->OpenEditorAndPanelsFromFlags(
      "Graphics", "graphics.sheet_browser_v2,graphics.pixel_editor");
  ctx->Yield(4);
  auto* editors = manager->GetCurrentEditorSet();
  auto* graphics = editors ? editors->GetGraphicsEditor() : nullptr;
  IM_CHECK(graphics && graphics->game_data() == data);
  IM_CHECK(!graphics->HasPendingGraphicsChanges());

  if (!reopen) {
    IM_CHECK(graphics->BuildContextSnapshot().semantic_owner !=
             (eb ? "graphics.sheet.EB" : "graphics.sheet.DF"));
  }
  const auto browser_name = manager->window_manager().GetWorkspaceWindowName(
      session, "graphics.sheet_browser_v2");
  ctx->SetRef(browser_name.c_str());
  IM_CHECK(ctx->GetWindowByRef("") != nullptr);
  ctx->WindowFocus("");
  ctx->Yield(2);
  IM_CHECK(ctx->ItemExists("**/##SheetSearch"));
  ctx->ItemInputValue("**/##SheetSearch", eb ? "EB" : "DF");
  ctx->Yield(3);
  IM_CHECK(!ctx->IsError());
  IM_CHECK(graphics->BuildContextSnapshot().semantic_owner ==
           (eb ? "graphics.sheet.EB" : "graphics.sheet.DF"));

  const auto pixel_name = manager->window_manager().GetWorkspaceWindowName(
      session, "graphics.pixel_editor");
  ctx->SetRef(pixel_name.c_str());
  auto* window = ctx->GetWindowByRef("");
  IM_CHECK(window != nullptr);
  ctx->WindowFocus("");
  ctx->Yield(2);
  IM_CHECK(ctx->ItemExists("**/PixelEditorCanvas"));
  IM_CHECK(manager->GetCurrentEditor() == graphics);
  if (!reopen) {
    const auto pencil = Control(ctx, window, "button", "Pencil (B)");
    IM_CHECK(pencil != 0);
    ctx->ItemClick(pencil);
    ctx->Yield(2);
    IM_CHECK(ctx->ItemExists("**/##BrushSize"));
    ctx->ItemInputValue("**/##BrushSize", 1);
    const auto color = Control(ctx, window, "color",
                               eb ? "Palette index 7" : "Palette index 6");
    IM_CHECK(color != 0);
    ctx->ItemClick(color);
    const auto fit = Control(ctx, window, "button", "Fit sheet");
    IM_CHECK(fit != 0);
    ctx->ItemClick(fit);
    ctx->Yield(3);

    const auto canvas =
        ctx->ItemInfo("**/PixelEditorCanvas", ImGuiTestOpFlags_NoError);
    IM_CHECK(canvas.ID != 0 && canvas.Window != nullptr);
    const float scale_x = canvas.RectFull.GetWidth() / 128.0f;
    const float scale_y = canvas.RectFull.GetHeight() / 32.0f;
    IM_CHECK(scale_x >= 1 && std::fabs(scale_x - scale_y) < 0.01f);
    IM_CHECK(std::fabs(scale_x - std::round(scale_x)) < 0.01f);
    const ImVec2 point(canvas.RectFull.Min.x + scale_x * 0.5f,
                       canvas.RectFull.Min.y + scale_y * 0.5f);
    IM_CHECK(canvas.RectClipped.Contains(point));
    const auto undo_size = graphics->undo_manager().UndoStackSize();
    IM_CHECK_EQ(undo_size, 0u);
    IM_CHECK(!graphics->CanRedo());
    ctx->MouseSetViewport(canvas.Window);
    ctx->MouseMoveToPos(point);
    ctx->MouseDown(ImGuiMouseButton_Left);
    ctx->Yield(2);
    ctx->MouseUp(ImGuiMouseButton_Left);
    ctx->Yield(3);
    IM_CHECK(!ctx->IsError());
    expected.front() = color_index;
    const auto pixels = data->sheet_store.Sheet(sheet);
    IM_CHECK(std::equal(pixels.begin(), pixels.end(), expected.begin(),
                        expected.end()));
    IM_CHECK(manager->IsCurrentProjectDirty());
    IM_CHECK(graphics->HasPendingSourceChanges());
    IM_CHECK(graphics->BuildContextSnapshot().has_pending_changes);
    IM_CHECK_EQ(graphics->undo_manager().UndoStackSize(), undo_size + 1);

    const auto undo = Control(ctx, window, "button", "Undo pixels");
    IM_CHECK(undo != 0);
    ctx->ItemClick(undo);
    ctx->Yield(2);
    IM_CHECK_EQ(data->sheet_store.Sheet(sheet).front(), initial_color);
    IM_CHECK_EQ(graphics->undo_manager().UndoStackSize(), undo_size);
    IM_CHECK(graphics->CanRedo());
    const auto redo = Control(ctx, window, "button", "Redo pixels");
    IM_CHECK(redo != 0);
    ctx->ItemClick(redo);
    ctx->Yield(2);
    IM_CHECK_EQ(data->sheet_store.Sheet(sheet).front(), color_index);
    IM_CHECK_EQ(graphics->undo_manager().UndoStackSize(), undo_size + 1);
    IM_CHECK_EQ(manager->GetCurrentSessionId(), session);
    IM_CHECK(manager->GetCurrentGameData() == data);

    if (mixed) {
      MixedTerrain(ctx, manager, false);
      IM_CHECK(!ctx->IsError());
      IM_CHECK(graphics->HasPendingSourceChanges());
    }
    // Use the normal app shortcut. Never call a source publisher or fall back
    // to a direct Save method when this actual frontend route fails.
    ctx->KeyPress(ImGuiMod_Shortcut | ImGuiKey_S);
    for (int frame = 0; frame < 120 && manager->IsCurrentProjectDirty();
         ++frame) {
      ctx->Yield(1);
    }
    IM_CHECK(!ctx->IsError());
    IM_CHECK(!manager->IsCurrentProjectDirty());
    IM_CHECK(!graphics->HasPendingGraphicsChanges());
    IM_CHECK(!graphics->HasPendingSourceChanges());
    IM_CHECK(graphics->CanUndo());
  }

  IM_CHECK_EQ(manager->GetCurrentSessionId(), session);
  IM_CHECK(manager->GetCurrentGameData() == data);
  IM_CHECK(!manager->IsCurrentProjectDirty());
  IM_CHECK(!graphics->HasPendingGraphicsChanges());
  IM_CHECK(!manager->GetCurrentRom()->dirty());
  const std::string_view loaded(
      reinterpret_cast<const char*>(manager->GetCurrentRom()->data()),
      manager->GetCurrentRom()->size());
  const auto saved_rom = Read(fixture.rom);
  IM_CHECK(saved_rom);
  IM_CHECK(loaded == *saved_rom);
  if (mixed && !reopen) {
    IM_CHECK(*saved_rom != fixture.files.at(fixture.rom));
  } else {
    IM_CHECK(*saved_rom == fixture.files.at(fixture.rom));
  }
  for (const auto& [id, before] : sheets_before) {
    const auto pixels = data->sheet_store.Sheet(id);
    const auto& wanted = id == sheet ? expected : before;
    IM_CHECK(
        std::equal(pixels.begin(), pixels.end(), wanted.begin(), wanted.end()));
  }
  for (const auto& [path, before] : fixture.files) {
    if (!reopen && (path == fixture.project || (mixed && path == fixture.rom) ||
                    path == fixture.spec || path == fixture.df ||
                    (eb && path == fixture.alternate_spec))) {
      continue;
    }
    IM_CHECK(Read(path) == std::optional<std::string>(before));
  }

  // This is disk validation, not the separate-process reopen acceptance.
  project::YazeProject saved;
  IM_CHECK(saved.Open(fixture.project.string()).ok());
  const auto source = core::GfxSheetSource::Load(saved);
  IM_CHECK(source.ok());
  IM_CHECK(source->sheets().contains(sheet));
  IM_CHECK(source->sheets().at(sheet).pixels == expected);
  IM_CHECK(source->spec_path() == fixture.spec.string());
  IM_CHECK(
      data->gfx_sheet_source->CheckBinding(*manager->GetCurrentProject()).ok());
  IM_CHECK(data->gfx_sheet_source->CheckDisk().ok());
  const auto spec_bytes = Read(fixture.spec);
  const auto df_bytes = Read(fixture.df);
  IM_CHECK(spec_bytes && df_bytes);
  if (!reopen) {
    IM_CHECK(*spec_bytes != fixture.files.at(fixture.spec));
    IM_CHECK(*df_bytes != fixture.files.at(fixture.df));
    IM_CHECK(Read(fixture.project) !=
             std::optional<std::string>(fixture.files.at(fixture.project)));
    IM_CHECK(saved.graphics_sheets.source_spec_sha1 !=
             Environment(eb ? kEbSpecHash : kSpecHash));
    auto expected_spec =
        nlohmann::json::parse(fixture.files.at(fixture.spec), nullptr, false);
    const auto actual_spec = nlohmann::json::parse(*spec_bytes, nullptr, false);
    IM_CHECK(expected_spec.is_object() && actual_spec.is_object());
    IM_CHECK(expected_spec.contains("sheets") &&
             expected_spec["sheets"].is_array());
    size_t df_entries = 0;
    for (auto& entry : expected_spec["sheets"]) {
      IM_CHECK(entry.is_object() && entry.contains("source") &&
               entry["source"].is_string());
      std::error_code ec;
      const auto path = fs::canonical(
          fixture.spec.parent_path() / entry["source"].get<std::string>(), ec);
      IM_CHECK(!ec);
      if (path == fixture.df) {
        IM_CHECK(entry.contains("global_sheet") &&
                 entry.contains("decoded_sha256"));
        entry["decoded_sha256"] = core::GfxSheetSource::DecodedSha256(expected);
        ++df_entries;
      }
    }
    IM_CHECK_EQ(df_entries, 1u);
    IM_CHECK(actual_spec == expected_spec);
  }
  if (eb) {
    IM_CHECK(saved.rom_metadata.role == project::RomRole::kDev);
    if (mixed) {
      IM_CHECK(
          saved.rom_metadata.expected_hash ==
          util::ComputeRomHash(reinterpret_cast<const uint8_t*>(loaded.data()),
                               loaded.size()));
      if (!reopen)
        IM_CHECK(saved.rom_metadata.expected_hash != fixture.rom_identity);
    } else {
      IM_CHECK(saved.rom_metadata.expected_hash == fixture.rom_identity);
    }
    IM_CHECK(saved.name ==
             (mixed ? "Oracle private EB mixed source terrain Save Build Run"
                    : "Oracle private EB source Save Build Run"));
    IM_CHECK(fs::path(saved.rom_filename) == fixture.rom);
    IM_CHECK(fs::path(saved.code_folder) ==
             fixture.project.parent_path() / "source-native");
    IM_CHECK(saved.graphics_sheets.source_spec ==
             "Data/graphics/d4_zora_sprite_sheets.json");
    const auto alternate_bytes = Read(fixture.alternate_spec);
    IM_CHECK(alternate_bytes);
    auto standard = nlohmann::json::parse(*alternate_bytes, nullptr, false);
    const auto active = nlohmann::json::parse(*spec_bytes, nullptr, false);
    IM_CHECK(standard.is_object() && active.is_object());
    IM_CHECK(standard.contains("sheets") && standard["sheets"].is_array());
    IM_CHECK(active.contains("sheets") && active["sheets"].is_array());
    IM_CHECK(standard["sheets"].size() <= active["sheets"].size());
    ctx->LogInfo("EB standard_spec_sha1=%s d4_spec_sha1=%s",
                 core::GfxSheetSource::SpecSha1(*alternate_bytes).c_str(),
                 core::GfxSheetSource::SpecSha1(*spec_bytes).c_str());
    for (size_t i = 0; i < standard["sheets"].size(); ++i) {
      IM_CHECK(standard["sheets"][i] == active["sheets"][i]);
    }
    if (!reopen) {
      auto expected_standard = nlohmann::json::parse(
          fixture.files.at(fixture.alternate_spec), nullptr, false);
      IM_CHECK(expected_standard.is_object() &&
               expected_standard.contains("sheets"));
      size_t matches = 0;
      for (auto& entry : expected_standard["sheets"]) {
        if (entry.value("source", std::string()) == "sky_overworld/gEB.txt") {
          IM_CHECK(entry.contains("global_sheet") &&
                   entry.contains("decoded_sha256"));
          entry["decoded_sha256"] =
              core::GfxSheetSource::DecodedSha256(expected);
          ++matches;
        }
      }
      IM_CHECK_EQ(matches, 1u);
      IM_CHECK(standard == expected_standard);
      IM_CHECK(core::GfxSheetSource::SpecSha1(*alternate_bytes) !=
               Environment(kEbAlternateHash));
    }
  }
  IM_CHECK(saved.graphics_sheets.source_spec_sha1 ==
           core::GfxSheetSource::SpecSha1(*spec_bytes));
  std::string output_hash;
  std::string output_path;
  if (eb && reopen) {
    namespace Context = editor::ContentRegistry::Context;
    using editor::ProjectWorkflowState;
    IM_CHECK(saved.run_script.empty());
    std::error_code ec;
    const auto expected_output =
        fixture.project.parent_path() / "source-native/Roms/oos168x.sfc";
    const auto output =
        fs::weakly_canonical(saved.GetAbsolutePath(saved.build_target), ec);
    IM_CHECK(!ec && output == expected_output);
    IM_CHECK(Inside(output, fixture.project.parent_path()));
    IM_CHECK(Context::build_workflow_status().state !=
             ProjectWorkflowState::kSuccess);
    IM_CHECK(Context::run_workflow_status().state !=
             ProjectWorkflowState::kSuccess);
    const auto visible_button = [&](const char* label) -> ImGuiID {
      ImGuiID found = 0;
      for (const auto& entry :
           gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
        const auto& info = entry.second;
        if (info.type != "button" ||
            info.label != gui::WidgetIdRegistry::NormalizeLabel(label) ||
            !info.enabled)
          continue;
        const auto item =
            ctx->ItemInfo(info.imgui_id, ImGuiTestOpFlags_NoError);
        if (!item.ID || !item.Window || item.RectClipped.GetWidth() <= 0 ||
            item.RectClipped.GetHeight() <= 0)
          continue;
        if (found)
          return 0;
        found = Control(ctx, item.Window, "button", label);
        if (!found)
          return 0;
      }
      return found;
    };
    const auto wait_success = [&](auto getter, int seconds) -> bool {
      const auto deadline =
          std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
      while (std::chrono::steady_clock::now() < deadline && !ctx->IsError()) {
        const auto state = getter().state;
        if (state == ProjectWorkflowState::kFailure)
          return false;
        if (state == ProjectWorkflowState::kSuccess)
          return true;
        ctx->Yield(1);
      }
      return false;
    };
    ctx->EngineIO->ConfigWatchdogKillTest = 300.0f;
    manager->ShowProjectManagement();
    ctx->Yield(3);
    const auto build_button = visible_button("Build Project");
    IM_CHECK(build_button != 0);
    ctx->ItemClick(build_button);
    IM_CHECK(
        wait_success([] { return Context::build_workflow_status(); }, 180));
    const auto output_status = fs::symlink_status(output, ec);
    IM_CHECK(!ec && fs::is_regular_file(output_status) &&
             !fs::is_symlink(output_status));
    IM_CHECK(fs::canonical(output, ec) == expected_output && !ec);
    const auto output_bytes = Read(output);
    IM_CHECK(output_bytes && !output_bytes->empty());
    output_hash = core::ComputeSourceArtifactSha256(*output_bytes);
    output_path = output.string();
    const auto run_button = visible_button("Run Project Output");
    IM_CHECK(run_button != 0);
    ctx->ItemClick(run_button);
    IM_CHECK(wait_success([] { return Context::run_workflow_status(); }, 30));
    IM_CHECK(Read(output) == output_bytes);
    for (const auto& [path, before] : fixture.files) {
      IM_CHECK(Read(path) == std::optional<std::string>(before));
    }
    IM_CHECK(!manager->GetCurrentRom()->dirty());
  }
  ctx->LogInfo(
      "Graphics source %s completed; project=%s spec_sha1=%s "
      "df_text_sha256=%s rom_sha256=%s",
      reopen ? "fresh-process observation" : "normal Save",
      fixture.project.string().c_str(),
      saved.graphics_sheets.source_spec_sha1.c_str(),
      core::ComputeSourceArtifactSha256(*df_bytes).c_str(),
      core::ComputeSourceArtifactSha256(*saved_rom).c_str());
  if (eb) {
    const auto alternate_bytes = Read(fixture.alternate_spec);
    const auto project_bytes = Read(fixture.project);
    IM_CHECK(alternate_bytes && project_bytes);
    const auto receipt = fixture.project.parent_path() /
                         (ReceiptStem(reopen, mixed) + "-result.json");
    std::error_code ec;
    IM_CHECK(!fs::exists(receipt, ec) && !ec);
    const nlohmann::json result = {
        {"result", "PASS"},
        {"phase", reopen ? "reopen" : "edit"},
        {"project", fixture.project.string()},
        {"project_sha256", core::ComputeSourceArtifactSha256(*project_bytes)},
        {"rom_sha256", core::ComputeSourceArtifactSha256(*saved_rom)},
        {"source_sha256", core::ComputeSourceArtifactSha256(*df_bytes)},
        {"d4_spec_sha1", core::GfxSheetSource::SpecSha1(*spec_bytes)},
        {"standard_spec_sha1",
         core::GfxSheetSource::SpecSha1(*alternate_bytes)},
        {"build_output", output_path},
        {"build_output_sha256", output_hash}};
    std::ofstream file(receipt, std::ios::binary);
    IM_CHECK(file.is_open());
    file << result.dump(2) << '\n';
    file.close();
    IM_CHECK(!file.fail());
    // FrameGuiBegin executes this after the test callback has returned.
    // The owner must independently confirm the normal process/window exit.
    manager->QueueDeferredAction([manager] { manager->Quit(); });
  }
  // The native owner must still observe normal Quit, PID/window closure and
  // the distinct reopen process. No native Save receipt is fabricated here.
}

// A failed gated run publishes its own FAIL result and, once no Build or Run
// is active, requests the normal Quit so the native owner does not wait for
// its deadline. The result file name never matches a PASS receipt. Failures
// before the owner marker or fixture ownership, and runs with unsaved work,
// stay with the native supervisor.
void PublishGatedFailure(ImGuiTestContext* ctx, const GatedProgress& progress,
                         bool reopen, bool mixed) {
  if (!progress.directory) {
    ctx->LogWarning("Failure before the owner marker; no result or Quit");
    return;
  }
  const auto path =
      *progress.directory / (ReceiptStem(reopen, mixed) + "-failure.json");
  std::error_code ec;
  if (fs::exists(path, ec) || ec) {
    ctx->LogWarning("Failure result not written; %s already exists",
                    path.string().c_str());
  } else {
    ImGuiTextBuffer errors;
    ctx->TestOutput->Log.ExtractLinesForVerboseLevels(
        ImGuiTestVerboseLevel_Error, ImGuiTestVerboseLevel_Error, &errors);
    const nlohmann::json result = {
        {"result", "FAIL"},
        {"phase", reopen ? "reopen" : "edit"},
        {"test", ctx->Test->Name},
        {"fixture_observed", progress.owner.has_value()},
        {"errors", std::string(errors.c_str())}};
    std::ofstream file(path, std::ios::binary);
    file << result.dump(2, ' ', false, nlohmann::json::error_handler_t::replace)
         << '\n';
    file.close();
    ctx->LogWarning(
        file.fail() ? "Failure result write failed: %s" : "Failure result: %s",
        path.string().c_str());
  }
  auto* controller = Application::Instance().GetController();
  auto* manager = controller ? controller->editor_manager() : nullptr;
  if (!manager) {
    return;
  }
  QueueFailedRunQuit(manager, progress.owner);
}

void RunGatedGraphicsSourceTest(ImGuiTestContext* ctx, bool reopen,
                                bool mixed) {
  GatedProgress progress;
  RunGraphicsSourceTest(ctx, reopen, true, mixed, &progress);
  if (ctx->IsError()) {
    PublishGatedFailure(ctx, progress, reopen, mixed);
  }
}

}  // namespace

void RegisterGraphicsSourceUITests(ImGuiTestEngine* engine) {
  const bool mixed = Environment(kMixedPaintProject) != nullptr ||
                     Environment(kMixedReopenProject) != nullptr;
  const bool eb = mixed || Environment(kEbPaintProject) != nullptr ||
                  Environment(kEbReopenProject) != nullptr;
  const bool paint = Environment(mixed ? kMixedPaintProject
                                 : eb  ? kEbPaintProject
                                       : kPaintProject) != nullptr;
  const bool reopen = Environment(mixed ? kMixedReopenProject
                                  : eb  ? kEbReopenProject
                                        : kReopenProject) != nullptr;
  if (!paint && !reopen) {
    return;
  }
  ImGuiTestEngine_GetIO(engine).ConfigLogToTTY = true;
  // Conflicting gates register only the first callback, which fails before
  // any UI action. An absent gate registers and queues nothing.
  ImGuiTest* test =
      IM_REGISTER_TEST(engine, "graphics_source",
                       mixed ? (paint ? "eb_mixed_terrain_normal_save"
                                      : "eb_mixed_terrain_fresh_reopen")
                       : eb  ? (paint ? "eb_pixel_undo_redo_normal_save"
                                      : "eb_fresh_process_source_reopen")
                             : (paint ? "df_pixel_undo_redo_normal_save"
                                      : "fresh_process_source_reopen"));
  if (mixed && paint) {
    test->TestFunc = [](ImGuiTestContext* ctx) {
      RunGatedGraphicsSourceTest(ctx, false, true);
    };
  } else if (mixed) {
    test->TestFunc = [](ImGuiTestContext* ctx) {
      RunGatedGraphicsSourceTest(ctx, true, true);
    };
  } else if (eb && paint) {
    test->TestFunc = [](ImGuiTestContext* ctx) {
      RunGatedGraphicsSourceTest(ctx, false, false);
    };
  } else if (eb) {
    test->TestFunc = [](ImGuiTestContext* ctx) {
      RunGatedGraphicsSourceTest(ctx, true, false);
    };
  } else if (paint) {
    test->TestFunc = [](ImGuiTestContext* ctx) {
      RunGraphicsSourceTest(ctx, false, false);
    };
  } else {
    test->TestFunc = [](ImGuiTestContext* ctx) {
      RunGraphicsSourceTest(ctx, true, false);
    };
  }
  if (eb) {
    ImGuiTestEngine_GetIO(engine).ConfigRestoreFocusAfterTests = false;
    ImGuiTestEngine_QueueTest(engine, test,
                              ImGuiTestRunFlags_RunFromCommandLine);
  }
  // DF remains registered for an explicit engine-GUI start.
}

}  // namespace yaze::test

#endif  // YAZE_ENABLE_IMGUI_TEST_ENGINE
