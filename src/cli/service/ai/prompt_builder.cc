#include "cli/service/ai/prompt_builder.h"

#include <fstream>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"

namespace yaze {
namespace cli {

PromptBuilder::PromptBuilder() {
  LoadDefaultExamples();
}

void PromptBuilder::LoadDefaultExamples() {
  // ==========================================================================
  // OVERWORLD TILE16 EDITING - Primary Focus
  // ==========================================================================
  
  // Single tile placement
  examples_.push_back({
      "Place a tree at position 10, 20 on the Light World map",
      {
          "overworld set-tile --map 0 --x 10 --y 20 --tile 0x02E"
      },
      "Single tile16 placement. Tree tile ID is 0x02E in vanilla ALTTP"
  });
  
  // Area/region editing
  examples_.push_back({
      "Create a 3x3 water pond at coordinates 15, 10",
      {
          "overworld set-tile --map 0 --x 15 --y 10 --tile 0x14C",
          "overworld set-tile --map 0 --x 16 --y 10 --tile 0x14D",
          "overworld set-tile --map 0 --x 17 --y 10 --tile 0x14C",
          "overworld set-tile --map 0 --x 15 --y 11 --tile 0x14D",
          "overworld set-tile --map 0 --x 16 --y 11 --tile 0x14D",
          "overworld set-tile --map 0 --x 17 --y 11 --tile 0x14D",
          "overworld set-tile --map 0 --x 15 --y 12 --tile 0x14E",
          "overworld set-tile --map 0 --x 16 --y 12 --tile 0x14E",
          "overworld set-tile --map 0 --x 17 --y 12 --tile 0x14E"
      },
      "Water areas use different edge tiles: 0x14C (top), 0x14D (middle), 0x14E (bottom)"
  });
  
  // Path/line creation
  examples_.push_back({
      "Add a dirt path from position 5,5 to 5,15",
      {
          "overworld set-tile --map 0 --x 5 --y 5 --tile 0x022",
          "overworld set-tile --map 0 --x 5 --y 6 --tile 0x022",
          "overworld set-tile --map 0 --x 5 --y 7 --tile 0x022",
          "overworld set-tile --map 0 --x 5 --y 8 --tile 0x022",
          "overworld set-tile --map 0 --x 5 --y 9 --tile 0x022",
          "overworld set-tile --map 0 --x 5 --y 10 --tile 0x022",
          "overworld set-tile --map 0 --x 5 --y 11 --tile 0x022",
          "overworld set-tile --map 0 --x 5 --y 12 --tile 0x022",
          "overworld set-tile --map 0 --x 5 --y 13 --tile 0x022",
          "overworld set-tile --map 0 --x 5 --y 14 --tile 0x022",
          "overworld set-tile --map 0 --x 5 --y 15 --tile 0x022"
      },
      "Linear paths are created by placing tiles sequentially. Dirt tile is 0x022"
  });
  
  // Forest/tree grouping
  examples_.push_back({
      "Plant a row of trees horizontally at y=8 from x=20 to x=25",
      {
          "overworld set-tile --map 0 --x 20 --y 8 --tile 0x02E",
          "overworld set-tile --map 0 --x 21 --y 8 --tile 0x02E",
          "overworld set-tile --map 0 --x 22 --y 8 --tile 0x02E",
          "overworld set-tile --map 0 --x 23 --y 8 --tile 0x02E",
          "overworld set-tile --map 0 --x 24 --y 8 --tile 0x02E",
          "overworld set-tile --map 0 --x 25 --y 8 --tile 0x02E"
      },
      "Tree rows create natural barriers and visual boundaries"
  });
  
  // ==========================================================================
  // DUNGEON EDITING - Label-Aware Operations
  // ==========================================================================
  
  // Sprite placement (label-aware)
  examples_.push_back({
      "Add 3 soldiers to the Eastern Palace entrance room",
      {
          "dungeon add-sprite --dungeon 0x02 --room 0x00 --sprite 0x41 --x 5 --y 3",
          "dungeon add-sprite --dungeon 0x02 --room 0x00 --sprite 0x41 --x 10 --y 3",
          "dungeon add-sprite --dungeon 0x02 --room 0x00 --sprite 0x41 --x 7 --y 8"
      },
      "Dungeon ID 0x02 is Eastern Palace. Sprite 0x41 is soldier. Spread placement for balance"
  });
  
  // Object placement
  examples_.push_back({
      "Place a chest in the Hyrule Castle treasure room",
      {
          "dungeon add-chest --dungeon 0x00 --room 0x60 --x 7 --y 5 --item 0x12 --big false"
      },
      "Dungeon 0x00 is Hyrule Castle. Item 0x12 is a small key. Position centered in room"
  });
  
  // ==========================================================================
  // COMMON TILE16 REFERENCE (for AI knowledge)
  // ==========================================================================
  // Grass: 0x020
  // Dirt: 0x022
  // Tree: 0x02E
  // Water (top): 0x14C
  // Water (middle): 0x14D
  // Water (bottom): 0x14E
  // Bush: 0x003
  // Rock: 0x004
  // Flower: 0x021
  // Sand: 0x023
  // Deep Water: 0x14F
  // Shallow Water: 0x150
  
  // Validation example (still useful)
  examples_.push_back({
      "Check if my overworld changes are valid",
      {
          "rom validate"
      },
      "Validation ensures ROM integrity after tile modifications"
  });
}

absl::Status PromptBuilder::LoadResourceCatalogue(const std::string& yaml_path) {
  // TODO: Parse z3ed-resources.yaml when available
  // For now, use hardcoded command reference
  
  command_docs_["palette export"] = 
      "Export palette data to JSON file\n"
      "  --group <group>  Palette group (overworld, dungeon, sprite)\n"
      "  --id <id>        Palette ID (0-based index)\n"
      "  --to <file>      Output JSON file path";
  
  command_docs_["palette import"] = 
      "Import palette data from JSON file\n"
      "  --group <group>  Palette group (overworld, dungeon, sprite)\n"
      "  --id <id>        Palette ID (0-based index)\n"
      "  --from <file>    Input JSON file path";
  
  command_docs_["palette set-color"] = 
      "Modify a color in palette JSON file\n"
      "  --file <file>    Palette JSON file to modify\n"
      "  --index <index>  Color index (0-15 per palette)\n"
      "  --color <hex>    New color in hex (0xRRGGBB format)";
  
  command_docs_["overworld set-tile"] = 
      "Place a tile in the overworld\n"
      "  --map <id>       Map ID (0-based)\n"
      "  --x <x>          X coordinate (0-63)\n"
      "  --y <y>          Y coordinate (0-63)\n"
      "  --tile <hex>     Tile ID in hex (e.g., 0x02E for tree)";
  
  command_docs_["sprite set-position"] = 
      "Move a sprite to new position\n"
      "  --id <id>        Sprite ID\n"
      "  --x <x>          X coordinate\n"
      "  --y <y>          Y coordinate";
  
  command_docs_["dungeon set-room-tile"] = 
      "Place a tile in dungeon room\n"
      "  --room <id>      Room ID\n"
      "  --x <x>          X coordinate\n"
      "  --y <y>          Y coordinate\n"
      "  --tile <hex>     Tile ID";
  
  command_docs_["rom validate"] = 
      "Validate ROM integrity and structure";
  
  catalogue_loaded_ = true;
  return absl::OkStatus();
}

std::string PromptBuilder::BuildCommandReference() {
  std::ostringstream oss;
  
  oss << "# Available z3ed Commands\n\n";
  
  for (const auto& [cmd, docs] : command_docs_) {
    oss << "## " << cmd << "\n";
    oss << docs << "\n\n";
  }
  
  return oss.str();
}

std::string PromptBuilder::BuildFewShotExamplesSection() {
  std::ostringstream oss;
  
  oss << "# Example Command Sequences\n\n";
  oss << "Here are proven examples of how to accomplish common tasks:\n\n";
  
  for (const auto& example : examples_) {
    oss << "**User Request:** \"" << example.user_prompt << "\"\n";
    oss << "**Commands:**\n";
    oss << "```json\n[";
    
    std::vector<std::string> quoted_cmds;
    for (const auto& cmd : example.expected_commands) {
      quoted_cmds.push_back("\"" + cmd + "\"");
    }
    oss << absl::StrJoin(quoted_cmds, ", ");
    
    oss << "]\n```\n";
    oss << "*Explanation:* " << example.explanation << "\n\n";
  }
  
  return oss.str();
}

std::string PromptBuilder::BuildConstraintsSection() {
  return R"(
# Critical Constraints

1. **Output Format:** You MUST respond with ONLY a JSON array of strings
   - Each string is a complete z3ed command
   - NO explanatory text before or after
   - NO markdown code blocks (```json)
   - NO "z3ed" prefix in commands

2. **Command Syntax:** Follow the exact syntax shown in examples
   - Use correct flag names (--group, --id, --to, --from, etc.)
   - Use hex format for colors (0xRRGGBB) and tile IDs (0xNNN)
   - Coordinates are 0-based indices

3. **Common Patterns:**
   - Palette modifications: export → set-color → import
   - Multiple tile placement: multiple overworld set-tile commands
   - Validation: single rom validate command

4. **Tile IDs Reference (ALTTP):**
   - Tree: 0x02E
   - House (2x2): 0x0C0, 0x0C1, 0x0D0, 0x0D1
   - Water: 0x038
   - Grass: 0x000

5. **Error Prevention:**
   - Always export before modifying palettes
   - Use temporary file names (temp_*.json) for intermediate files
   - Validate coordinates are within bounds
)";
}

std::string PromptBuilder::BuildContextSection(const RomContext& context) {
  std::ostringstream oss;

  oss << "# Current ROM Context\n\n";

  // Use ResourceContextBuilder if a ROM is available
  if (rom_ && rom_->is_loaded()) {
    if (!resource_context_builder_) {
      resource_context_builder_ = std::make_unique<ResourceContextBuilder>(rom_);
    }
    auto resource_context_or = resource_context_builder_->BuildResourceContext();
    if (resource_context_or.ok()) {
      oss << resource_context_or.value();
    }
  }

  if (context.rom_loaded) {
    oss << "- **ROM Loaded:** Yes (" << context.rom_path << ")\n";
  } else {
    oss << "- **ROM Loaded:** No\n";
  }
  
  if (!context.current_editor.empty()) {
    oss << "- **Active Editor:** " << context.current_editor << "\n";
  }
  
  if (!context.editor_state.empty()) {
    oss << "- **Editor State:**\n";
    for (const auto& [key, value] : context.editor_state) {
      oss << "  - " << key << ": " << value << "\n";
    }
  }
  
  oss << "\n";
  return oss.str();
}

std::string PromptBuilder::BuildSystemInstruction() {
  std::ostringstream oss;
  
  oss << "You are an expert ROM hacking assistant for The Legend of Zelda: "
      << "A Link to the Past (ALTTP).\n\n";
  
  oss << "Your task is to generate a sequence of z3ed CLI commands to achieve "
      << "the user's request.\n\n";
  
  if (catalogue_loaded_) {
    oss << BuildCommandReference();
  }
  
  oss << BuildConstraintsSection();
  
  oss << "\n**Response Format:**\n";
  oss << "```json\n";
  oss << "[\"command1 --flag value\", \"command2 --flag value\"]\n";
  oss << "```\n";
  
  return oss.str();
}

std::string PromptBuilder::BuildSystemInstructionWithExamples() {
  std::ostringstream oss;
  
  oss << BuildSystemInstruction();
  oss << "\n---\n\n";
  oss << BuildFewShotExamplesSection();
  
  return oss.str();
}

std::string PromptBuilder::BuildContextualPrompt(
    const std::string& user_prompt,
    const RomContext& context) {
  std::ostringstream oss;
  
  if (context.rom_loaded || !context.current_editor.empty()) {
    oss << BuildContextSection(context);
    oss << "---\n\n";
  }
  
  oss << "**User Request:** " << user_prompt << "\n\n";
  oss << "Generate the appropriate z3ed commands as a JSON array.";
  
  return oss.str();
}

void PromptBuilder::AddFewShotExample(const FewShotExample& example) {
  examples_.push_back(example);
}

std::vector<FewShotExample> PromptBuilder::GetExamplesForCategory(
    const std::string& category) {
  std::vector<FewShotExample> result;
  
  for (const auto& example : examples_) {
    // Simple category matching based on keywords
    if (category == "palette" && 
        (example.user_prompt.find("palette") != std::string::npos ||
         example.user_prompt.find("color") != std::string::npos)) {
      result.push_back(example);
    } else if (category == "overworld" && 
               (example.user_prompt.find("place") != std::string::npos ||
                example.user_prompt.find("tree") != std::string::npos ||
                example.user_prompt.find("house") != std::string::npos)) {
      result.push_back(example);
    } else if (category == "validation" && 
               example.user_prompt.find("validate") != std::string::npos) {
      result.push_back(example);
    }
  }
  
  return result;
}

}  // namespace cli
}  // namespace yaze
