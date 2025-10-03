#include "cli/service/prompt_builder.h"

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
  // Palette manipulation examples
  examples_.push_back({
      "Change the color at index 5 in palette 0 to red",
      {
          "palette export --group overworld --id 0 --to temp_palette.json",
          "palette set-color --file temp_palette.json --index 5 --color 0xFF0000",
          "palette import --group overworld --id 0 --from temp_palette.json"
      },
      "Export palette, modify specific color, then import back"
  });
  
  examples_.push_back({
      "Make all soldiers red",
      {
          "palette export --group sprite --id 3 --to soldier_palette.json",
          "palette set-color --file soldier_palette.json --index 1 --color 0xFF0000",
          "palette set-color --file soldier_palette.json --index 2 --color 0xCC0000",
          "palette import --group sprite --id 3 --from soldier_palette.json"
      },
      "Modify multiple colors in a sprite palette"
  });
  
  // Overworld manipulation examples
  examples_.push_back({
      "Place a tree at coordinates (10, 20) on map 0",
      {
          "overworld set-tile --map 0 --x 10 --y 20 --tile 0x02E"
      },
      "Tree tile ID is 0x02E in ALTTP"
  });
  
  examples_.push_back({
      "Put a house at position 5, 5",
      {
          "overworld set-tile --map 0 --x 5 --y 5 --tile 0x0C0",
          "overworld set-tile --map 0 --x 6 --y 5 --tile 0x0C1",
          "overworld set-tile --map 0 --x 5 --y 6 --tile 0x0D0",
          "overworld set-tile --map 0 --x 6 --y 6 --tile 0x0D1"
      },
      "Houses require 4 tiles (2x2 grid)"
  });
  
  // Validation examples
  examples_.push_back({
      "Validate the ROM",
      {
          "rom validate"
      },
      "Simple validation command"
  });
  
  examples_.push_back({
      "Check if my changes are valid",
      {
          "rom validate"
      },
      "Validation ensures ROM integrity"
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
