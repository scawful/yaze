#include "app/testing/graphics_ui_tests.h"

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE

#include <algorithm>
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
#include "app/gui/automation/widget_id_registry.h"
#include "core/gfx_sheet_source.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "nlohmann/json.hpp"

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
constexpr uint16_t kSheet = 0xDF;
constexpr uint16_t kNeighbor = 0xE4;
constexpr uint8_t kColor = 6;

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
  std::map<fs::path, std::string> files;
};

absl::StatusOr<Fixture> ObserveFixture(editor::EditorManager& manager,
                                       const fs::path& requested) {
  const auto refuse = [](const char* message) {
    return absl::FailedPreconditionError(message);
  };
  std::error_code ec;
  Fixture result;
  result.project = fs::canonical(requested, ec);
  if (ec || !requested.is_absolute() || requested != result.project ||
      result.project.filename() != "graphics-source-acceptance.yaze") {
    return refuse("Select an exact canonical private graphics fixture");
  }
  const auto* current_project = manager.GetCurrentProject();
  auto* data = manager.GetCurrentGameData();
  auto* rom = manager.GetCurrentRom();
  if (!current_project || !current_project->project_opened() ||
      fs::canonical(current_project->filepath, ec) != result.project || ec ||
      current_project->name.find("[Graphics source acceptance]") ==
          std::string::npos ||
      manager.GetProjectRomWritePolicy() != project::RomWritePolicy::kBlock ||
      current_project->hack_manifest.graphics_sheet_layout().pointer_layout ||
      !data || !data->gfx_sheet_source || !rom || !rom->is_loaded() ||
      rom->dirty()) {
    return refuse("Require the selected clean legacy source-overlay project");
  }
  const auto directory = result.project.parent_path();
  const auto source_root = fs::canonical(
      current_project->GetAbsolutePath(current_project->code_folder), ec);
  if (ec || !Inside(source_root, directory)) {
    return refuse("Fixture code folder must be private beneath its project");
  }
  result.rom = fs::canonical(rom->filename(), ec);
  if (ec || result.rom.parent_path() != directory ||
      result.rom.filename() != "rom.sfc" ||
      fs::canonical(
          current_project->GetAbsolutePath(current_project->rom_filename),
          ec) != result.rom ||
      ec) {
    return refuse("Require the fixture's actual private rom.sfc");
  }
  const auto& source = *data->gfx_sheet_source;
  if (!source.CheckBinding(*current_project).ok() || !source.CheckDisk().ok() ||
      !source.sheets().contains(kSheet) ||
      !source.sheets().contains(kNeighbor)) {
    return refuse("Require current bound DF..E4 source inputs");
  }
  result.spec = fs::canonical(source.spec_path(), ec);
  if (ec ||
      result.spec != source_root / "Data/graphics/source_sprite_sheets.json") {
    return refuse("Require the canonical fixture source spec");
  }
  result.df = fs::canonical(source.sheets().at(kSheet).path, ec);
  if (ec || result.df.extension() != ".txt" ||
      !Inside(result.df, result.spec.parent_path())) {
    return refuse("Require a private text-backed DF source");
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
  if (!Hex(Environment(kRomHash), 64) || !Hex(Environment(kSpecHash), 40) ||
      core::ComputeSourceArtifactSha256(result.files.at(result.rom)) !=
          Environment(kRomHash) ||
      core::GfxSheetSource::SpecSha1(result.files.at(result.spec)) !=
          Environment(kSpecHash)) {
    return refuse("Supply actual selected ROM/spec hashes; no defaults exist");
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

void RunGraphicsSourceTest(ImGuiTestContext* ctx, bool reopen) {
  const char* requested = Environment(reopen ? kReopenProject : kPaintProject);
  IM_CHECK(requested != nullptr);
  IM_CHECK(Environment(reopen ? kPaintProject : kReopenProject) == nullptr);
  const auto& config = Application::Instance().GetConfig();
  IM_CHECK(!config.headless && !config.service_mode);
  IM_CHECK(!config.enable_api && !config.editor_control &&
           !config.enable_test_harness);
  IM_CHECK(Environment("YAZE_LYRA_TERRAIN_ACCEPTANCE_PROJECT") == nullptr);
  IM_CHECK(Environment("YAZE_LYRA_TERRAIN_REOPEN_PROJECT") == nullptr);
  IM_CHECK(Environment("YAZE_GUI_BOAT_PROJECT") == nullptr);

  editor::EditorManager* manager = nullptr;
  for (int frame = 0; frame < 200; ++frame) {
    auto* controller = Application::Instance().GetController();
    manager = controller ? controller->editor_manager() : nullptr;
    auto* data = manager ? manager->GetCurrentGameData() : nullptr;
    if (manager && manager->GetCurrentProject()->project_opened() && data &&
        data->gfx_sheet_source && data->sheet_store.HasSheet(kSheet)) {
      break;
    }
    ctx->Yield(1);
  }
  IM_CHECK(manager != nullptr);
  const auto observed = ObserveFixture(*manager, requested);
  if (!observed.ok()) {
    IM_ERRORF("Graphics fixture refused: %s",
              observed.status().ToString().c_str());
    return;
  }
  const Fixture fixture = *observed;
  const auto session = manager->GetCurrentSessionId();
  auto* data = manager->GetCurrentGameData();
  IM_CHECK(data && data->sheet_store.HasSheet(kSheet));
  IM_CHECK(data->sheet_store.HasSheet(kNeighbor));
  IM_CHECK(!data->sheet_store.HasSheet(0xFF));
  IM_CHECK(data->sheet_store.Sheet(kSheet).size() == 4096);
  IM_CHECK(data->sheet_store.Sheet(kNeighbor).size() == 4096);
  IM_CHECK(!manager->IsCurrentProjectDirty());

  std::map<uint16_t, std::vector<uint8_t>> sheets_before;
  for (uint16_t id = 0; id < 0xFF; ++id) {
    if (data->sheet_store.HasSheet(id)) {
      const auto pixels = data->sheet_store.Sheet(id);
      sheets_before.emplace(id,
                            std::vector<uint8_t>(pixels.begin(), pixels.end()));
    }
  }
  auto expected = sheets_before.at(kSheet);
  IM_CHECK(std::all_of(expected.begin() + (reopen ? 1 : 0), expected.end(),
                       [](uint8_t pixel) { return pixel == 0; }));
  IM_CHECK_EQ(expected.front(), reopen ? kColor : 0);
  IM_CHECK(std::all_of(sheets_before.at(kNeighbor).begin(),
                       sheets_before.at(kNeighbor).end(),
                       [](uint8_t pixel) { return pixel == 0; }));

  // Reopen asserts the automatically loaded indexed store before any sheet
  // selection or canvas input. The native owner proves prior PID exit and
  // launches this separately with the actual saved spec hash.
  if (reopen) {
    const auto decoded =
        core::GfxSheetSource::DecodeText(fixture.files.at(fixture.df));
    IM_CHECK(decoded.ok() && *decoded == expected);
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
             "graphics.sheet.DF");
  }
  const auto browser_name = manager->window_manager().GetWorkspaceWindowName(
      session, "graphics.sheet_browser_v2");
  ctx->SetRef(browser_name.c_str());
  IM_CHECK(ctx->GetWindowByRef("") != nullptr);
  ctx->WindowFocus("");
  ctx->Yield(2);
  IM_CHECK(ctx->ItemExists("**/##SheetSearch"));
  ctx->ItemInputValue("**/##SheetSearch", "DF");
  ctx->Yield(3);
  IM_CHECK(!ctx->IsError());
  IM_CHECK(graphics->BuildContextSnapshot().semantic_owner ==
           "graphics.sheet.DF");

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
    const auto color = Control(ctx, window, "color", "Palette index 6");
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
    expected.front() = kColor;
    const auto pixels = data->sheet_store.Sheet(kSheet);
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
    IM_CHECK_EQ(data->sheet_store.Sheet(kSheet).front(), 0);
    IM_CHECK_EQ(graphics->undo_manager().UndoStackSize(), undo_size);
    IM_CHECK(graphics->CanRedo());
    const auto redo = Control(ctx, window, "button", "Redo pixels");
    IM_CHECK(redo != 0);
    ctx->ItemClick(redo);
    ctx->Yield(2);
    IM_CHECK_EQ(data->sheet_store.Sheet(kSheet).front(), kColor);
    IM_CHECK_EQ(graphics->undo_manager().UndoStackSize(), undo_size + 1);
    IM_CHECK_EQ(manager->GetCurrentSessionId(), session);
    IM_CHECK(manager->GetCurrentGameData() == data);

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
  IM_CHECK(loaded == fixture.files.at(fixture.rom));
  for (const auto& [id, before] : sheets_before) {
    const auto pixels = data->sheet_store.Sheet(id);
    const auto& wanted = id == kSheet ? expected : before;
    IM_CHECK(
        std::equal(pixels.begin(), pixels.end(), wanted.begin(), wanted.end()));
  }
  for (const auto& [path, before] : fixture.files) {
    if (!reopen && (path == fixture.project || path == fixture.spec ||
                    path == fixture.df)) {
      continue;
    }
    IM_CHECK(Read(path) == std::optional<std::string>(before));
  }

  // This is disk validation, not the separate-process reopen acceptance.
  project::YazeProject saved;
  IM_CHECK(saved.Open(fixture.project.string()).ok());
  const auto source = core::GfxSheetSource::Load(saved);
  IM_CHECK(source.ok());
  IM_CHECK(source->sheets().contains(kSheet));
  IM_CHECK(source->sheets().at(kSheet).pixels == expected);
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
    IM_CHECK(saved.graphics_sheets.source_spec_sha1 != Environment(kSpecHash));
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
  IM_CHECK(saved.graphics_sheets.source_spec_sha1 ==
           core::GfxSheetSource::SpecSha1(*spec_bytes));
  ctx->LogInfo(
      "Graphics source %s completed; project=%s spec_sha1=%s "
      "df_text_sha256=%s rom_sha256=%s",
      reopen ? "fresh-process observation" : "normal Save",
      fixture.project.string().c_str(),
      saved.graphics_sheets.source_spec_sha1.c_str(),
      core::ComputeSourceArtifactSha256(*df_bytes).c_str(),
      Environment(kRomHash));
  // The native owner must still observe normal Quit, PID/window closure and
  // the distinct reopen process. No receipt/context/guardian is created here.
}

}  // namespace

void RegisterGraphicsSourceUITests(ImGuiTestEngine* engine) {
  const bool paint = Environment(kPaintProject) != nullptr;
  const bool reopen = Environment(kReopenProject) != nullptr;
  if (!paint && !reopen) {
    return;
  }
  ImGuiTestEngine_GetIO(engine).ConfigLogToTTY = true;
  // Conflicting gates register only the first callback, which fails before
  // any UI action. An absent gate registers and queues nothing.
  ImGuiTest* test = IM_REGISTER_TEST(
      engine, "graphics_source",
      paint ? "df_pixel_undo_redo_normal_save" : "fresh_process_source_reopen");
  if (paint) {
    test->TestFunc = [](ImGuiTestContext* ctx) {
      RunGraphicsSourceTest(ctx, false);
    };
  } else {
    test->TestFunc = [](ImGuiTestContext* ctx) {
      RunGraphicsSourceTest(ctx, true);
    };
  }
  ImGuiTestEngine_QueueTest(engine, test, ImGuiTestRunFlags_RunFromCommandLine);
}

}  // namespace yaze::test

#endif  // YAZE_ENABLE_IMGUI_TEST_ENGINE
