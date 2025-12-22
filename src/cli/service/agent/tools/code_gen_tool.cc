/**
 * @file code_gen_tool.cc
 * @brief Implementation of code generation tools
 */

#include "cli/service/agent/tools/code_gen_tool.h"

#include <algorithm>
#include <iostream>
#include <regex>
#include <sstream>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "rom/rom.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

// =============================================================================
// Static Data
// =============================================================================

// Known safe hook locations (address -> description)
const std::map<std::string, uint32_t> CodeGenToolBase::kKnownHooks = {
    {"EnableForceBlank", 0x00893D},
    {"Overworld_LoadMapProperties", 0x02AB08},
    {"Overworld_LoadSubscreenAndSilenceSFX1", 0x02AF19},
    {"Sprite_OverworldReloadAll", 0x09C499},
};

// Built-in ASM templates
const std::vector<AsmTemplate> CodeGenToolBase::kTemplates =
    CodeGenToolBase::InitializeTemplates();

std::vector<AsmTemplate> CodeGenToolBase::InitializeTemplates() {
  std::vector<AsmTemplate> templates;

  // NMI Hook Template
  templates.push_back(
      {"nmi_hook",
       R"(; NMI Hook Template
; Hooks into the NMI (Non-Maskable Interrupt) handler
org ${{NMI_HOOK_ADDRESS}}
  JSL {{LABEL}}_NMI
  NOP

freecode
{{LABEL}}_NMI:
  PHB : PHK : PLB
  {{CUSTOM_CODE}}
  PLB
  RTL
)",
       {"LABEL", "NMI_HOOK_ADDRESS", "CUSTOM_CODE"},
       "Hook into NMI handler for frame-by-frame code execution"});

  // Sprite Template
  templates.push_back(
      {"sprite",
       R"(; Sprite Template
; Sprite Variables:
; $0D00,X = Y pos (low)  $0D10,X = X pos (low)
; $0D20,X = Y pos (high) $0D30,X = X pos (high)
; $0DD0,X = State (08=init, 09=active)

freecode
{{SPRITE_NAME}}:
  PHB : PHK : PLB
  LDA $0DD0, X
  CMP #$08 : BEQ .initialize
  CMP #$09 : BEQ .main
  PLB : RTL

.initialize
  {{INIT_CODE}}
  LDA #$09 : STA $0DD0, X
  PLB : RTL

.main
  {{MAIN_CODE}}
  PLB : RTL
)",
       {"SPRITE_NAME", "INIT_CODE", "MAIN_CODE"},
       "Complete sprite with init and main loop"});

  // Freespace Allocation Template
  templates.push_back(
      {"freespace_alloc",
       R"(; Freespace Allocation
org ${{FREESPACE_ADDRESS}}
{{LABEL}}:
  {{CODE}}
  RTL

; Hook from existing code
org ${{HOOK_ADDRESS}}
  JSL {{LABEL}}
  {{NOP_FILL}}
)",
       {"LABEL", "FREESPACE_ADDRESS", "HOOK_ADDRESS", "CODE", "NOP_FILL"},
       "Allocate code in freespace with hook from existing code"});

  // Simple JSL Hook Template
  templates.push_back(
      {"jsl_hook",
       R"(; JSL Hook
org ${{HOOK_ADDRESS}}
  JSL {{LABEL}}
  {{NOP_FILL}}
)",
       {"HOOK_ADDRESS", "LABEL", "NOP_FILL"},
       "Simple JSL hook at address"});

  // Event Handler Template
  templates.push_back(
      {"event_handler",
       R"(; {{EVENT_TYPE}} Event Handler
org ${{HOOK_ADDRESS}}
  JSL {{LABEL}}_Handler
  NOP

freecode
{{LABEL}}_Handler:
  PHB : PHK : PLB
  PHP
  {{CUSTOM_CODE}}
  PLP
  PLB
  RTL
)",
       {"EVENT_TYPE", "HOOK_ADDRESS", "LABEL", "CUSTOM_CODE"},
       "Event handler with state preservation"});

  return templates;
}

// =============================================================================
// CodeGenToolBase
// =============================================================================

absl::Status CodeGenToolBase::ValidateHookAddress(Rom* rom,
                                                   uint32_t address) const {
  // Check ROM bounds
  if (address >= rom->size()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Address 0x%06X is beyond ROM size (0x%X)", address,
                        rom->size()));
  }

  // Check if address is already hooked (has JSL or JML)
  auto opcode_result = rom->ReadByte(address);
  if (!opcode_result.ok()) {
    return opcode_result.status();
  }
  uint8_t opcode = *opcode_result;

  if (opcode == 0x22) {  // JSL
    return absl::AlreadyExistsError(
        absl::StrFormat("Address 0x%06X already contains a JSL instruction",
                        address));
  }
  if (opcode == 0x5C) {  // JML
    return absl::AlreadyExistsError(
        absl::StrFormat("Address 0x%06X already contains a JML instruction",
                        address));
  }

  // Check alignment (JSL is 4 bytes, so we need at least 4 bytes available)
  if (address + 4 > rom->size()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Not enough space at 0x%06X for JSL hook (need 4 bytes)",
                        address));
  }

  return absl::OkStatus();
}

std::vector<FreeSpaceRegion> CodeGenToolBase::DetectFreeSpace(
    Rom* rom, size_t min_size) const {
  std::vector<FreeSpaceRegion> regions;

  // Known freespace regions to check
  struct RegionCandidate {
    uint32_t start;
    uint32_t end;
    const char* description;
  };

  const std::vector<RegionCandidate> candidates = {
      {0x1F8000, 0x1FFFFF, "Bank $3F freespace (32KB)"},
      {0x278000, 0x27FFFF, "Bank $4F freespace (32KB)"},
      {0x2F8000, 0x2FFFFF, "Bank $5F freespace (32KB)"},
      {0x378000, 0x37FFFF, "Bank $6F freespace (32KB)"},
      {0x3F8000, 0x3FFFFF, "Bank $7F freespace (32KB)"},
  };

  for (const auto& candidate : candidates) {
    // Ensure region is within ROM bounds
    if (candidate.start >= rom->size()) {
      continue;
    }

    uint32_t actual_end = std::min(candidate.end, static_cast<uint32_t>(rom->size() - 1));
    size_t region_size = actual_end - candidate.start + 1;

    // Count free bytes (0x00 or 0xFF)
    size_t free_bytes = 0;
    for (uint32_t addr = candidate.start; addr <= actual_end; ++addr) {
      auto byte_result = rom->ReadByte(addr);
      if (!byte_result.ok()) {
        continue;  // Skip unreadable bytes
      }
      uint8_t byte = *byte_result;
      if (byte == 0x00 || byte == 0xFF) {
        free_bytes++;
      }
    }

    int free_percent = (free_bytes * 100) / region_size;

    // Only include regions with significant free space and meeting min_size
    if (free_percent >= 80 && region_size >= min_size) {
      FreeSpaceRegion region;
      region.start = candidate.start;
      region.end = actual_end;
      region.description = candidate.description;
      region.free_percent = free_percent;
      regions.push_back(region);
    }
  }

  return regions;
}

std::string CodeGenToolBase::SubstitutePlaceholders(
    const std::string& template_code,
    const std::map<std::string, std::string>& params) const {
  std::string result = template_code;

  // Replace all {{PLACEHOLDER}} with corresponding values
  for (const auto& [key, value] : params) {
    std::string placeholder = "{{" + key + "}}";
    result = absl::StrReplaceAll(result, {{placeholder, value}});
  }

  return result;
}

absl::StatusOr<AsmTemplate> CodeGenToolBase::GetTemplate(
    const std::string& name) const {
  for (const auto& tmpl : kTemplates) {
    if (tmpl.name == name) {
      return tmpl;
    }
  }
  return absl::NotFoundError(
      absl::StrFormat("Template '%s' not found", name));
}

const std::vector<AsmTemplate>& CodeGenToolBase::GetAllTemplates() const {
  return kTemplates;
}

bool CodeGenToolBase::IsKnownHookLocation(uint32_t address) const {
  for (const auto& [name, addr] : kKnownHooks) {
    if (addr == address) {
      return true;
    }
  }
  return false;
}

std::string CodeGenToolBase::GetHookLocationDescription(uint32_t address) const {
  for (const auto& [name, addr] : kKnownHooks) {
    if (addr == address) {
      return name;
    }
  }
  return "Unknown";
}

std::string CodeGenToolBase::FormatResultAsJson(
    const CodeGenerationResult& result) const {
  std::ostringstream json;

  json << "{\n";
  json << "  \"success\": " << (result.success ? "true" : "false") << ",\n";

  // Generated code
  json << "  \"code\": ";
  if (!result.generated_code.empty()) {
    // Escape newlines and quotes for JSON
    std::string escaped_code = result.generated_code;
    escaped_code = absl::StrReplaceAll(escaped_code, {{"\\", "\\\\"}, {"\"", "\\\""}, {"\n", "\\n"}});
    json << "\"" << escaped_code << "\"";
  } else {
    json << "null";
  }
  json << ",\n";

  // Symbols
  json << "  \"symbols\": {";
  bool first_symbol = true;
  for (const auto& [label, address] : result.symbols) {
    if (!first_symbol) json << ", ";
    json << "\"" << label << "\": \"" << absl::StrFormat("0x%06X", address) << "\"";
    first_symbol = false;
  }
  json << "},\n";

  // Diagnostics
  json << "  \"diagnostics\": [\n";
  for (size_t i = 0; i < result.diagnostics.size(); ++i) {
    const auto& diag = result.diagnostics[i];
    json << "    {";
    json << "\"severity\": \"" << diag.SeverityString() << "\", ";
    json << "\"message\": \"" << diag.message << "\"";
    if (diag.address != 0) {
      json << ", \"address\": \"" << absl::StrFormat("0x%06X", diag.address) << "\"";
    }
    json << "}";
    if (i < result.diagnostics.size() - 1) json << ",";
    json << "\n";
  }
  json << "  ]\n";

  json << "}\n";

  return json.str();
}

std::string CodeGenToolBase::FormatResultAsText(
    const CodeGenerationResult& result) const {
  std::ostringstream text;

  text << "Code Generation Result\n";
  text << "======================\n\n";

  text << "Status: " << (result.success ? "SUCCESS" : "FAILED") << "\n\n";

  // Generated code
  if (!result.generated_code.empty()) {
    text << "Generated Code:\n";
    text << "---------------\n";
    text << result.generated_code << "\n\n";
  }

  // Symbols
  if (!result.symbols.empty()) {
    text << "Symbols:\n";
    for (const auto& [label, address] : result.symbols) {
      text << "  " << label << " = " << absl::StrFormat("$%06X", address) << "\n";
    }
    text << "\n";
  }

  // Diagnostics
  if (!result.diagnostics.empty()) {
    text << "Diagnostics:\n";
    for (const auto& diag : result.diagnostics) {
      std::string prefix;
      switch (diag.severity) {
        case CodeGenerationDiagnostic::Severity::kInfo:
          prefix = "[INFO]";
          break;
        case CodeGenerationDiagnostic::Severity::kWarning:
          prefix = "[WARN]";
          break;
        case CodeGenerationDiagnostic::Severity::kError:
          prefix = "[ERROR]";
          break;
      }

      text << "  " << prefix << " " << diag.message;
      if (diag.address != 0) {
        text << absl::StrFormat(" (at $%06X)", diag.address);
      }
      text << "\n";
    }
  }

  return text.str();
}

// =============================================================================
// CodeGenAsmHookTool
// =============================================================================

absl::Status CodeGenAsmHookTool::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  CodeGenerationResult result;
  result.success = true;

  // Parse arguments
  auto address_result = parser.GetHex("address");
  if (!address_result.ok()) {
    result.AddError("Invalid or missing address");
    std::string format = parser.GetString("format").value_or("json");
    std::cout << (format == "json" ? FormatResultAsJson(result)
                                    : FormatResultAsText(result));
    return address_result.status();
  }
  uint32_t address = static_cast<uint32_t>(*address_result);

  std::string label = parser.GetString("label").value();
  int nop_fill = parser.GetInt("nop-fill").value_or(0);
  std::string format = parser.GetString("format").value_or("json");

  // Validate hook address
  auto validate_status = ValidateHookAddress(rom, address);
  if (!validate_status.ok()) {
    result.AddError(std::string(validate_status.message()), address);
    std::cout << (format == "json" ? FormatResultAsJson(result)
                                    : FormatResultAsText(result));
    return validate_status;
  }

  // Check if this is a known safe hook location
  if (IsKnownHookLocation(address)) {
    result.AddInfo(
        absl::StrFormat("Using known safe hook: %s",
                        GetHookLocationDescription(address)),
        address);
  } else {
    result.AddWarning(
        "Address is not a known safe hook location. Verify manually.", address);
  }

  // Generate NOP fill
  std::string nop_fill_str;
  for (int i = 0; i < nop_fill; ++i) {
    nop_fill_str += "NOP\n  ";
  }
  if (!nop_fill_str.empty()) {
    nop_fill_str.pop_back();  // Remove trailing newline
    nop_fill_str.pop_back();
  }

  // Generate hook code using jsl_hook template
  auto tmpl = GetTemplate("jsl_hook");
  if (!tmpl.ok()) {
    result.AddError("Failed to load jsl_hook template");
    std::cout << (format == "json" ? FormatResultAsJson(result)
                                    : FormatResultAsText(result));
    return tmpl.status();
  }

  std::map<std::string, std::string> params = {
      {"HOOK_ADDRESS", absl::StrFormat("%06X", address)},
      {"LABEL", label},
      {"NOP_FILL", nop_fill_str},
  };

  result.generated_code = SubstitutePlaceholders(tmpl->code_template, params);
  result.symbols[label] = address;

  result.AddInfo(absl::StrFormat("Generated JSL hook to %s at $%06X", label, address),
                 address);

  // Output result
  std::cout << (format == "json" ? FormatResultAsJson(result)
                                  : FormatResultAsText(result));

  formatter.AddField("status", "complete");
  return absl::OkStatus();
}

// =============================================================================
// CodeGenFreespacePatchTool
// =============================================================================

absl::Status CodeGenFreespacePatchTool::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  CodeGenerationResult result;
  result.success = true;

  // Parse arguments
  std::string label = parser.GetString("label").value();
  int size = parser.GetInt("size").value_or(0x100);
  int prefer_bank = parser.GetInt("prefer-bank").value_or(-1);
  std::string format = parser.GetString("format").value_or("json");

  // Detect available freespace
  auto regions = DetectFreeSpace(rom, size);

  if (regions.empty()) {
    result.AddError(absl::StrFormat(
        "No suitable freespace found for %d bytes (need >=%d bytes with >=80%% free)",
        size, size));
    std::cout << (format == "json" ? FormatResultAsJson(result)
                                    : FormatResultAsText(result));
    return absl::NotFoundError("No freespace available");
  }

  // Select best region (prefer requested bank if specified)
  FreeSpaceRegion selected_region = regions[0];
  if (prefer_bank >= 0) {
    for (const auto& region : regions) {
      uint8_t bank = (region.start >> 16) & 0xFF;
      if (bank == static_cast<uint8_t>(prefer_bank)) {
        selected_region = region;
        break;
      }
    }
  }

  result.AddInfo(absl::StrFormat("Selected region: %s ($%06X-$%06X, %d%% free)",
                                 selected_region.description.c_str(),
                                 selected_region.start, selected_region.end,
                                 selected_region.free_percent));

  // Generate patch code
  std::ostringstream code;
  code << "; Freespace Allocation\n";
  code << "; Selected: " << selected_region.description << "\n";
  code << absl::StrFormat("org $%06X\n", selected_region.start);
  code << label << ":\n";
  code << "  ; Your code here (up to " << size << " bytes)\n";
  code << "  RTL\n";

  result.generated_code = code.str();
  result.symbols[label] = selected_region.start;

  result.AddInfo(absl::StrFormat("Allocated %d bytes in %s", size,
                                 selected_region.description.c_str()),
                 selected_region.start);

  // List other available regions
  if (regions.size() > 1) {
    result.AddInfo(absl::StrFormat("Found %zu other suitable regions",
                                   regions.size() - 1));
  }

  // Output result
  std::cout << (format == "json" ? FormatResultAsJson(result)
                                  : FormatResultAsText(result));

  formatter.AddField("status", "complete");
  return absl::OkStatus();
}

// =============================================================================
// CodeGenSpriteTemplateTool
// =============================================================================

absl::Status CodeGenSpriteTemplateTool::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  CodeGenerationResult result;
  result.success = true;

  // Parse arguments
  std::string name = parser.GetString("name").value();
  std::string init_code = parser.GetString("init-code").value_or(
      "; Initialize sprite here\n  LDA #$00 : STA $0F50, X  ; Example: Clear state");
  std::string main_code = parser.GetString("main-code").value_or(
      "; Main sprite logic here\n  JSR Sprite_Move  ; Example: Move sprite");
  std::string format = parser.GetString("format").value_or("json");

  // Get sprite template
  auto tmpl = GetTemplate("sprite");
  if (!tmpl.ok()) {
    result.AddError("Failed to load sprite template");
    std::cout << (format == "json" ? FormatResultAsJson(result)
                                    : FormatResultAsText(result));
    return tmpl.status();
  }

  // Substitute placeholders
  std::map<std::string, std::string> params = {
      {"SPRITE_NAME", name},
      {"INIT_CODE", init_code},
      {"MAIN_CODE", main_code},
  };

  result.generated_code = SubstitutePlaceholders(tmpl->code_template, params);
  result.AddInfo(absl::StrFormat("Generated sprite template for '%s'", name.c_str()));

  // Output result
  std::cout << (format == "json" ? FormatResultAsJson(result)
                                  : FormatResultAsText(result));

  formatter.AddField("status", "complete");
  return absl::OkStatus();
}

// =============================================================================
// CodeGenEventHandlerTool
// =============================================================================

absl::Status CodeGenEventHandlerTool::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  CodeGenerationResult result;
  result.success = true;

  // Parse arguments
  std::string type = parser.GetString("type").value();
  std::string label = parser.GetString("label").value();
  std::string custom_code = parser.GetString("custom-code").value_or(
      "; Your custom code here");
  std::string format = parser.GetString("format").value_or("json");

  // Validate event type
  std::map<std::string, uint32_t> event_addresses = {
      {"nmi", 0x008040},    // NMI hook location
      {"irq", 0x008050},    // IRQ hook location (example)
      {"reset", 0x008000},  // Reset vector (example)
  };

  auto it = event_addresses.find(type);
  if (it == event_addresses.end()) {
    result.AddError(absl::StrFormat(
        "Unknown event type '%s'. Valid types: nmi, irq, reset", type.c_str()));
    std::cout << (format == "json" ? FormatResultAsJson(result)
                                    : FormatResultAsText(result));
    return absl::InvalidArgumentError("Invalid event type");
  }

  uint32_t hook_address = it->second;

  // Get event handler template
  auto tmpl = GetTemplate("event_handler");
  if (!tmpl.ok()) {
    result.AddError("Failed to load event_handler template");
    std::cout << (format == "json" ? FormatResultAsJson(result)
                                    : FormatResultAsText(result));
    return tmpl.status();
  }

  // Substitute placeholders
  std::map<std::string, std::string> params = {
      {"EVENT_TYPE", type},
      {"HOOK_ADDRESS", absl::StrFormat("%06X", hook_address)},
      {"LABEL", label},
      {"CUSTOM_CODE", custom_code},
  };

  result.generated_code = SubstitutePlaceholders(tmpl->code_template, params);
  result.symbols[label + "_Handler"] = hook_address;

  result.AddInfo(absl::StrFormat("Generated %s event handler '%s'",
                                 type.c_str(), label.c_str()),
                 hook_address);

  // Output result
  std::cout << (format == "json" ? FormatResultAsJson(result)
                                  : FormatResultAsText(result));

  formatter.AddField("status", "complete");
  return absl::OkStatus();
}

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze
