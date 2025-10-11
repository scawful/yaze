#include "cli/tui/command_palette.h"

#include <algorithm>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "cli/tui/tui.h"
#include "cli/handlers/agent/hex_commands.h"
#include "cli/handlers/agent/palette_commands.h"

namespace yaze {
namespace cli {

using namespace ftxui;

namespace {
// A simple fuzzy search implementation
int fuzzy_match(const std::string& query, const std::string& target) {
    if (query.empty()) return 1;
    if (target.empty()) return 0;

    int score = 0;
    int query_idx = 0;
    int target_idx = 0;
    int consecutive_matches = 0;

    while (query_idx < query.length() && target_idx < target.length()) {
        if (std::tolower(query[query_idx]) == std::tolower(target[target_idx])) {
            score += 1 + consecutive_matches;
            consecutive_matches++;
            query_idx++;
        } else {
            consecutive_matches = 0;
        }
        target_idx++;
    }

    return (query_idx == query.length()) ? score : 0;
}
}

Component CommandPaletteComponent::Render() {
  struct PaletteState {
    std::string query;
    int selected = 0;
    std::string status_msg;
  };
  
  struct Cmd {
    std::string name;
    std::string cat;
    std::string desc;
    std::string usage;
    std::function<absl::Status()> exec;
    int score = 0;
  };
  
  auto state = std::make_shared<PaletteState>();
  
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
  
  auto search_input = Input(&state->query, "Search commands...");
  
  auto menu = Renderer([state] {
    std::vector<int> filtered_idx;
    if (state->query.empty()) {
        for (size_t i = 0; i < cmds.size(); ++i) {
            filtered_idx.push_back(i);
        }
    } else {
        for (size_t i = 0; i < cmds.size(); ++i) {
            cmds[i].score = fuzzy_match(state->query, cmds[i].name);
            if (cmds[i].score > 0) {
                filtered_idx.push_back(i);
            }
        }
        std::sort(filtered_idx.begin(), filtered_idx.end(), [](int a, int b) {
            return cmds[a].score > cmds[b].score;
        });
    }
    Elements items;
    for (size_t i = 0; i < filtered_idx.size(); ++i) {
      int idx = filtered_idx[i];
      auto item = hbox({
          text(cmds[idx].cat) | color(Color::GrayLight),
          text(" "),
          text(cmds[idx].name) | bold,
      });
      if (static_cast<int>(i) == state->selected) {
        item = item | inverted | focus;
      }
      items.push_back(item);
    }
    return vbox(items) | vscroll_indicator | frame;
  });
  
  auto execute_command = [state] {
    std::vector<int> filtered_idx;
    if (state->query.empty()) {
        for (size_t i = 0; i < cmds.size(); ++i) {
            filtered_idx.push_back(i);
        }
    } else {
        for (size_t i = 0; i < cmds.size(); ++i) {
            cmds[i].score = fuzzy_match(state->query, cmds[i].name);
            if (cmds[i].score > 0) {
                filtered_idx.push_back(i);
            }
        }
        std::sort(filtered_idx.begin(), filtered_idx.end(), [](int a, int b) {
            return cmds[a].score > cmds[b].score;
        });
    }
    
    if (state->selected < static_cast<int>(filtered_idx.size())) {
      int cmd_idx = filtered_idx[state->selected];
      auto status = cmds[cmd_idx].exec();
      state->status_msg = status.ok() ? "✓ Success: Command executed." : "✗ Error: " + std::string(status.message());
    }
  };

  auto back_btn = Button("Back", [] {
    app_context.current_layout = LayoutID::kMainMenu;
    ScreenInteractive::Active()->ExitLoopClosure()();
  });
  
  auto container = Container::Vertical({search_input, menu, back_btn});
  
  return Renderer(container, [container, search_input, menu, back_btn, state] {
    std::vector<int> filtered_idx;
    if (state->query.empty()) {
        for (size_t i = 0; i < cmds.size(); ++i) {
            filtered_idx.push_back(i);
        }
    } else {
        for (size_t i = 0; i < cmds.size(); ++i) {
            cmds[i].score = fuzzy_match(state->query, cmds[i].name);
            if (cmds[i].score > 0) {
                filtered_idx.push_back(i);
            }
        }
        std::sort(filtered_idx.begin(), filtered_idx.end(), [](int a, int b) {
            return cmds[a].score > cmds[b].score;
        });
    }
    
    Element details = text("Select a command to see details.") | dim;
    if (state->selected < static_cast<int>(filtered_idx.size())) {
      int idx = filtered_idx[state->selected];
      details = vbox({
        text(cmds[idx].desc) | bold,
        separator(),
        text("Usage: " + cmds[idx].name + " " + cmds[idx].usage) | color(Color::Cyan),
      });
    }
    
    return vbox({
      text("⚡ Command Palette") | bold | center | color(Color::Cyan),
      text(app_context.rom.is_loaded() ? "ROM: " + app_context.rom.title() : "No ROM loaded") | center | dim,
      separator(),
      hbox({text("🔍 "), search_input->Render() | flex}),
      separator(),
      hbox({
        menu->Render() | flex,
        separator(),
        details | flex,
      }),
      separator(),
      hbox({ back_btn->Render() }) | center,
      separator(),
      text(state->status_msg) | center | (state->status_msg.find("✓") != 0 ? color(Color::Green) : color(Color::Red)),
      text("↑↓: Navigate | Enter: Execute | Esc: Back") | center | dim,
    }) | border | flex;
  }) | CatchEvent([state, execute_command](const Event& e) {
    if (e == Event::Return) {
      execute_command();
      return true;
    }
    if (e == Event::ArrowUp) {
        if (state->selected > 0) state->selected--;
        return true;
    }
    if (e == Event::ArrowDown) {
        // Calculate filtered_idx size
        std::vector<int> filtered_idx;
        if (state->query.empty()) {
            for (size_t i = 0; i < cmds.size(); ++i) {
                filtered_idx.push_back(i);
            }
        } else {
            for (size_t i = 0; i < cmds.size(); ++i) {
                cmds[i].score = fuzzy_match(state->query, cmds[i].name);
                if (cmds[i].score > 0) {
                    filtered_idx.push_back(i);
                }
            }
        }
        if (state->selected < static_cast<int>(filtered_idx.size()) - 1) state->selected++;
        return true;
    }
    return false;
  });
}

}  // namespace cli
}  // namespace yaze