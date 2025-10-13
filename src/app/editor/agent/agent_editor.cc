#include "app/editor/agent/agent_editor.h"

#include <filesystem>
#include <fstream>
#include <memory>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "app/platform/asset_loader.h"
#include "app/editor/agent/agent_chat_widget.h"
#include "app/editor/agent/agent_collaboration_coordinator.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/toast_manager.h"
#include "app/gui/icons.h"
#include "app/rom.h"
#include "util/file_util.h"
#include "util/platform_paths.h"

#ifdef YAZE_WITH_GRPC
#include "app/editor/agent/network_collaboration_coordinator.h"
#endif

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace editor {

AgentEditor::AgentEditor() {
  type_ = EditorType::kAgent;
  chat_widget_ = std::make_unique<AgentChatWidget>();
  local_coordinator_ = std::make_unique<AgentCollaborationCoordinator>();
  prompt_editor_ = std::make_unique<TextEditor>();
  common_tiles_editor_ = std::make_unique<TextEditor>();

  // Initialize default configuration (legacy)
  current_config_.provider = "mock";
  current_config_.show_reasoning = true;
  current_config_.max_tool_iterations = 4;

  // Initialize default bot profile
  current_profile_.name = "Default Z3ED Bot";
  current_profile_.description = "Default bot for Zelda 3 ROM editing";
  current_profile_.provider = "mock";
  current_profile_.show_reasoning = true;
  current_profile_.max_tool_iterations = 4;
  current_profile_.max_retry_attempts = 3;
  current_profile_.tags = {"default", "z3ed"};

  // Setup text editors
  prompt_editor_->SetLanguageDefinition(
      TextEditor::LanguageDefinition::CPlusPlus());
  prompt_editor_->SetReadOnly(false);
  prompt_editor_->SetShowWhitespaces(false);

  common_tiles_editor_->SetLanguageDefinition(
      TextEditor::LanguageDefinition::CPlusPlus());
  common_tiles_editor_->SetReadOnly(false);
  common_tiles_editor_->SetShowWhitespaces(false);

  // Ensure profiles directory exists
  EnsureProfilesDirectory();
}

AgentEditor::~AgentEditor() = default;

void AgentEditor::Initialize() {
  // Base initialization
  EnsureProfilesDirectory();
}

absl::Status AgentEditor::Load() {
  // Load agent configuration from project/settings
  // Try to load all bot profiles
  auto profiles_dir = GetProfilesDirectory();
  if (std::filesystem::exists(profiles_dir)) {
    for (const auto& entry :
         std::filesystem::directory_iterator(profiles_dir)) {
      if (entry.path().extension() == ".json") {
        std::ifstream file(entry.path());
        if (file.is_open()) {
          std::string json_content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
          auto profile_or = JsonToProfile(json_content);
          if (profile_or.ok()) {
            loaded_profiles_.push_back(profile_or.value());
          }
        }
      }
    }
  }
  return absl::OkStatus();
}

absl::Status AgentEditor::Save() {
  // Save current profile
  return SaveBotProfile(current_profile_);
}

absl::Status AgentEditor::Update() {
  if (!active_)
    return absl::OkStatus();

  // Draw configuration dashboard
  DrawDashboard();

  // Chat widget is drawn separately (not here)

  return absl::OkStatus();
}

void AgentEditor::InitializeWithDependencies(ToastManager* toast_manager,
                                             ProposalDrawer* proposal_drawer,
                                             Rom* rom) {
  toast_manager_ = toast_manager;
  proposal_drawer_ = proposal_drawer;
  rom_ = rom;

  if (chat_widget_) {
    chat_widget_->SetToastManager(toast_manager);
    chat_widget_->SetProposalDrawer(proposal_drawer);
    if (rom) {
      chat_widget_->SetRomContext(rom);
    }
  }

  SetupChatWidgetCallbacks();
  SetupMultimodalCallbacks();
}

void AgentEditor::SetRomContext(Rom* rom) {
  rom_ = rom;
  if (chat_widget_) {
    chat_widget_->SetRomContext(rom);
  }
}

void AgentEditor::DrawDashboard() {
  if (!active_)
    return;

  // Animate retro effects
  ImGuiIO& io = ImGui::GetIO();
  pulse_animation_ += io.DeltaTime * 2.0f;
  scanline_offset_ += io.DeltaTime * 0.4f;
  if (scanline_offset_ > 1.0f) scanline_offset_ -= 1.0f;
  glitch_timer_ += io.DeltaTime * 5.0f;
  blink_counter_ = static_cast<int>(pulse_animation_ * 2.0f) % 2;

  // Pulsing glow for window
  float pulse = 0.5f + 0.5f * std::sin(pulse_animation_);
  ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(
    0.1f + 0.1f * pulse,
    0.2f + 0.15f * pulse,
    0.3f + 0.2f * pulse,
    1.0f
  ));

  ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
  ImGui::Begin(ICON_MD_SMART_TOY " AI AGENT PLATFORM [v0.4.x]", &active_,
               ImGuiWindowFlags_MenuBar);

  // Menu bar
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu(ICON_MD_MENU " File")) {
      if (ImGui::MenuItem(ICON_MD_SAVE " Save Profile")) {
        Save();
        if (toast_manager_) {
          toast_manager_->Show("Bot profile saved", ToastType::kSuccess);
        }
      }
      if (ImGui::MenuItem(ICON_MD_FILE_UPLOAD " Export Profile...")) {
        // TODO: Open file dialog for export
        if (toast_manager_) {
          toast_manager_->Show("Export functionality coming soon",
                               ToastType::kInfo);
        }
      }
      if (ImGui::MenuItem(ICON_MD_FILE_DOWNLOAD " Import Profile...")) {
        // TODO: Open file dialog for import
        if (toast_manager_) {
          toast_manager_->Show("Import functionality coming soon",
                               ToastType::kInfo);
        }
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(ICON_MD_VIEW_LIST " View")) {
      if (ImGui::MenuItem(ICON_MD_CHAT " Open Chat Window", "Ctrl+Shift+A")) {
        OpenChatWindow();
      }
      ImGui::Separator();
      ImGui::MenuItem(ICON_MD_EDIT " Show Prompt Editor", nullptr,
                      &show_prompt_editor_);
      ImGui::MenuItem(ICON_MD_FOLDER " Show Bot Profiles", nullptr,
                      &show_bot_profiles_);
      ImGui::MenuItem(ICON_MD_HISTORY " Show Chat History", nullptr,
                      &show_chat_history_);
      ImGui::MenuItem(ICON_MD_ANALYTICS " Show Metrics Dashboard", nullptr,
                      &show_metrics_dashboard_);
      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }

  // Compact tabbed interface (combined tabs)
  if (ImGui::BeginTabBar("AgentEditorTabs", ImGuiTabBarFlags_None)) {
    // Bot Studio Tab - Modular 3-column layout
    if (ImGui::BeginTabItem(ICON_MD_SMART_TOY " Bot Studio")) {
      ImGui::Spacing();

      // Three-column layout: Config+Status | Editors | Profiles
      ImGuiTableFlags table_flags = ImGuiTableFlags_Resizable |
                                    ImGuiTableFlags_BordersInnerV |
                                    ImGuiTableFlags_SizingStretchProp;

      if (ImGui::BeginTable("BotStudioLayout", 3, table_flags)) {
        ImGui::TableSetupColumn("Settings", ImGuiTableColumnFlags_WidthFixed,
                                320.0f);
        ImGui::TableSetupColumn("Editors", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Profiles", ImGuiTableColumnFlags_WidthFixed,
                                280.0f);
        ImGui::TableNextRow();

        // Column 1: AI Provider, Behavior, ROM, Tips, Metrics (merged!)
        ImGui::TableNextColumn();
        ImGui::PushID("SettingsColumn");

        // Provider settings (always visible)
        DrawConfigurationPanel();
        ImGui::Spacing();

        // Status cards (always visible)
        DrawStatusPanel();

        ImGui::PopID();

        // Column 2: Tabbed Editors
        ImGui::TableNextColumn();
        ImGui::PushID("EditorsColumn");

        if (ImGui::BeginTabBar("EditorTabs", ImGuiTabBarFlags_None)) {
          if (ImGui::BeginTabItem(ICON_MD_EDIT " System Prompt")) {
            DrawPromptEditorPanel();
            ImGui::EndTabItem();
          }

          if (ImGui::BeginTabItem(ICON_MD_GRID_ON " Common Tiles")) {
            DrawCommonTilesEditor();
            ImGui::EndTabItem();
          }

          if (ImGui::BeginTabItem(ICON_MD_ADD " New Prompt")) {
            DrawNewPromptCreator();
            ImGui::EndTabItem();
          }

          ImGui::EndTabBar();
        }

        ImGui::PopID();

        // Column 3: Bot Profiles
        ImGui::TableNextColumn();
        ImGui::PushID("ProfilesColumn");
        DrawBotProfilesPanel();
        ImGui::PopID();

        ImGui::EndTable();
      }

      ImGui::EndTabItem();
    }

    // Session Manager Tab (combines History + Metrics)
    if (ImGui::BeginTabItem(ICON_MD_HISTORY " Sessions & History")) {
      ImGui::Spacing();

      // Two-column layout
      if (ImGui::BeginTable(
              "SessionLayout", 2,
              ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("History", ImGuiTableColumnFlags_WidthStretch,
                                0.6f);
        ImGui::TableSetupColumn("Metrics", ImGuiTableColumnFlags_WidthStretch,
                                0.4f);
        ImGui::TableNextRow();

        // LEFT: Chat History
        ImGui::TableSetColumnIndex(0);
        DrawChatHistoryViewer();

        // RIGHT: Metrics
        ImGui::TableSetColumnIndex(1);
        DrawAdvancedMetricsPanel();

        ImGui::EndTable();
      }

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::End();
  
  // Pop the TitleBgActive color pushed at the beginning of DrawDashboard
  ImGui::PopStyleColor();
}

void AgentEditor::DrawConfigurationPanel() {

  // AI Provider Configuration
  if (ImGui::CollapsingHeader(ICON_MD_SETTINGS " AI Provider",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f),
                       ICON_MD_SMART_TOY " Provider Selection");
    ImGui::Spacing();

    // Provider buttons (large, visual)
    ImVec2 button_size(ImGui::GetContentRegionAvail().x / 3 - 8, 60);

    bool is_mock = (current_profile_.provider == "mock");
    bool is_ollama = (current_profile_.provider == "ollama");
    bool is_gemini = (current_profile_.provider == "gemini");

    if (is_mock)
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 0.8f));
    if (ImGui::Button(ICON_MD_SETTINGS " Mock", button_size)) {
      current_profile_.provider = "mock";
    }
    if (is_mock)
      ImGui::PopStyleColor();

    ImGui::SameLine();
    if (is_ollama)
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.4f, 0.8f));
    if (ImGui::Button(ICON_MD_CLOUD " Ollama", button_size)) {
      current_profile_.provider = "ollama";
    }
    if (is_ollama)
      ImGui::PopStyleColor();

    ImGui::SameLine();
    if (is_gemini)
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.196f, 0.6f, 0.8f, 0.8f));
    if (ImGui::Button(ICON_MD_SMART_TOY " Gemini", button_size)) {
      current_profile_.provider = "gemini";
    }
    if (is_gemini)
      ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Spacing();

    // Provider-specific settings
    if (current_profile_.provider == "ollama") {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.4f, 1.0f),
                         ICON_MD_SETTINGS " Ollama Settings");
      ImGui::Text("Model:");
      ImGui::SetNextItemWidth(-1);
      static char model_buf[128] = "qwen2.5-coder:7b";
      if (!current_profile_.model.empty()) {
        strncpy(model_buf, current_profile_.model.c_str(),
                sizeof(model_buf) - 1);
      }
      if (ImGui::InputTextWithHint("##ollama_model",
                                   "e.g., qwen2.5-coder:7b, llama3.2",
                                   model_buf, sizeof(model_buf))) {
        current_profile_.model = model_buf;
      }

      ImGui::Text("Host URL:");
      ImGui::SetNextItemWidth(-1);
      static char host_buf[256] = "http://localhost:11434";
      strncpy(host_buf, current_profile_.ollama_host.c_str(),
              sizeof(host_buf) - 1);
      if (ImGui::InputText("##ollama_host", host_buf, sizeof(host_buf))) {
        current_profile_.ollama_host = host_buf;
      }
    } else if (current_profile_.provider == "gemini") {
      ImGui::TextColored(ImVec4(0.196f, 0.6f, 0.8f, 1.0f),
                         ICON_MD_SMART_TOY " Gemini Settings");

      // Load from environment button
      if (ImGui::Button(ICON_MD_REFRESH " Load from Environment")) {
        const char* gemini_key = std::getenv("GEMINI_API_KEY");
        if (gemini_key) {
          current_profile_.gemini_api_key = gemini_key;
          ApplyConfig(current_config_);
          if (toast_manager_) {
            toast_manager_->Show("Gemini API key loaded", ToastType::kSuccess);
          }
        } else {
          if (toast_manager_) {
            toast_manager_->Show("GEMINI_API_KEY not found",
                                 ToastType::kWarning);
          }
        }
      }

      ImGui::Spacing();

      ImGui::Text("Model:");
      ImGui::SetNextItemWidth(-1);
      static char model_buf[128] = "gemini-1.5-flash";
      if (!current_profile_.model.empty()) {
        strncpy(model_buf, current_profile_.model.c_str(),
                sizeof(model_buf) - 1);
      }
      if (ImGui::InputTextWithHint("##gemini_model", "e.g., gemini-1.5-flash",
                                   model_buf, sizeof(model_buf))) {
        current_profile_.model = model_buf;
      }

      ImGui::Text("API Key:");
      ImGui::SetNextItemWidth(-1);
      static char key_buf[256] = "";
      if (!current_profile_.gemini_api_key.empty() && key_buf[0] == '\0') {
        strncpy(key_buf, current_profile_.gemini_api_key.c_str(),
                sizeof(key_buf) - 1);
      }
      if (ImGui::InputText("##gemini_key", key_buf, sizeof(key_buf),
                           ImGuiInputTextFlags_Password)) {
        current_profile_.gemini_api_key = key_buf;
      }
      if (!current_profile_.gemini_api_key.empty()) {
        ImGui::TextColored(ImVec4(0.133f, 0.545f, 0.133f, 1.0f),
                           ICON_MD_CHECK_CIRCLE " API key configured");
      }
    } else {
      ImGui::TextDisabled(ICON_MD_INFO " Mock mode - no configuration needed");
    }
  }

  // Behavior Settings
  if (ImGui::CollapsingHeader(ICON_MD_TUNE " Behavior",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox(ICON_MD_VISIBILITY " Show Reasoning",
                    &current_profile_.show_reasoning);
    ImGui::Checkbox(ICON_MD_ANALYTICS " Verbose Output",
                    &current_profile_.verbose);
    ImGui::SliderInt(ICON_MD_LOOP " Max Tool Iterations",
                     &current_profile_.max_tool_iterations, 1, 10);
    ImGui::SliderInt(ICON_MD_REFRESH " Max Retry Attempts",
                     &current_profile_.max_retry_attempts, 1, 10);
  }

  // Profile Metadata
  if (ImGui::CollapsingHeader(ICON_MD_INFO " Profile Info")) {
    ImGui::Text("Name:");
    static char name_buf[128];
    strncpy(name_buf, current_profile_.name.c_str(), sizeof(name_buf) - 1);
    if (ImGui::InputText("##profile_name", name_buf, sizeof(name_buf))) {
      current_profile_.name = name_buf;
    }

    ImGui::Text("Description:");
    static char desc_buf[256];
    strncpy(desc_buf, current_profile_.description.c_str(),
            sizeof(desc_buf) - 1);
    if (ImGui::InputTextMultiline("##profile_desc", desc_buf, sizeof(desc_buf),
                                  ImVec2(-1, 60))) {
      current_profile_.description = desc_buf;
    }

    ImGui::Text("Tags (comma-separated):");
    static char tags_buf[256];
    if (tags_buf[0] == '\0' && !current_profile_.tags.empty()) {
      std::string tags_str;
      for (size_t i = 0; i < current_profile_.tags.size(); ++i) {
        if (i > 0)
          tags_str += ", ";
        tags_str += current_profile_.tags[i];
      }
      strncpy(tags_buf, tags_str.c_str(), sizeof(tags_buf) - 1);
    }
    if (ImGui::InputText("##profile_tags", tags_buf, sizeof(tags_buf))) {
      // Parse comma-separated tags
      current_profile_.tags.clear();
      std::string tags_str(tags_buf);
      size_t pos = 0;
      while ((pos = tags_str.find(',')) != std::string::npos) {
        std::string tag = tags_str.substr(0, pos);
        // Trim whitespace
        tag.erase(0, tag.find_first_not_of(" \t"));
        tag.erase(tag.find_last_not_of(" \t") + 1);
        if (!tag.empty()) {
          current_profile_.tags.push_back(tag);
        }
        tags_str.erase(0, pos + 1);
      }
      if (!tags_str.empty()) {
        tags_str.erase(0, tags_str.find_first_not_of(" \t"));
        tags_str.erase(tags_str.find_last_not_of(" \t") + 1);
        if (!tags_str.empty()) {
          current_profile_.tags.push_back(tags_str);
        }
      }
    }
  }

  // Apply button
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.133f, 0.545f, 0.133f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.133f, 0.545f, 0.133f, 1.0f));
  if (ImGui::Button(ICON_MD_CHECK " Apply & Save Configuration",
                    ImVec2(-1, 40))) {
    // Update legacy config
    current_config_.provider = current_profile_.provider;
    current_config_.model = current_profile_.model;
    current_config_.ollama_host = current_profile_.ollama_host;
    current_config_.gemini_api_key = current_profile_.gemini_api_key;
    current_config_.verbose = current_profile_.verbose;
    current_config_.show_reasoning = current_profile_.show_reasoning;
    current_config_.max_tool_iterations = current_profile_.max_tool_iterations;

    ApplyConfig(current_config_);
    Save();

    if (toast_manager_) {
      toast_manager_->Show("Configuration applied and saved",
                           ToastType::kSuccess);
    }
  }
  ImGui::PopStyleColor(2);

  DrawMetricsPanel();
}

void AgentEditor::DrawStatusPanel() {
  // Always visible status cards (no collapsing)

  // Chat Status Card
  ImGui::BeginChild("ChatStatusCard", ImVec2(0, 100), true);
  ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f), ICON_MD_CHAT " Chat");
  ImGui::Separator();

  if (chat_widget_ && chat_widget_->is_active()) {
    ImGui::TextColored(ImVec4(0.133f, 0.545f, 0.133f, 1.0f),
                       ICON_MD_CHECK_CIRCLE " Active");
  } else {
    ImGui::TextDisabled(ICON_MD_CANCEL " Inactive");
  }

  ImGui::Spacing();
  if (ImGui::Button(ICON_MD_OPEN_IN_NEW " Open", ImVec2(-1, 0))) {
    OpenChatWindow();
  }
  ImGui::EndChild();

  ImGui::Spacing();

  // ROM Context Card
  ImGui::BeginChild("RomStatusCard", ImVec2(0, 100), true);
  ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f), ICON_MD_GAMEPAD " ROM");
  ImGui::Separator();

  if (rom_ && rom_->is_loaded()) {
    ImGui::TextColored(ImVec4(0.133f, 0.545f, 0.133f, 1.0f),
                       ICON_MD_CHECK_CIRCLE " Loaded");
    ImGui::TextDisabled("Title: %s", rom_->title().c_str());
    ImGui::TextDisabled("Tools: Ready");
  } else {
    ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f),
                       ICON_MD_WARNING " Not Loaded");
    ImGui::TextDisabled("Load ROM for AI tools");
  }
  ImGui::EndChild();

  ImGui::Spacing();

  // Quick Tips Card
  ImGui::BeginChild("QuickTipsCard", ImVec2(0, 150), true);
  ImGui::TextColored(ImVec4(0.196f, 0.6f, 0.8f, 1.0f),
                     ICON_MD_TIPS_AND_UPDATES " Quick Tips");
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::BulletText("Ctrl+H: Toggle chat popup");
  ImGui::BulletText("Ctrl+P: View proposals");
  ImGui::BulletText("Edit prompts in center");
  ImGui::BulletText("Create custom bots");
  ImGui::BulletText("Save/load chat sessions");

  ImGui::EndChild();
}

void AgentEditor::DrawMetricsPanel() {
  if (ImGui::CollapsingHeader(ICON_MD_ANALYTICS " Quick Metrics")) {
    if (chat_widget_) {
      // Get metrics from the chat widget's service
      ImGui::TextDisabled("View detailed metrics in the Metrics tab");
    } else {
      ImGui::TextDisabled("No metrics available");
    }
  }
}

void AgentEditor::DrawPromptEditorPanel() {
  ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f),
                     ICON_MD_EDIT " Prompt Editor");
  ImGui::Separator();
  ImGui::Spacing();

  // Compact prompt file selector
  ImGui::Text("File:");
  ImGui::SetNextItemWidth(-45);
  if (ImGui::BeginCombo("##prompt_file", active_prompt_file_.c_str())) {
    if (ImGui::Selectable("system_prompt.txt",
                          active_prompt_file_ == "system_prompt.txt")) {
      active_prompt_file_ = "system_prompt.txt";
      prompt_editor_initialized_ = false;
    }
    if (ImGui::Selectable("system_prompt_v2.txt",
                          active_prompt_file_ == "system_prompt_v2.txt")) {
      active_prompt_file_ = "system_prompt_v2.txt";
      prompt_editor_initialized_ = false;
    }
    if (ImGui::Selectable("system_prompt_v3.txt",
                          active_prompt_file_ == "system_prompt_v3.txt")) {
      active_prompt_file_ = "system_prompt_v3.txt";
      prompt_editor_initialized_ = false;
    }
    ImGui::EndCombo();
  }

  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_REFRESH)) {
    prompt_editor_initialized_ = false;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Reload from disk");
  }

  // Load prompt file if not initialized
  if (!prompt_editor_initialized_ && prompt_editor_) {
    std::string asset_path = "agent/" + active_prompt_file_;
    auto content_result = AssetLoader::LoadTextFile(asset_path);

    if (content_result.ok()) {
      prompt_editor_->SetText(*content_result);
      current_profile_.system_prompt = *content_result;
      prompt_editor_initialized_ = true;

      if (toast_manager_) {
        toast_manager_->Show(absl::StrFormat(ICON_MD_CHECK_CIRCLE " Loaded %s",
                                             active_prompt_file_),
                             ToastType::kSuccess, 2.0f);
      }
    } else {
      // Show detailed error in console
      std::cerr << "❌ Failed to load " << active_prompt_file_ << "\n";
      std::cerr << "   Error: " << content_result.status().message() << "\n";

      // Set placeholder with instructions
      std::string placeholder = absl::StrFormat(
          "# System prompt file not found: %s\n"
          "# Error: %s\n\n"
          "# Please ensure the file exists in:\n"
          "# - assets/agent/%s\n"
          "# - Or Contents/Resources/agent/%s (macOS bundle)\n\n"
          "# You can create a custom prompt here and save it to your bot "
          "profile.",
          active_prompt_file_, content_result.status().message(),
          active_prompt_file_, active_prompt_file_);

      prompt_editor_->SetText(placeholder);
      prompt_editor_initialized_ = true;
    }
  }

  ImGui::Spacing();

  // Text editor
  if (prompt_editor_) {
    ImVec2 editor_size = ImVec2(ImGui::GetContentRegionAvail().x,
                                ImGui::GetContentRegionAvail().y - 50);
    prompt_editor_->Render("##prompt_editor", editor_size, true);

    // Save button
    ImGui::Spacing();
    if (ImGui::Button(ICON_MD_SAVE " Save Prompt to Profile", ImVec2(-1, 0))) {
      current_profile_.system_prompt = prompt_editor_->GetText();
      if (toast_manager_) {
        toast_manager_->Show("System prompt saved to profile",
                             ToastType::kSuccess);
      }
    }
  }

  ImGui::Spacing();
  ImGui::TextWrapped(
      "Edit the system prompt that guides the AI agent's behavior. Changes are "
      "saved to the current bot profile.");
}

void AgentEditor::DrawBotProfilesPanel() {
  ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f),
                     ICON_MD_FOLDER " Bot Profile Manager");
  ImGui::Separator();
  ImGui::Spacing();

  // Current profile display
  ImGui::BeginChild("CurrentProfile", ImVec2(0, 150), true);
  ImGui::TextColored(ImVec4(0.196f, 0.6f, 0.8f, 1.0f),
                     ICON_MD_STAR " Current Profile");
  ImGui::Separator();
  ImGui::Text("Name: %s", current_profile_.name.c_str());
  ImGui::Text("Provider: %s", current_profile_.provider.c_str());
  if (!current_profile_.model.empty()) {
    ImGui::Text("Model: %s", current_profile_.model.c_str());
  }
  ImGui::TextWrapped("Description: %s",
                     current_profile_.description.empty()
                         ? "No description"
                         : current_profile_.description.c_str());
  ImGui::EndChild();

  ImGui::Spacing();

  // Profile management buttons
  if (ImGui::Button(ICON_MD_ADD " Create New Profile", ImVec2(-1, 0))) {
    // Create new profile from current
    BotProfile new_profile = current_profile_;
    new_profile.name = "New Profile";
    new_profile.created_at = absl::Now();
    new_profile.modified_at = absl::Now();
    current_profile_ = new_profile;
    if (toast_manager_) {
      toast_manager_->Show("New profile created. Configure and save it.",
                           ToastType::kInfo);
    }
  }

  ImGui::Spacing();

  // Saved profiles list
  ImGui::TextColored(ImVec4(0.196f, 0.6f, 0.8f, 1.0f),
                     ICON_MD_LIST " Saved Profiles");
  ImGui::Separator();

  ImGui::BeginChild("ProfilesList", ImVec2(0, 0), true);

  if (loaded_profiles_.empty()) {
    ImGui::TextDisabled(
        "No saved profiles. Create and save a profile to see it here.");
  } else {
    for (size_t i = 0; i < loaded_profiles_.size(); ++i) {
      const auto& profile = loaded_profiles_[i];
      ImGui::PushID(static_cast<int>(i));

      bool is_current = (profile.name == current_profile_.name);
      if (is_current) {
        ImGui::PushStyleColor(ImGuiCol_Button,
                              ImVec4(0.196f, 0.6f, 0.8f, 0.6f));
      }

      if (ImGui::Button(profile.name.c_str(),
                        ImVec2(ImGui::GetContentRegionAvail().x - 80, 0))) {
        LoadBotProfile(profile.name);
        if (toast_manager_) {
          toast_manager_->Show(
              absl::StrFormat("Loaded profile: %s", profile.name),
              ToastType::kSuccess);
        }
      }

      if (is_current) {
        ImGui::PopStyleColor();
      }

      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 0.6f));
      if (ImGui::SmallButton(ICON_MD_DELETE)) {
        DeleteBotProfile(profile.name);
        if (toast_manager_) {
          toast_manager_->Show(
              absl::StrFormat("Deleted profile: %s", profile.name),
              ToastType::kInfo);
        }
      }
      ImGui::PopStyleColor();

      ImGui::TextDisabled("  %s | %s", profile.provider.c_str(),
                          profile.description.empty()
                              ? "No description"
                              : profile.description.c_str());
      ImGui::Spacing();

      ImGui::PopID();
    }
  }

  ImGui::EndChild();
}

void AgentEditor::DrawChatHistoryViewer() {
  ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f),
                     ICON_MD_HISTORY " Chat History Viewer");
  ImGui::Separator();
  ImGui::Spacing();

  if (ImGui::Button(ICON_MD_REFRESH " Refresh History")) {
    history_needs_refresh_ = true;
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_DELETE " Clear History")) {
    if (chat_widget_) {
      // Clear through the chat widget's service
      if (toast_manager_) {
        toast_manager_->Show("Chat history cleared", ToastType::kInfo);
      }
    }
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Get history from chat widget
  if (chat_widget_ && history_needs_refresh_) {
    // Access the service's history through the chat widget
    // For now, show a placeholder
    history_needs_refresh_ = false;
  }

  ImGui::BeginChild("HistoryList", ImVec2(0, 0), true);

  if (cached_history_.empty()) {
    ImGui::TextDisabled(
        "No chat history. Start a conversation in the chat window.");
  } else {
    for (const auto& msg : cached_history_) {
      bool from_user = (msg.sender == cli::agent::ChatMessage::Sender::kUser);
      ImVec4 color = from_user ? ImVec4(0.6f, 0.8f, 1.0f, 1.0f)
                               : ImVec4(0.4f, 0.8f, 0.4f, 1.0f);

      ImGui::PushStyleColor(ImGuiCol_Text, color);
      ImGui::Text("%s:", from_user ? "User" : "Agent");
      ImGui::PopStyleColor();

      ImGui::SameLine();
      ImGui::TextDisabled("%s", absl::FormatTime("%H:%M:%S", msg.timestamp,
                                                 absl::LocalTimeZone())
                                    .c_str());

      ImGui::TextWrapped("%s", msg.message.c_str());
      ImGui::Spacing();
      ImGui::Separator();
    }
  }

  ImGui::EndChild();
}

void AgentEditor::DrawAdvancedMetricsPanel() {
  ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f),
                     ICON_MD_ANALYTICS " Session Metrics & Analytics");
  ImGui::Separator();
  ImGui::Spacing();

  // Get metrics from chat widget service
  if (chat_widget_) {
    // For now show placeholder metrics structure
    if (ImGui::BeginTable("MetricsTable", 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed,
                              200.0f);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text(ICON_MD_CHAT " Total Messages");
      ImGui::TableSetColumnIndex(1);
      ImGui::TextDisabled("Available in chat session");

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text(ICON_MD_BUILD " Tool Calls");
      ImGui::TableSetColumnIndex(1);
      ImGui::TextDisabled("Available in chat session");

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text(ICON_MD_PREVIEW " Proposals Created");
      ImGui::TableSetColumnIndex(1);
      ImGui::TextDisabled("Available in chat session");

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text(ICON_MD_TIMER " Average Latency");
      ImGui::TableSetColumnIndex(1);
      ImGui::TextDisabled("Available in chat session");

      ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextWrapped(
        "Detailed session metrics are available during active chat sessions. "
        "Open the chat window to see live statistics.");
  } else {
    ImGui::TextDisabled(
        "No metrics available. Initialize the chat system first.");
  }
}

void AgentEditor::DrawCommonTilesEditor() {
  ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f),
                     ICON_MD_GRID_ON " Common Tiles Reference");
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::TextWrapped(
      "Customize the tile reference file that AI uses for tile placement. "
      "Organize tiles by category and provide hex IDs with descriptions.");

  ImGui::Spacing();

  // Load/Save buttons
  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Load", ImVec2(100, 0))) {
    auto content = AssetLoader::LoadTextFile("agent/common_tiles.txt");
    if (content.ok()) {
      common_tiles_editor_->SetText(*content);
      common_tiles_initialized_ = true;
      if (toast_manager_) {
        toast_manager_->Show(ICON_MD_CHECK_CIRCLE " Common tiles loaded",
                             ToastType::kSuccess, 2.0f);
      }
    }
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SAVE " Save", ImVec2(100, 0))) {
    // Save to project or assets directory
    if (toast_manager_) {
      toast_manager_->Show(ICON_MD_INFO
                           " Save to project directory (coming soon)",
                           ToastType::kInfo, 2.0f);
    }
  }

  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_REFRESH)) {
    common_tiles_initialized_ = false;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Reload from disk");
  }

  // Load if not initialized
  if (!common_tiles_initialized_ && common_tiles_editor_) {
    auto content = AssetLoader::LoadTextFile("agent/common_tiles.txt");
    if (content.ok()) {
      common_tiles_editor_->SetText(*content);
    } else {
      // Create default template
      std::string default_tiles =
          "# Common Tile16 Reference\n"
          "# Format: 0xHEX = Description\n\n"
          "[grass_tiles]\n"
          "0x020 = Grass (standard)\n\n"
          "[nature_tiles]\n"
          "0x02E = Tree (oak)\n"
          "0x003 = Bush\n\n"
          "[water_tiles]\n"
          "0x14C = Water (top edge)\n"
          "0x14D = Water (middle)\n";
      common_tiles_editor_->SetText(default_tiles);
    }
    common_tiles_initialized_ = true;
  }

  ImGui::Separator();
  ImGui::Spacing();

  // Editor
  if (common_tiles_editor_) {
    ImVec2 editor_size(ImGui::GetContentRegionAvail().x,
                       ImGui::GetContentRegionAvail().y);
    common_tiles_editor_->Render("##tiles_editor", editor_size, true);
  }
}

void AgentEditor::DrawNewPromptCreator() {
  ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f),
                     ICON_MD_ADD " Create New System Prompt");
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::TextWrapped(
      "Create a custom system prompt from scratch or use a template.");

  ImGui::Spacing();
  ImGui::Separator();

  // Prompt name input
  ImGui::Text("Prompt Name:");
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##new_prompt_name", "e.g., custom_prompt.txt",
                           new_prompt_name_, sizeof(new_prompt_name_));

  ImGui::Spacing();

  // Template selection
  ImGui::Text("Start from template:");

  if (ImGui::Button(ICON_MD_FILE_COPY " v1 (Basic)", ImVec2(-1, 0))) {
    auto content = AssetLoader::LoadTextFile("agent/system_prompt.txt");
    if (content.ok() && prompt_editor_) {
      prompt_editor_->SetText(*content);
      if (toast_manager_) {
        toast_manager_->Show("Template v1 loaded", ToastType::kSuccess, 1.5f);
      }
    }
  }

  if (ImGui::Button(ICON_MD_FILE_COPY " v2 (Enhanced)", ImVec2(-1, 0))) {
    auto content =
        AssetLoader::LoadTextFile("agent/system_prompt_v2.txt");
    if (content.ok() && prompt_editor_) {
      prompt_editor_->SetText(*content);
      if (toast_manager_) {
        toast_manager_->Show("Template v2 loaded", ToastType::kSuccess, 1.5f);
      }
    }
  }

  if (ImGui::Button(ICON_MD_FILE_COPY " v3 (Proactive)", ImVec2(-1, 0))) {
    auto content =
        AssetLoader::LoadTextFile("agent/system_prompt_v3.txt");
    if (content.ok() && prompt_editor_) {
      prompt_editor_->SetText(*content);
      if (toast_manager_) {
        toast_manager_->Show("Template v3 loaded", ToastType::kSuccess, 1.5f);
      }
    }
  }

  if (ImGui::Button(ICON_MD_NOTE_ADD " Blank Template", ImVec2(-1, 0))) {
    if (prompt_editor_) {
      std::string blank_template =
          "# Custom System Prompt\n\n"
          "You are an AI assistant for ROM hacking.\n\n"
          "## Your Role\n"
          "- Help users understand ROM data\n"
          "- Provide accurate information\n"
          "- Use tools when needed\n\n"
          "## Available Tools\n"
          "- resource-list: List resources by type\n"
          "- dungeon-describe-room: Get room details\n"
          "- overworld-find-tile: Find tile locations\n"
          "- ... (see function schemas for complete list)\n\n"
          "## Guidelines\n"
          "1. Always provide text_response after tool calls\n"
          "2. Be helpful and accurate\n"
          "3. Explain your reasoning\n";
      prompt_editor_->SetText(blank_template);
      if (toast_manager_) {
        toast_manager_->Show("Blank template created", ToastType::kSuccess,
                             1.5f);
      }
    }
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Save button
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.133f, 0.545f, 0.133f, 0.8f));
  if (ImGui::Button(ICON_MD_SAVE " Save New Prompt", ImVec2(-1, 40))) {
    if (std::strlen(new_prompt_name_) > 0 && prompt_editor_) {
      // Save to assets/agent/ directory
      std::string filename = new_prompt_name_;
      if (!absl::EndsWith(filename, ".txt")) {
        filename += ".txt";
      }

      // TODO: Actually save the file
      if (toast_manager_) {
        toast_manager_->Show(
            absl::StrFormat(ICON_MD_SAVE " Prompt saved as %s", filename),
            ToastType::kSuccess, 3.0f);
      }

      // Clear name buffer
      std::memset(new_prompt_name_, 0, sizeof(new_prompt_name_));
    } else if (toast_manager_) {
      toast_manager_->Show(ICON_MD_WARNING " Enter a name for the prompt",
                           ToastType::kWarning, 2.0f);
    }
  }
  ImGui::PopStyleColor();

  ImGui::Spacing();
  ImGui::TextWrapped(
      "Note: New prompts are saved to your project. Use 'System Prompt' tab to "
      "edit existing prompts.");
}

// Bot Profile Management Implementation
absl::Status AgentEditor::SaveBotProfile(const BotProfile& profile) {
#if defined(YAZE_WITH_JSON)
  RETURN_IF_ERROR(EnsureProfilesDirectory());

  std::filesystem::path profile_path =
      GetProfilesDirectory() / (profile.name + ".json");
  std::ofstream file(profile_path);
  if (!file.is_open()) {
    return absl::InternalError("Failed to open profile file for writing");
  }

  file << ProfileToJson(profile);
  file.close();

  // Reload profiles list
  Load();

  return absl::OkStatus();
#else
  return absl::UnimplementedError(
      "JSON support required for profile management");
#endif
}

absl::Status AgentEditor::LoadBotProfile(const std::string& name) {
#if defined(YAZE_WITH_JSON)
  std::filesystem::path profile_path =
      GetProfilesDirectory() / (name + ".json");
  if (!std::filesystem::exists(profile_path)) {
    return absl::NotFoundError(absl::StrFormat("Profile '%s' not found", name));
  }

  std::ifstream file(profile_path);
  if (!file.is_open()) {
    return absl::InternalError("Failed to open profile file");
  }

  std::string json_content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

  ASSIGN_OR_RETURN(auto profile, JsonToProfile(json_content));
  current_profile_ = profile;

  // Update legacy config
  current_config_.provider = profile.provider;
  current_config_.model = profile.model;
  current_config_.ollama_host = profile.ollama_host;
  current_config_.gemini_api_key = profile.gemini_api_key;
  current_config_.verbose = profile.verbose;
  current_config_.show_reasoning = profile.show_reasoning;
  current_config_.max_tool_iterations = profile.max_tool_iterations;

  // Apply to chat widget
  ApplyConfig(current_config_);

  return absl::OkStatus();
#else
  return absl::UnimplementedError(
      "JSON support required for profile management");
#endif
}

absl::Status AgentEditor::DeleteBotProfile(const std::string& name) {
  std::filesystem::path profile_path =
      GetProfilesDirectory() / (name + ".json");
  if (!std::filesystem::exists(profile_path)) {
    return absl::NotFoundError(absl::StrFormat("Profile '%s' not found", name));
  }

  std::filesystem::remove(profile_path);

  // Reload profiles list
  Load();

  return absl::OkStatus();
}

std::vector<AgentEditor::BotProfile> AgentEditor::GetAllProfiles() const {
  return loaded_profiles_;
}

void AgentEditor::SetCurrentProfile(const BotProfile& profile) {
  current_profile_ = profile;

  // Update legacy config
  current_config_.provider = profile.provider;
  current_config_.model = profile.model;
  current_config_.ollama_host = profile.ollama_host;
  current_config_.gemini_api_key = profile.gemini_api_key;
  current_config_.verbose = profile.verbose;
  current_config_.show_reasoning = profile.show_reasoning;
  current_config_.max_tool_iterations = profile.max_tool_iterations;
}

absl::Status AgentEditor::ExportProfile(const BotProfile& profile,
                                        const std::filesystem::path& path) {
#if defined(YAZE_WITH_JSON)
  std::ofstream file(path);
  if (!file.is_open()) {
    return absl::InternalError("Failed to open export file");
  }

  file << ProfileToJson(profile);
  file.close();

  return absl::OkStatus();
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

absl::Status AgentEditor::ImportProfile(const std::filesystem::path& path) {
#if defined(YAZE_WITH_JSON)
  if (!std::filesystem::exists(path)) {
    return absl::NotFoundError("Import file not found");
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    return absl::InternalError("Failed to open import file");
  }

  std::string json_content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

  ASSIGN_OR_RETURN(auto profile, JsonToProfile(json_content));

  // Save as new profile
  return SaveBotProfile(profile);
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

std::filesystem::path AgentEditor::GetProfilesDirectory() const {
  auto config_dir = yaze::util::PlatformPaths::GetConfigDirectory();
  if (!config_dir.ok()) {
    // Fallback to a local directory if config can't be determined.
    return std::filesystem::current_path() / ".yaze" / "agent" / "profiles";
  }
  return *config_dir / "agent" / "profiles";
}

absl::Status AgentEditor::EnsureProfilesDirectory() {
  auto dir = GetProfilesDirectory();
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    return absl::InternalError(absl::StrFormat(
        "Failed to create profiles directory: %s", ec.message()));
  }
  return absl::OkStatus();
}

std::string AgentEditor::ProfileToJson(const BotProfile& profile) const {
#if defined(YAZE_WITH_JSON)
  nlohmann::json json;
  json["name"] = profile.name;
  json["description"] = profile.description;
  json["provider"] = profile.provider;
  json["model"] = profile.model;
  json["ollama_host"] = profile.ollama_host;
  json["gemini_api_key"] = profile.gemini_api_key;
  json["system_prompt"] = profile.system_prompt;
  json["verbose"] = profile.verbose;
  json["show_reasoning"] = profile.show_reasoning;
  json["max_tool_iterations"] = profile.max_tool_iterations;
  json["max_retry_attempts"] = profile.max_retry_attempts;
  json["tags"] = profile.tags;
  json["created_at"] = absl::FormatTime(absl::RFC3339_full, profile.created_at,
                                        absl::UTCTimeZone());
  json["modified_at"] = absl::FormatTime(
      absl::RFC3339_full, profile.modified_at, absl::UTCTimeZone());

  return json.dump(2);
#else
  return "{}";
#endif
}

absl::StatusOr<AgentEditor::BotProfile> AgentEditor::JsonToProfile(
    const std::string& json_str) const {
#if defined(YAZE_WITH_JSON)
  try {
    nlohmann::json json = nlohmann::json::parse(json_str);

    BotProfile profile;
    profile.name = json.value("name", "Unnamed Profile");
    profile.description = json.value("description", "");
    profile.provider = json.value("provider", "mock");
    profile.model = json.value("model", "");
    profile.ollama_host = json.value("ollama_host", "http://localhost:11434");
    profile.gemini_api_key = json.value("gemini_api_key", "");
    profile.system_prompt = json.value("system_prompt", "");
    profile.verbose = json.value("verbose", false);
    profile.show_reasoning = json.value("show_reasoning", true);
    profile.max_tool_iterations = json.value("max_tool_iterations", 4);
    profile.max_retry_attempts = json.value("max_retry_attempts", 3);

    if (json.contains("tags") && json["tags"].is_array()) {
      for (const auto& tag : json["tags"]) {
        profile.tags.push_back(tag.get<std::string>());
      }
    }

    if (json.contains("created_at")) {
      absl::Time created;
      if (absl::ParseTime(absl::RFC3339_full,
                          json["created_at"].get<std::string>(), &created,
                          nullptr)) {
        profile.created_at = created;
      }
    }

    if (json.contains("modified_at")) {
      absl::Time modified;
      if (absl::ParseTime(absl::RFC3339_full,
                          json["modified_at"].get<std::string>(), &modified,
                          nullptr)) {
        profile.modified_at = modified;
      }
    }

    return profile;
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to parse profile JSON: %s", e.what()));
  }
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

// Legacy methods
AgentEditor::AgentConfig AgentEditor::GetCurrentConfig() const {
  return current_config_;
}

void AgentEditor::ApplyConfig(const AgentConfig& config) {
  current_config_ = config;

  // Apply to chat widget if available
  if (chat_widget_) {
    AgentChatWidget::AgentConfigState chat_config;
    chat_config.ai_provider = config.provider;
    chat_config.ai_model = config.model;
    chat_config.ollama_host = config.ollama_host;
    chat_config.gemini_api_key = config.gemini_api_key;
    chat_config.verbose = config.verbose;
    chat_config.show_reasoning = config.show_reasoning;
    chat_config.max_tool_iterations = config.max_tool_iterations;
    chat_widget_->UpdateAgentConfig(chat_config);
  }
}

bool AgentEditor::IsChatActive() const {
  return chat_widget_ && chat_widget_->is_active();
}

void AgentEditor::SetChatActive(bool active) {
  if (chat_widget_) {
    chat_widget_->set_active(active);
  }
}

void AgentEditor::ToggleChat() {
  SetChatActive(!IsChatActive());
}

void AgentEditor::OpenChatWindow() {
  if (chat_widget_) {
    chat_widget_->set_active(true);
  }
}

absl::StatusOr<AgentEditor::SessionInfo> AgentEditor::HostSession(
    const std::string& session_name, CollaborationMode mode) {
  current_mode_ = mode;

  if (mode == CollaborationMode::kLocal) {
    ASSIGN_OR_RETURN(auto session,
                     local_coordinator_->HostSession(session_name));

    SessionInfo info;
    info.session_id = session.session_id;
    info.session_name = session.session_name;
    info.participants = session.participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    // Switch chat to shared history
    if (chat_widget_) {
      chat_widget_->SwitchToSharedHistory(info.session_id);
    }

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Hosting local session: %s", session_name),
          ToastType::kSuccess, 3.5f);
    }

    return info;
  }

#ifdef YAZE_WITH_GRPC
  if (mode == CollaborationMode::kNetwork) {
    if (!network_coordinator_) {
      return absl::FailedPreconditionError(
          "Network coordinator not initialized. Connect to a server first.");
    }

    const char* username = std::getenv("USER");
    if (!username) {
      username = std::getenv("USERNAME");
    }
    if (!username) {
      username = "unknown";
    }

    ASSIGN_OR_RETURN(auto session,
                     network_coordinator_->HostSession(session_name, username));

    SessionInfo info;
    info.session_id = session.session_id;
    info.session_name = session.session_name;
    info.participants = session.participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Hosting network session: %s", session_name),
          ToastType::kSuccess, 3.5f);
    }

    return info;
  }
#endif

  return absl::InvalidArgumentError("Unsupported collaboration mode");
}

absl::StatusOr<AgentEditor::SessionInfo> AgentEditor::JoinSession(
    const std::string& session_code, CollaborationMode mode) {
  current_mode_ = mode;

  if (mode == CollaborationMode::kLocal) {
    ASSIGN_OR_RETURN(auto session,
                     local_coordinator_->JoinSession(session_code));

    SessionInfo info;
    info.session_id = session.session_id;
    info.session_name = session.session_name;
    info.participants = session.participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    // Switch chat to shared history
    if (chat_widget_) {
      chat_widget_->SwitchToSharedHistory(info.session_id);
    }

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Joined local session: %s", session_code),
          ToastType::kSuccess, 3.5f);
    }

    return info;
  }

#ifdef YAZE_WITH_GRPC
  if (mode == CollaborationMode::kNetwork) {
    if (!network_coordinator_) {
      return absl::FailedPreconditionError(
          "Network coordinator not initialized. Connect to a server first.");
    }

    const char* username = std::getenv("USER");
    if (!username) {
      username = std::getenv("USERNAME");
    }
    if (!username) {
      username = "unknown";
    }

    ASSIGN_OR_RETURN(auto session,
                     network_coordinator_->JoinSession(session_code, username));

    SessionInfo info;
    info.session_id = session.session_id;
    info.session_name = session.session_name;
    info.participants = session.participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Joined network session: %s", session_code),
          ToastType::kSuccess, 3.5f);
    }

    return info;
  }
#endif

  return absl::InvalidArgumentError("Unsupported collaboration mode");
}

absl::Status AgentEditor::LeaveSession() {
  if (!in_session_) {
    return absl::FailedPreconditionError("Not in a session");
  }

  if (current_mode_ == CollaborationMode::kLocal) {
    RETURN_IF_ERROR(local_coordinator_->LeaveSession());
  }
#ifdef YAZE_WITH_GRPC
  else if (current_mode_ == CollaborationMode::kNetwork) {
    if (network_coordinator_) {
      RETURN_IF_ERROR(network_coordinator_->LeaveSession());
    }
  }
#endif

  // Switch chat back to local history
  if (chat_widget_) {
    chat_widget_->SwitchToLocalHistory();
  }

  in_session_ = false;
  current_session_id_.clear();
  current_session_name_.clear();
  current_participants_.clear();

  if (toast_manager_) {
    toast_manager_->Show("Left collaboration session", ToastType::kInfo, 3.0f);
  }

  return absl::OkStatus();
}

absl::StatusOr<AgentEditor::SessionInfo> AgentEditor::RefreshSession() {
  if (!in_session_) {
    return absl::FailedPreconditionError("Not in a session");
  }

  if (current_mode_ == CollaborationMode::kLocal) {
    ASSIGN_OR_RETURN(auto session, local_coordinator_->RefreshSession());

    SessionInfo info;
    info.session_id = session.session_id;
    info.session_name = session.session_name;
    info.participants = session.participants;

    current_participants_ = info.participants;

    return info;
  }

  // Network mode doesn't need explicit refresh - it's real-time
  SessionInfo info;
  info.session_id = current_session_id_;
  info.session_name = current_session_name_;
  info.participants = current_participants_;
  return info;
}

absl::Status AgentEditor::CaptureSnapshot(
    [[maybe_unused]] std::filesystem::path* output_path,
    [[maybe_unused]] const CaptureConfig& config) {
  return absl::UnimplementedError(
      "CaptureSnapshot should be called through the chat widget UI");
}

absl::Status AgentEditor::SendToGemini(
    [[maybe_unused]] const std::filesystem::path& image_path,
    [[maybe_unused]] const std::string& prompt) {
  return absl::UnimplementedError(
      "SendToGemini should be called through the chat widget UI");
}

#ifdef YAZE_WITH_GRPC
absl::Status AgentEditor::ConnectToServer(const std::string& server_url) {
  try {
    network_coordinator_ =
        std::make_unique<NetworkCollaborationCoordinator>(server_url);

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Connected to server: %s", server_url),
          ToastType::kSuccess, 3.0f);
    }

    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to connect to server: %s", e.what()));
  }
}

void AgentEditor::DisconnectFromServer() {
  if (in_session_ && current_mode_ == CollaborationMode::kNetwork) {
    LeaveSession();
  }
  network_coordinator_.reset();

  if (toast_manager_) {
    toast_manager_->Show("Disconnected from server", ToastType::kInfo, 2.5f);
  }
}

bool AgentEditor::IsConnectedToServer() const {
  return network_coordinator_ && network_coordinator_->IsConnected();
}
#endif

bool AgentEditor::IsInSession() const {
  return in_session_;
}

AgentEditor::CollaborationMode AgentEditor::GetCurrentMode() const {
  return current_mode_;
}

std::optional<AgentEditor::SessionInfo> AgentEditor::GetCurrentSession() const {
  if (!in_session_) {
    return std::nullopt;
  }

  SessionInfo info;
  info.session_id = current_session_id_;
  info.session_name = current_session_name_;
  info.participants = current_participants_;
  return info;
}

void AgentEditor::SetupChatWidgetCallbacks() {
  if (!chat_widget_) {
    return;
  }

  AgentChatWidget::CollaborationCallbacks collab_callbacks;

  collab_callbacks.host_session = [this](const std::string& session_name)
      -> absl::StatusOr<
          AgentChatWidget::CollaborationCallbacks::SessionContext> {
    ASSIGN_OR_RETURN(auto session,
                     this->HostSession(session_name, current_mode_));

    AgentChatWidget::CollaborationCallbacks::SessionContext context;
    context.session_id = session.session_id;
    context.session_name = session.session_name;
    context.participants = session.participants;
    return context;
  };

  collab_callbacks.join_session = [this](const std::string& session_code)
      -> absl::StatusOr<
          AgentChatWidget::CollaborationCallbacks::SessionContext> {
    ASSIGN_OR_RETURN(auto session,
                     this->JoinSession(session_code, current_mode_));

    AgentChatWidget::CollaborationCallbacks::SessionContext context;
    context.session_id = session.session_id;
    context.session_name = session.session_name;
    context.participants = session.participants;
    return context;
  };

  collab_callbacks.leave_session = [this]() {
    return this->LeaveSession();
  };

  collab_callbacks.refresh_session =
      [this]() -> absl::StatusOr<
                   AgentChatWidget::CollaborationCallbacks::SessionContext> {
    ASSIGN_OR_RETURN(auto session, this->RefreshSession());

    AgentChatWidget::CollaborationCallbacks::SessionContext context;
    context.session_id = session.session_id;
    context.session_name = session.session_name;
    context.participants = session.participants;
    return context;
  };

  chat_widget_->SetCollaborationCallbacks(collab_callbacks);
}

void AgentEditor::SetupMultimodalCallbacks() {
  // Multimodal callbacks are set up by the EditorManager since it has
  // access to the screenshot utilities. We just initialize the structure here.
}

}  // namespace editor
}  // namespace yaze