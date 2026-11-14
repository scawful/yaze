#include "app/editor/hack/workflow/build_log_view.h"

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "util/i18n/tr.h"

namespace yaze::editor::workflow {

namespace {

ImVec4 SeverityColor(BuildLogSeverity severity) {
  switch (severity) {
    case BuildLogSeverity::kSuccess:
      return gui::GetSuccessColor();
    case BuildLogSeverity::kWarning:
      return gui::GetWarningColor();
    case BuildLogSeverity::kError:
      return gui::GetErrorColor();
    case BuildLogSeverity::kSkipped:
      return gui::GetTextDisabledVec4();
    case BuildLogSeverity::kInfo:
    default:
      return gui::GetTextSecondaryVec4();
  }
}

const char* SeverityIcon(BuildLogSeverity severity) {
  switch (severity) {
    case BuildLogSeverity::kSuccess:
      return ICON_MD_CHECK_CIRCLE;
    case BuildLogSeverity::kWarning:
      return ICON_MD_WARNING;
    case BuildLogSeverity::kError:
      return ICON_MD_ERROR;
    case BuildLogSeverity::kSkipped:
      return ICON_MD_REMOVE_CIRCLE_OUTLINE;
    case BuildLogSeverity::kInfo:
    default:
      return ICON_MD_CHEVRON_RIGHT;
  }
}

BuildLogSeverity ReceiptCheckSeverity(const BuildReceiptCheck& check) {
  const bool not_run =
      check.status == "unavailable" || check.status == "skipped";
  if (check.status == "passed") {
    return BuildLogSeverity::kSuccess;
  }
  if (check.status == "failed" || check.status == "error" ||
      (check.required && not_run)) {
    return BuildLogSeverity::kError;
  }
  return not_run ? BuildLogSeverity::kSkipped : BuildLogSeverity::kInfo;
}

bool IsProblem(BuildLogSeverity severity) {
  return severity == BuildLogSeverity::kWarning ||
         severity == BuildLogSeverity::kError;
}

// Continues the current row when `width` still fits, otherwise wraps, so the
// counts stay legible in a narrow sidebar.
void SameLineIfFits(float width) {
  ImGui::SameLine();
  if (ImGui::GetCursorPosX() + width > ImGui::GetContentRegionMax().x) {
    ImGui::NewLine();
  }
}

void DrawCount(BuildLogSeverity severity, int count, const char* label,
               bool continue_row) {
  const std::string text =
      absl::StrFormat("%s %d %s", SeverityIcon(severity), count, label);
  if (continue_row) {
    SameLineIfFits(ImGui::CalcTextSize(text.c_str()).x);
  }
  gui::ColoredTextF(
      count > 0 ? SeverityColor(severity) : gui::GetTextDisabledVec4(), "%s",
      text.c_str());
}

float SmallButtonWidth(const char* label) {
  return ImGui::CalcTextSize(label, nullptr, true).x +
         ImGui::GetStyle().FramePadding.x * 2.0f;
}

}  // namespace

void BuildLogView::SetLog(const std::string& log) {
  if (log == log_) {
    return;
  }
  log_ = log;
  summary_ = ParseBuildLog(log_);
  receipt_.reset();
  receipt_passed_ = 0;
  receipt_failed_ = 0;
  if (summary_.receipt_path.empty()) {
    return;
  }
  std::ifstream file(summary_.receipt_path);
  if (!file) {
    return;
  }
  const std::string json((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
  auto receipt = ParseBuildReceipt(json);
  if (!receipt.ok()) {
    return;
  }
  receipt_ = std::move(*receipt);
  for (const auto& check : receipt_->checks) {
    const BuildLogSeverity severity = ReceiptCheckSeverity(check);
    receipt_passed_ += severity == BuildLogSeverity::kSuccess ? 1 : 0;
    receipt_failed_ += severity == BuildLogSeverity::kError ? 1 : 0;
  }
}

void BuildLogView::Draw(const char* id, float height, bool follow_tail) {
  ImGui::PushID(id);
  DrawToolbar();
  ImGui::BeginChild("##build_log", ImVec2(0, height), true,
                    ImGuiWindowFlags_HorizontalScrollbar);
  if (show_raw_) {
    ImGui::TextUnformatted(log_.c_str());
  } else {
    DrawReceiptChecks();
    DrawSteps();
  }
  if (follow_tail && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0f);
  }
  ImGui::EndChild();
  ImGui::PopID();
}

void BuildLogView::DrawToolbar() {
  DrawCount(BuildLogSeverity::kSuccess,
            receipt_ ? receipt_passed_ : summary_.passed, tr("passed"),
            /*continue_row=*/false);
  DrawCount(BuildLogSeverity::kWarning, summary_.warnings, tr("warn"),
            /*continue_row=*/true);
  DrawCount(BuildLogSeverity::kError, summary_.errors + receipt_failed_,
            tr("err"), /*continue_row=*/true);
  DrawCount(BuildLogSeverity::kSkipped, summary_.skipped, tr("skipped"),
            /*continue_row=*/true);

  const float buttons_width =
      SmallButtonWidth(ICON_MD_FILTER_LIST) +
      SmallButtonWidth(ICON_MD_RECEIPT_LONG) +
      SmallButtonWidth(ICON_MD_CONTENT_COPY "##copy_build_log") +
      ImGui::GetStyle().ItemSpacing.x * 2.0f;
  const float right_edge = ImGui::GetContentRegionMax().x - buttons_width;
  ImGui::SameLine();
  if (ImGui::GetCursorPosX() <= right_edge) {
    ImGui::SameLine(right_edge);
  } else {
    ImGui::NewLine();
  }
  gui::ToggleIconButton(ICON_MD_FILTER_LIST, ICON_MD_FILTER_LIST,
                        &problems_only_, tr("Show only warnings and errors"));
  ImGui::SameLine();
  gui::ToggleIconButton(ICON_MD_RECEIPT_LONG, ICON_MD_RECEIPT_LONG, &show_raw_,
                        tr("Show raw output"));
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_CONTENT_COPY "##copy_build_log")) {
    ImGui::SetClipboardText(log_.c_str());
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tr("Copy full build output"));
  }
}

void BuildLogView::DrawReceiptChecks() {
  if (!receipt_ || receipt_->checks.empty()) {
    return;
  }
  const ImGuiTreeNodeFlags flags =
      receipt_failed_ > 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0;
  if (!ImGui::TreeNodeEx("##receipt_checks", flags, "%s %s (%d/%zu)",
                         ICON_MD_FACT_CHECK, tr("Build receipt checks"),
                         receipt_passed_, receipt_->checks.size())) {
    return;
  }
  if (!receipt_->state.empty()) {
    ImGui::TextColored(gui::GetTextDisabledVec4(), "%s, exit %d",
                       receipt_->state.c_str(), receipt_->exit_code);
  }
  for (const auto& check : receipt_->checks) {
    const BuildLogSeverity severity = ReceiptCheckSeverity(check);
    if (problems_only_ && !IsProblem(severity)) {
      continue;
    }
    gui::ColoredTextF(SeverityColor(severity), "%s %s%s",
                      SeverityIcon(severity), check.name.c_str(),
                      check.required ? "" : tr(" (optional)"));
    if (!check.detail.empty() && ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s: %s", check.status.c_str(), check.detail.c_str());
    }
  }
  ImGui::TreePop();
}

void BuildLogView::DrawSteps() {
  std::vector<int> visible;
  const float leaf_indent = ImGui::GetTreeNodeToLabelSpacing();
  for (size_t step_index = 0; step_index < summary_.steps.size();
       ++step_index) {
    const BuildLogStep& step = summary_.steps[step_index];
    if (problems_only_ && !IsProblem(step.severity)) {
      continue;
    }
    ImGui::PushID(static_cast<int>(step_index));
    if (step.lines.empty()) {
      ImGui::Indent(leaf_indent);
      gui::ColoredTextF(SeverityColor(step.severity), "%s %s",
                        SeverityIcon(step.severity), step.title.c_str());
      ImGui::Unindent(leaf_indent);
      ImGui::PopID();
      continue;
    }
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
    if (IsProblem(step.severity)) {
      flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }
    bool open = false;
    {
      gui::StyleColorGuard color(ImGuiCol_Text, SeverityColor(step.severity));
      open = ImGui::TreeNodeEx("##step", flags, "%s %s (%zu)",
                               SeverityIcon(step.severity), step.title.c_str(),
                               step.lines.size());
    }
    if (open) {
      visible.clear();
      for (size_t i = 0; i < step.lines.size(); ++i) {
        if (!problems_only_ || IsProblem(step.lines[i].severity)) {
          visible.push_back(static_cast<int>(i));
        }
      }
      ImGuiListClipper clipper;
      clipper.Begin(static_cast<int>(visible.size()));
      while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
          const BuildLogLine& line = step.lines[visible[row]];
          if (line.location.empty()) {
            gui::ColoredTextF(SeverityColor(line.severity), "%s %s",
                              SeverityIcon(line.severity), line.text.c_str());
          } else {
            gui::ColoredTextF(SeverityColor(line.severity), "%s %s  %s",
                              SeverityIcon(line.severity),
                              line.location.c_str(), line.text.c_str());
          }
        }
      }
      ImGui::TreePop();
    }
    ImGui::PopID();
  }
}

}  // namespace yaze::editor::workflow
