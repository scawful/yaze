#include "app/gui/ui_helpers.h"
#include "app/gui/theme_manager.h"
#include "app/gui/icons.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {

ImVec4 GetThemeColor(ImGuiCol idx) {
    return ImGui::GetStyle().Colors[idx];
}

ImVec4 GetSuccessColor() {
    const auto& theme = ThemeManager::Get().GetCurrentTheme();
    return ConvertColorToImVec4(theme.success);
}

ImVec4 GetWarningColor() {
    const auto& theme = ThemeManager::Get().GetCurrentTheme();
    return ConvertColorToImVec4(theme.warning);
}

ImVec4 GetErrorColor() {
    const auto& theme = ThemeManager::Get().GetCurrentTheme();
    return ConvertColorToImVec4(theme.error);
}

ImVec4 GetInfoColor() {
    const auto& theme = ThemeManager::Get().GetCurrentTheme();
    return ConvertColorToImVec4(theme.info);
}

ImVec4 GetAccentColor() {
    const auto& theme = ThemeManager::Get().GetCurrentTheme();
    return ConvertColorToImVec4(theme.accent);
}

void BeginField(const char* label) {
    ImGui::BeginGroup();
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
}

void EndField() {
    ImGui::PopItemWidth();
    ImGui::EndGroup();
}

bool IconButton(const char* icon, const char* label, const ImVec2& size) {
    std::string button_text = std::string(icon) + " " + std::string(label);
    return ImGui::Button(button_text.c_str(), size);
}

void HelpMarker(const char* desc) {
    ImGui::TextDisabled(ICON_MD_HELP_OUTLINE);
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void SeparatorText(const char* label) {
    ImGui::SeparatorText(label);
}

} // namespace gui
} // namespace yaze
