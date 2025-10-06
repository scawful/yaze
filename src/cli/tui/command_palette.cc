#include "cli/tui/command_palette.h"

#include <algorithm>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "cli/tui/tui.h"
#include "cli/handlers/agent/hex_commands.h"
#include "cli/handlers/agent/palette_commands.h"
#include "absl/strings/str_split.h"

namespace yaze {
namespace cli {

using namespace ftxui;

Component CommandPaletteComponent::Render() {
  static std::string query;
  static int selected = 0;
  static std::string status_msg;
  
  struct Cmd {
    std::string name;
    std::string cat;
    std::string desc;
    std::string usage;
    std::function<absl::Status()> exec;
  };
  
  static std::vector<Cmd> cmds = {
    {"hex-read", "🔢 Hex", "Read ROM bytes", 
     "--address=0x1C800 --length=16 --format=both",
     []() { return agent::HandleHexRead({"--address=0x1C800", "--length=16"}, &app_context.rom); }},
    
    {"hex-write", "🔢 Hex", "Write ROM bytes",
     "--address=0x1C800 --data=\"FF 00\"",
     []() { return agent::HandleHexWrite({"--address=0x1C800", "--data=FF 00"}, &app_context.rom); }},
    
    {"hex-search", "🔢 Hex", "Search byte pattern",
     "--pattern=\"FF 00 ?? 12\"",
     []() { return agent::HandleHexSearch({"--pattern=FF 00"}, &app_context.rom); }},
    
    {"palette-get", "🎨 Palette", "Get palette colors",
     "--group=0 --palette=0 --format=hex",
     []() { return agent::HandlePaletteGetColors({"--group=0", "--palette=0", "--format=hex"}, &app_context.rom); }},
    
    {"palette-set", "🎨 Palette", "Set palette color",
     "--group=0 --palette=0 --index=5 --color=FF0000",
     []() { return agent::HandlePaletteSetColor({"--group=0", "--palette=0", "--index=5", "--color=FF0000"}, &app_context.rom); }},
    
    {"palette-analyze", "🎨 Palette", "Analyze palette",
     "--type=palette --id=0/0",
     []() { return agent::HandlePaletteAnalyze({"--type=palette", "--id=0/0"}, &app_context.rom); }},
  };
  
  // Fuzzy filter
  std::vector<int> filtered_idx;
  std::string q_lower = query;
  std::transform(q_lower.begin(), q_lower.end(), q_lower.begin(), ::tolower);
  
  for (size_t i = 0; i < cmds.size(); ++i) {
    if (query.empty()) {
      filtered_idx.push_back(i);
    } else {
      std::string n = cmds[i].name;
      std::transform(n.begin(), n.end(), n.begin(), ::tolower);
      if (n.find(q_lower) != std::string::npos) {
        filtered_idx.push_back(i);
      }
    }
  }
  
  auto search_input = Input(&query, "Search...");
  
  std::vector<std::string> menu_items;
  for (int idx : filtered_idx) {
    menu_items.push_back(cmds[idx].cat + " " + cmds[idx].name);
  }
  
  auto menu = Menu(&menu_items, &selected);
  
  auto exec_btn = Button("Execute", [&] {
    if (selected < static_cast<int>(filtered_idx.size())) {
      int cmd_idx = filtered_idx[selected];
      auto status = cmds[cmd_idx].exec();
      status_msg = status.ok() ? "✓ Success" : "✗ " + std::string(status.message());
    }
  });
  
  auto back_btn = Button("Back", [&] {
    app_context.current_layout = LayoutID::kMainMenu;
    ScreenInteractive::Active()->ExitLoopClosure()();
  });
  
  auto container = Container::Vertical({search_input, menu, exec_btn, back_btn});
  
  return CatchEvent(Renderer(container, [&] {
    Elements items;
    for (size_t i = 0; i < filtered_idx.size(); ++i) {
      int idx = filtered_idx[i];
      auto item = text(cmds[idx].cat + " " + cmds[idx].name);
      if (static_cast<int>(i) == selected) {
        item = item | bold | inverted | color(Color::Cyan);
      }
      items.push_back(item);
    }
    
    // Show selected command details
    Element details = text("");
    if (selected < static_cast<int>(filtered_idx.size())) {
      int idx = filtered_idx[selected];
      details = vbox({
        text("Description: " + cmds[idx].desc) | color(Color::GreenLight),
        text("Usage: " + cmds[idx].usage) | color(Color::Yellow) | dim,
      });
    }
    
    return vbox({
      text("⚡ Command Palette") | bold | center | color(Color::Cyan),
      text(app_context.rom.is_loaded() ? "ROM: " + app_context.rom.title() : "No ROM") | center | dim,
      separator(),
      hbox({text("🔍 "), search_input->Render() | flex}),
      separator(),
      vbox(items) | frame | flex | vscroll_indicator,
      separator(),
      details,
      separator(),
      hbox({exec_btn->Render(), text("  "), back_btn->Render()}) | center,
      separator(),
      text(status_msg) | center | (status_msg.find("✓") == 0 ? color(Color::Green) : color(Color::Red)),
      text("Enter=Execute | ↑↓=Navigate | Esc=Back") | center | dim,
    }) | border | size(WIDTH, EQUAL, 80) | size(HEIGHT, EQUAL, 30);
  }), [&](Event e) {
    if (e == Event::Return && selected < static_cast<int>(filtered_idx.size())) {
      int idx = filtered_idx[selected];
      auto status = cmds[idx].exec();
      status_msg = status.ok() ? "✓ Executed" : "✗ " + std::string(status.message());
      return true;
    }
    return false;
  });
}

}  // namespace cli
}  // namespace yaze