#include "app/gui/automation/widget_id_registry.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "app/gui/automation/widget_state_capture.h"
#include "imgui/imgui.h"

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace gui {
namespace {

// Non-ASCII strings are spelled as UTF-8 byte escapes so the source file stays
// ASCII. A literal break follows an escape whose next character is a hex digit.
constexpr char kQuotePath[] = "Editor/Tab/button:Say \"hi\"";
constexpr char kBackslashPath[] = "Editor/Tab/input:C:\\roms\\oracle.sfc";
constexpr char kUnicodeWindow[] =
    "\xC3\x89"
    "diteur";
constexpr char kUnicodeLabel[] = "Zelda \xE3\x82\xBC\xE3\x83\xAB\xE3\x83\x80";
constexpr char kUnicodePath[] =
    "\xC3\x89"
    "diteur/Onglet/button:"
    "Zelda \xE3\x82\xBC\xE3\x83\xAB\xE3\x83\x80";
constexpr char kDescription[] = "line one\nline two\ttab";

class WidgetIdRegistryTest : public ::testing::Test {
 protected:
  void SetUp() override { WidgetIdRegistry::Instance().Clear(); }
  void TearDown() override { WidgetIdRegistry::Instance().Clear(); }

  // Metadata is spelled out so the result does not depend on whether an
  // ImGui context happens to be current.
  static void Register(const std::string& path, const std::string& label,
                       const std::string& window,
                       const std::string& description = "",
                       bool bounds_valid = true) {
    WidgetIdRegistry::WidgetMetadata metadata;
    metadata.label = label;
    metadata.window_name = window;
    metadata.visible = true;
    metadata.enabled = true;
    WidgetIdRegistry::WidgetBounds bounds;
    bounds.min_x = 4.0f;
    bounds.min_y = 8.0f;
    bounds.max_x = 100.0f;
    bounds.max_y = 50.0f;
    bounds.valid = bounds_valid;
    metadata.bounds = bounds;
    WidgetIdRegistry::Instance().RegisterWidget(path, "button", 0x1234u,
                                                description, metadata);
  }

  static void RegisterPath(const std::string& path) {
    Register(path, path, "Window");
  }
};

#if defined(YAZE_WITH_JSON)

TEST_F(WidgetIdRegistryTest, JsonCatalogParsesAndRoundTripsSpecialCharacters) {
  Register(kQuotePath, "Say \"hi\"", "Editor", kDescription);
  Register(kBackslashPath, "C:\\roms\\oracle.sfc", "Editor", "",
           /*bounds_valid=*/false);
  Register(kUnicodePath, kUnicodeLabel, kUnicodeWindow);

  const std::string text = WidgetIdRegistry::Instance().ExportCatalog("json");
  const nlohmann::json parsed =
      nlohmann::json::parse(text, nullptr, /*allow_exceptions=*/false);
  ASSERT_FALSE(parsed.is_discarded()) << text;
  ASSERT_TRUE(parsed.contains("widgets")) << text;
  const nlohmann::json& widgets = parsed["widgets"];
  ASSERT_TRUE(widgets.is_array());
  ASSERT_EQ(widgets.size(), 3u);

  // Entries are sorted by path bytes; the UTF-8 lead byte 0xC3 sorts last.
  EXPECT_EQ(widgets[0]["path"].get<std::string>(), kQuotePath);
  EXPECT_EQ(widgets[0]["label"].get<std::string>(), "Say \"hi\"");
  EXPECT_EQ(widgets[0]["description"].get<std::string>(), kDescription);
  EXPECT_EQ(widgets[0]["type"].get<std::string>(), "button");
  EXPECT_EQ(widgets[0]["imgui_id"].get<unsigned int>(), 0x1234u);
  EXPECT_TRUE(widgets[0]["visible"].get<bool>());
  ASSERT_TRUE(widgets[0]["bounds"].is_object());
  EXPECT_DOUBLE_EQ(widgets[0]["bounds"]["min"][0].get<double>(), 4.0);
  EXPECT_DOUBLE_EQ(widgets[0]["bounds"]["max"][1].get<double>(), 50.0);

  EXPECT_EQ(widgets[1]["path"].get<std::string>(), kBackslashPath);
  EXPECT_EQ(widgets[1]["label"].get<std::string>(), "C:\\roms\\oracle.sfc");
  EXPECT_TRUE(widgets[1]["bounds"].is_null());
  EXPECT_FALSE(widgets[1].contains("description"));

  EXPECT_EQ(widgets[2]["path"].get<std::string>(), kUnicodePath);
  EXPECT_EQ(widgets[2]["label"].get<std::string>(), kUnicodeLabel);
  EXPECT_EQ(widgets[2]["window"].get<std::string>(), kUnicodeWindow);
}

TEST_F(WidgetIdRegistryTest, JsonCatalogWritesNonFiniteBoundsAsNull) {
  WidgetIdRegistry::WidgetMetadata metadata;
  metadata.label = "Odd";
  WidgetIdRegistry::WidgetBounds bounds;
  bounds.min_x = std::numeric_limits<float>::quiet_NaN();
  bounds.max_x = std::numeric_limits<float>::infinity();
  bounds.valid = true;
  metadata.bounds = bounds;
  WidgetIdRegistry::Instance().RegisterWidget("Editor/button:Odd", "button", 1u,
                                              "", metadata);

  const std::string text = WidgetIdRegistry::Instance().ExportCatalog("json");
  const nlohmann::json parsed =
      nlohmann::json::parse(text, nullptr, /*allow_exceptions=*/false);
  ASSERT_FALSE(parsed.is_discarded()) << text;
  ASSERT_TRUE(parsed.contains("widgets")) << text;
  ASSERT_EQ(parsed["widgets"].size(), 1u) << text;
  const nlohmann::json& widget_bounds = parsed["widgets"][0]["bounds"];
  ASSERT_TRUE(widget_bounds.is_object()) << text;
  EXPECT_TRUE(widget_bounds["min"][0].is_null());
  EXPECT_TRUE(widget_bounds["max"][0].is_null());
}

TEST_F(WidgetIdRegistryTest, EmptyJsonCatalogIsAnEmptyArray) {
  const std::string text = WidgetIdRegistry::Instance().ExportCatalog("json");
  const nlohmann::json parsed =
      nlohmann::json::parse(text, nullptr, /*allow_exceptions=*/false);
  ASSERT_FALSE(parsed.is_discarded()) << text;
  ASSERT_TRUE(parsed.contains("widgets")) << text;
  ASSERT_TRUE(parsed["widgets"].is_array());
  EXPECT_TRUE(parsed["widgets"].empty());
}

#else  // !defined(YAZE_WITH_JSON)

TEST_F(WidgetIdRegistryTest, JsonCatalogEscapesSpecialCharacters) {
  Register(kQuotePath, "Say \"hi\"", "Editor", kDescription);
  Register(kBackslashPath, "C:\\roms\\oracle.sfc", "Editor");

  const std::string text = WidgetIdRegistry::Instance().ExportCatalog("json");
  EXPECT_NE(text.find("\"Say \\\"hi\\\"\""), std::string::npos) << text;
  EXPECT_NE(text.find("\"C:\\\\roms\\\\oracle.sfc\""), std::string::npos)
      << text;
  EXPECT_NE(text.find("\"line one\\nline two\\ttab\""), std::string::npos)
      << text;
}

#endif  // defined(YAZE_WITH_JSON)

TEST_F(WidgetIdRegistryTest, FindWidgetsMatchesGlobPatterns) {
  const std::string unicode_short =
      "a\xC3\xA9"
      "c";
  std::vector<std::string> all = {"Overworld/Toolset/button:DrawTile",
                                  "Overworld/Canvas/canvas:Map",
                                  "Dungeon/Toolset/button:DrawTile",
                                  "abc",
                                  "abcd",
                                  "axc",
                                  "ac",
                                  "abbc",
                                  unicode_short};
  for (const std::string& path : all) {
    RegisterPath(path);
  }
  std::sort(all.begin(), all.end());
  const WidgetIdRegistry& registry = WidgetIdRegistry::Instance();
  using Paths = std::vector<std::string>;

  EXPECT_EQ(registry.FindWidgets("*"), all);
  EXPECT_EQ(registry.FindWidgets("Overworld/*"),
            (Paths{"Overworld/Canvas/canvas:Map",
                   "Overworld/Toolset/button:DrawTile"}));
  EXPECT_EQ(registry.FindWidgets("*button:DrawTile"),
            (Paths{"Dungeon/Toolset/button:DrawTile",
                   "Overworld/Toolset/button:DrawTile"}));
  EXPECT_EQ(registry.FindWidgets("*Toolset*"),
            (Paths{"Dungeon/Toolset/button:DrawTile",
                   "Overworld/Toolset/button:DrawTile"}));
  EXPECT_EQ(registry.FindWidgets("Overworld*Map"),
            (Paths{"Overworld/Canvas/canvas:Map"}));

  // '?' is exactly one character, and a two-byte UTF-8 character counts as
  // one.
  EXPECT_EQ(registry.FindWidgets("a?c"), (Paths{"abc", "axc", unicode_short}));
  EXPECT_EQ(registry.FindWidgets("a??c"), (Paths{"abbc"}));
  EXPECT_EQ(registry.FindWidgets("a*c"),
            (Paths{"abbc", "abc", "ac", "axc", unicode_short}));

  // Without wildcards the whole path must match, so "abc" skips "abcd".
  EXPECT_EQ(registry.FindWidgets("abc"), (Paths{"abc"}));
  EXPECT_TRUE(registry.FindWidgets("").empty());

  // The pattern is anchored at both ends: "Toolset*" used to be a substring
  // search and now matches only paths that start with "Toolset".
  EXPECT_TRUE(registry.FindWidgets("Toolset*").empty());
}

class WidgetStateCaptureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(640.0f, 480.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }

  void TearDown() override {
    ImGui::DestroyContext(context_);
    context_ = nullptr;
  }

  // Submits windows Alpha and Beta. `inside_beta` runs while Beta is ImGui's
  // CurrentWindow, which is the window the old capture reported as focused.
  void RunFrame(const std::function<void()>& inside_beta = {}) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(200.0f, 100.0f), ImGuiCond_Always);
    ImGui::Begin("Alpha", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::End();
    ImGui::SetNextWindowPos(ImVec2(300.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(200.0f, 100.0f), ImGuiCond_Always);
    ImGui::Begin("Beta", nullptr, ImGuiWindowFlags_NoSavedSettings);
    if (inside_beta) {
      inside_beta();
    }
    ImGui::End();
    ImGui::EndFrame();
  }

  static std::string FocusedWindowField(const std::string& capture) {
#if defined(YAZE_WITH_JSON)
    const nlohmann::json parsed =
        nlohmann::json::parse(capture, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_discarded() || !parsed.contains("focused_window")) {
      return "<missing>";
    }
    return parsed["focused_window"].get<std::string>();
#else
    const std::string key = "\"focused_window\": \"";
    size_t start = capture.find(key);
    if (start == std::string::npos) {
      return "<missing>";
    }
    start += key.size();
    return capture.substr(start, capture.find('"', start) - start);
#endif
  }

  ImGuiContext* context_ = nullptr;
};

TEST_F(WidgetStateCaptureTest, FocusedWindowFollowsNavWindow) {
  RunFrame();
  RunFrame();

  std::string alpha_capture;
  RunFrame([&] {
    ImGui::SetWindowFocus("Alpha");
    alpha_capture = core::CaptureWidgetState();
  });
  EXPECT_EQ(FocusedWindowField(alpha_capture), "Alpha") << alpha_capture;

  // Between frames there is no CurrentWindow; focus is still Alpha.
  const std::string between_frames = core::CaptureWidgetState();
  EXPECT_EQ(FocusedWindowField(between_frames), "Alpha") << between_frames;

  std::string beta_capture;
  RunFrame([&] {
    ImGui::SetWindowFocus("Beta");
    beta_capture = core::CaptureWidgetState();
  });
  EXPECT_EQ(FocusedWindowField(beta_capture), "Beta") << beta_capture;

  std::string none_capture;
  RunFrame([&] {
    ImGui::SetWindowFocus(nullptr);
    none_capture = core::CaptureWidgetState();
  });
  EXPECT_EQ(FocusedWindowField(none_capture), "") << none_capture;
}

}  // namespace
}  // namespace gui
}  // namespace yaze
