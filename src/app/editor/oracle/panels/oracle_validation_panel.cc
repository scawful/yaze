#include "app/editor/oracle/panels/oracle_validation_panel.h"

#include <chrono>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/core/content_registry.h"
#include "app/editor/core/panel_registration.h"
#include "app/gui/core/icons.h"
#include "cli/handlers/game/oracle_menu_commands.h"
#include "cli/handlers/game/oracle_smoke_check_commands.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "rom/rom.h"

namespace yaze::editor {
namespace {
constexpr ImVec4 kGreen{0.2f, 0.75f, 0.3f, 1.0f};
constexpr ImVec4 kRed{0.85f, 0.2f, 0.2f, 1.0f};
constexpr ImVec4 kYellow{0.85f, 0.75f, 0.1f, 1.0f};
constexpr ImVec4 kGrey{0.55f, 0.55f, 0.55f, 1.0f};

ImVec4 BoolColor(bool flag) { return flag ? kGreen : kRed; }

const char* CheckStr(bool flag) { return flag ? "OK" : "X"; }

void DrawCheckBadge(const std::string& state) {
  if (state == "ran") {
    ImGui::TextColored(kGreen, "[ran]");
    return;
  }
  if (state == "skipped") {
    ImGui::TextColored(kGrey, "[skipped]");
    return;
  }
  ImGui::TextColored(kYellow, "[%s]", state.c_str());
}

void DrawOptionalBool(const char* label, const std::optional<bool>& flag) {
  ImGui::Text("  %s", label);
  ImGui::SameLine();
  if (flag.has_value()) {
    ImGui::TextColored(BoolColor(*flag), "%s", CheckStr(*flag));
    return;
  }
  ImGui::TextColored(kGrey, "-");
}
}  // namespace

OracleValidationPanel::~OracleValidationPanel() {
  if (pending_.valid()) {
    pending_.wait();
  }
}

std::string OracleValidationPanel::GetId() const { return "oracle.validation"; }

std::string OracleValidationPanel::GetDisplayName() const {
  return "Oracle Validation";
}

std::string OracleValidationPanel::GetIcon() const {
  return ICON_MD_VERIFIED_USER;
}

std::string OracleValidationPanel::GetEditorCategory() const {
  return "Oracle";
}

PanelCategory OracleValidationPanel::GetPanelCategory() const {
  return PanelCategory::CrossEditor;
}

float OracleValidationPanel::GetPreferredWidth() const { return 540.0f; }

void OracleValidationPanel::Draw(bool* p_open) {
  (void)p_open;
  PollPendingResult();
  DrawOptions();
  ImGui::Separator();
  DrawActionButtons();
  ImGui::Separator();
  DrawResults();
}

std::string OracleValidationPanel::DefaultRomPath() {
  if (auto* rom = ContentRegistry::Context::rom(); rom && rom->is_loaded()) {
    return rom->filename();
  }
  return "roms/oos168.sfc";
}

void OracleValidationPanel::PollPendingResult() {
  if (!running_) {
    return;
  }
  if (pending_.wait_for(std::chrono::milliseconds(0)) !=
      std::future_status::ready) {
    return;
  }
  last_result_ = pending_.get();
  running_ = false;
  status_message_ = last_result_->command_ok ? "Completed." : "Failed.";
}

Rom* OracleValidationPanel::GetRom() const {
  return ContentRegistry::Context::rom();
}

void OracleValidationPanel::LaunchRun(oracle_validation::RunMode mode) {
  if (running_) {
    return;
  }

  oracle_validation::SmokeOptions smoke_opts;
  smoke_opts.rom_path = rom_path_;
  smoke_opts.min_d6_track_rooms = min_d6_track_rooms_;
  smoke_opts.strict_readiness =
      (mode == oracle_validation::RunMode::kStrictReadiness);
  if (write_report_ && !report_path_.empty()) {
    smoke_opts.report_path = report_path_;
  }

  oracle_validation::PreflightOptions preflight_opts;
  preflight_opts.rom_path = rom_path_;
  preflight_opts.required_collision_rooms = required_collision_rooms_;
  if (write_report_ && !report_path_.empty()) {
    preflight_opts.report_path = report_path_;
  }

  Rom* rom_context = nullptr;
  if (rom_path_.empty()) {
    rom_context = GetRom();
  }

  running_ = true;
  status_message_ = "Running...";

  pending_ = std::async(std::launch::async,
                        [mode, smoke_opts, preflight_opts, rom_context]() {
                          oracle_validation::OracleRunResult result;
                          result.mode = mode;
                          result.timestamp =
                              oracle_validation::CurrentTimestamp();

                          if (mode == oracle_validation::RunMode::kPreflight) {
                            auto args = oracle_validation::BuildPreflightArgs(
                                preflight_opts);
                            result.cli_command = oracle_validation::BuildCliCommand(
                                "dungeon-oracle-preflight", args);

                            cli::handlers::DungeonOraclePreflightCommandHandler
                                handler;
                            auto status =
                                handler.Run(args, rom_context, &result.raw_output);
                            result.command_ok =
                                status.ok() ||
                                status.code() ==
                                    absl::StatusCode::kFailedPrecondition;
                            result.status_code = status.code();
                            if (!status.ok() &&
                                status.code() !=
                                    absl::StatusCode::kFailedPrecondition) {
                              result.error_message =
                                  std::string(status.message());
                              return result;
                            }
                            auto parsed = oracle_validation::ParsePreflightOutput(
                                result.raw_output);
                            if (parsed.ok()) {
                              result.preflight = *parsed;
                            } else {
                              result.json_parse_failed = true;
                            }
                            return result;
                          }

                          auto args =
                              oracle_validation::BuildSmokeArgs(smoke_opts);
                          result.cli_command = oracle_validation::BuildCliCommand(
                              "oracle-smoke-check", args);

                          cli::handlers::OracleSmokeCheckCommandHandler handler;
                          auto status =
                              handler.Run(args, rom_context, &result.raw_output);
                          result.command_ok =
                              status.ok() ||
                              status.code() ==
                                  absl::StatusCode::kFailedPrecondition;
                          result.status_code = status.code();
                          if (!status.ok() &&
                              status.code() !=
                                  absl::StatusCode::kFailedPrecondition) {
                            result.error_message = std::string(status.message());
                            return result;
                          }
                          auto parsed = oracle_validation::ParseSmokeCheckOutput(
                              result.raw_output);
                          if (parsed.ok()) {
                            result.smoke = *parsed;
                          } else {
                            result.json_parse_failed = true;
                          }
                          return result;
                        });
}

void OracleValidationPanel::DrawOptions() {
  ImGui::SeparatorText("Options");

  ImGui::SetNextItemWidth(320.0f);
  ImGui::InputText("ROM Path", &rom_path_);
  ImGui::SameLine();
  if (ImGui::SmallButton("From ROM")) {
    rom_path_ = DefaultRomPath();
  }

  ImGui::SetNextItemWidth(80.0f);
  ImGui::InputInt("Min D6 track rooms", &min_d6_track_rooms_);
  if (min_d6_track_rooms_ < 0) {
    min_d6_track_rooms_ = 0;
  }

  ImGui::SeparatorText("Preflight options");
  ImGui::SetNextItemWidth(200.0f);
  ImGui::InputText("Required rooms", &required_collision_rooms_);
  ImGui::SameLine();
  ImGui::TextDisabled("(e.g. 0x25,0x27)");

  ImGui::Checkbox("Write report file", &write_report_);
  if (write_report_) {
    ImGui::SetNextItemWidth(280.0f);
    ImGui::InputText("Report path", &report_path_);
  }
}

void OracleValidationPanel::DrawActionButtons() {
  Rom* rom = GetRom();
  const bool rom_missing =
      (rom == nullptr || !rom->is_loaded()) && rom_path_.empty();
  if (rom_missing) {
    ImGui::BeginDisabled();
  }
  if (running_) {
    ImGui::BeginDisabled();
  }

  if (ImGui::Button("Run Structural Smoke")) {
    LaunchRun(oracle_validation::RunMode::kStructural);
  }
  ImGui::SameLine();
  if (ImGui::Button("Run Strict Readiness")) {
    LaunchRun(oracle_validation::RunMode::kStrictReadiness);
  }
  ImGui::SameLine();
  if (ImGui::Button("Run Oracle Preflight")) {
    LaunchRun(oracle_validation::RunMode::kPreflight);
  }

  if (running_) {
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextColored(kYellow, "%s", status_message_.c_str());
  }
  if (rom_missing) {
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextColored(kRed, "Load a ROM first");
  }
}

void OracleValidationPanel::DrawResults() {
  if (!last_result_.has_value()) {
    ImGui::TextDisabled("No results yet. Run a check above.");
    return;
  }
  const auto& result = *last_result_;

  const char* mode_label =
      result.mode == oracle_validation::RunMode::kPreflight
          ? "Oracle Preflight"
          : (result.mode == oracle_validation::RunMode::kStrictReadiness
                 ? "Strict Readiness Smoke"
                 : "Structural Smoke");
  const bool overall_ok =
      result.smoke.has_value()
          ? result.smoke->ok
          : (result.preflight.has_value() && result.preflight->ok);

  ImGui::TextColored(overall_ok ? kGreen : kRed, "%s %s",
                     CheckStr(overall_ok), mode_label);
  ImGui::SameLine(0.0f, 16.0f);
  ImGui::TextColored(kGrey, "%s", result.timestamp.c_str());

  ImGui::SetNextItemWidth(380.0f);
  ImGui::InputText("##cli_cmd",
                   const_cast<char*>(result.cli_command.c_str()),
                   result.cli_command.size() + 1,
                   ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  if (ImGui::SmallButton("Copy")) {
    ImGui::SetClipboardText(result.cli_command.c_str());
  }

  if (!result.error_message.empty()) {
    ImGui::TextColored(kRed, ICON_MD_ERROR " %s",
                       result.error_message.c_str());
    ImGui::TextDisabled(
        "Hint: check that the ROM is loaded and the command is available.");
    DrawRawOutput(result);
    return;
  }

  if (result.json_parse_failed) {
    ImGui::TextColored(kYellow, ICON_MD_WARNING
                                " JSON parse failed - raw output:");
    DrawRawOutput(result);
    return;
  }

  if (result.smoke.has_value()) {
    DrawSmokeCards(*result.smoke);
  }
  if (result.preflight.has_value()) {
    DrawPreflightCards(*result.preflight);
  }
}

void OracleValidationPanel::DrawSmokeCards(
    const oracle_validation::SmokeResult& smoke) {
  if (ImGui::CollapsingHeader("D4 Zora Temple", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextColored(BoolColor(smoke.d4.structural_ok), "  Structural  %s",
                       CheckStr(smoke.d4.structural_ok));
    ImGui::Text("  Required rooms check:");
    ImGui::SameLine();
    DrawCheckBadge(smoke.d4.required_rooms_check);
    DrawOptionalBool("  Rooms 0x25/0x27 have collision:",
                     smoke.d4.required_rooms_ok);
  }

  if (ImGui::CollapsingHeader("D6 Goron Mines", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextColored(BoolColor(smoke.d6.meets_min_track_rooms),
                       "  Track rooms  %d / %d  %s",
                       smoke.d6.track_rooms_found, smoke.d6.min_track_rooms,
                       smoke.d6.meets_min_track_rooms ? "(ok)"
                                                      : "(below threshold)");
    ImGui::TextColored(BoolColor(smoke.d6.ok), "  Audit command  %s",
                       CheckStr(smoke.d6.ok));
  }

  if (ImGui::CollapsingHeader("D3 Kalyxo Castle", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("  Readiness check:");
    ImGui::SameLine();
    DrawCheckBadge(smoke.d3.readiness_check);
    DrawOptionalBool("  Room 0x32 has collision:", smoke.d3.ok);
  }
}

void OracleValidationPanel::DrawPreflightCards(
    const oracle_validation::PreflightResult& preflight) {
  if (ImGui::CollapsingHeader("Water Fill", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextColored(BoolColor(preflight.water_fill_region_ok),
                       "  Region present  %s",
                       CheckStr(preflight.water_fill_region_ok));
    ImGui::TextColored(BoolColor(preflight.water_fill_table_ok),
                       "  Table valid  %s",
                       CheckStr(preflight.water_fill_table_ok));
  }

  if (ImGui::CollapsingHeader("Custom Collision",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextColored(BoolColor(preflight.custom_collision_maps_ok),
                       "  Maps valid  %s",
                       CheckStr(preflight.custom_collision_maps_ok));
    ImGui::Text("  Required rooms check:");
    ImGui::SameLine();
    DrawCheckBadge(preflight.required_rooms_check);
    DrawOptionalBool("  Required rooms have data:",
                     preflight.required_rooms_ok);
  }

  if (!preflight.errors.empty()) {
    const std::string errors_label = absl::StrFormat(
        "Errors  (%d)###preflight_errors", preflight.error_count);
    if (ImGui::CollapsingHeader(errors_label.c_str())) {
      for (const auto& err : preflight.errors) {
        ImGui::TextColored(kRed, "  [%s]  %s", err.code.c_str(),
                           err.message.c_str());
        if (err.room_id.has_value()) {
          ImGui::SameLine();
          ImGui::TextColored(kGrey, " room %s", err.room_id->c_str());
        }
      }
    }
  }
}

void OracleValidationPanel::DrawRawOutput(
    const oracle_validation::OracleRunResult& result) {
  if (ImGui::CollapsingHeader("Raw Output (diagnostics)")) {
    ImGui::InputTextMultiline(
        "##raw", const_cast<char*>(result.raw_output.c_str()),
        result.raw_output.size() + 1, ImVec2(-1.0f, 120.0f),
        ImGuiInputTextFlags_ReadOnly);
  }
}

REGISTER_PANEL(OracleValidationPanel);

}  // namespace yaze::editor
