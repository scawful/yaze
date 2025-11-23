#include "cli/service/agent/rom_debug_agent.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "util/log.h"

namespace yaze {
namespace cli {
namespace agent {

using namespace yaze::agent;

namespace {

// Helper to format a 24-bit SNES address
std::string FormatSnesAddress(uint32_t address) {
  return absl::StrFormat("$%02X:%04X", (address >> 16) & 0xFF, address & 0xFFFF);
}

// Helper to check if an opcode is a branch/jump instruction
bool IsBranchOrJump(uint8_t opcode) {
  // 65816 branch and jump opcodes
  static const std::set<uint8_t> branch_jump_opcodes = {
      0x10,  // BPL
      0x30,  // BMI
      0x50,  // BVC
      0x70,  // BVS
      0x80,  // BRA
      0x82,  // BRL
      0x90,  // BCC
      0xB0,  // BCS
      0xD0,  // BNE
      0xF0,  // BEQ
      0x4C,  // JMP abs
      0x5C,  // JML long
      0x6C,  // JMP (abs)
      0x7C,  // JMP (abs,X)
      0xDC,  // JML [abs]
      0x20,  // JSR abs
      0x22,  // JSL long
      0xFC,  // JSR (abs,X)
  };
  return branch_jump_opcodes.count(opcode) > 0;
}

// Helper to check if an opcode is a return instruction
bool IsReturn(uint8_t opcode) {
  return opcode == 0x60 ||  // RTS
         opcode == 0x6B ||  // RTL
         opcode == 0x40;    // RTI
}

// Helper to check if an opcode modifies the stack
bool ModifiesStack(uint8_t opcode) {
  static const std::set<uint8_t> stack_opcodes = {
      0x48, 0x08,  // PHA, PHP
      0x68, 0x28,  // PLA, PLP
      0xDA, 0x5A,  // PHX, PHY
      0xFA, 0x7A,  // PLX, PLY
      0x8B,        // PHB
      0xAB,        // PLB
      0x0B,        // PHD
      0x2B,        // PLD
      0x4B,        // PHK
      0x62,        // PER
      0xD4,        // PEI
      0xF4,        // PEA
  };
  return stack_opcodes.count(opcode) > 0;
}

}  // namespace

RomDebugAgent::RomDebugAgent(yaze::agent::EmulatorServiceImpl* emulator_service)
    : emulator_service_(emulator_service),
      disassembler_(std::make_unique<Disassembler65816>()),
      symbol_provider_(std::make_unique<yaze::emu::debug::SymbolProvider>()) {
  // Initialize with default M and X flags (8-bit mode)
  disassembler_->SetFlags(true, true);
}

absl::StatusOr<RomDebugAgent::BreakpointAnalysis> RomDebugAgent::AnalyzeBreakpoint(
    const yaze::agent::BreakpointHitResponse& hit) {
  BreakpointAnalysis analysis;

  // Basic information from the hit
  if (hit.has_breakpoint()) {
    analysis.address = hit.breakpoint().address();
  } else {
    analysis.address = hit.cpu_state().pc();
  }

  // Get symbol or format address
  if (symbol_provider_->HasSymbols()) {
    analysis.location_description = symbol_provider_->FormatAddress(analysis.address);
  } else {
    analysis.location_description = FormatSnesAddress(analysis.address);
  }

  // Extract registers from the hit response
  const auto& cpu = hit.cpu_state();
  analysis.registers["A"] = cpu.a();
  analysis.registers["X"] = cpu.x();
  analysis.registers["Y"] = cpu.y();
  analysis.registers["S"] = cpu.sp();
  analysis.registers["PC"] = cpu.pc();
  analysis.registers["P"] = cpu.status();
  analysis.registers["DB"] = cpu.db();
  analysis.registers["PB"] = cpu.pb();

  // Disassemble the current instruction
  grpc::ServerContext context;
  yaze::agent::DisassemblyRequest disasm_req;
  disasm_req.set_start_address(analysis.address);
  disasm_req.set_count(1);
  yaze::agent::DisassemblyResponse disasm_resp;

  auto status = emulator_service_->GetDisassembly(&context, &disasm_req, &disasm_resp);
  if (!status.ok()) {
    return absl::InternalError("Failed to get disassembly");
  }

  if (disasm_resp.lines_size() > 0) {
    const auto& inst = disasm_resp.lines(0);
    analysis.disassembly = inst.mnemonic() + " " + inst.operand_str();
    
    // Analyze instruction for AI explanation
    auto explanation = AnalyzeInstruction(analysis.address, nullptr, 0); // We don't have raw bytes here easily without reading memory
    if (explanation.ok()) {
      analysis.instruction_explanation = *explanation;
    }
  }

  // Get context (surrounding lines)
  analysis.context_lines = GetDisassemblyContext(analysis.address, 5, 5);

  // Build call stack
  analysis.call_stack = BuildCallStack(cpu.pc());

  // Detect issues
  auto issue = DetectIssuePattern(analysis.address, nullptr, 0);
  if (issue) {
    analysis.suggestions.push_back(issue->suggested_fix);
  }

  return analysis;
}

absl::StatusOr<RomDebugAgent::MemoryAnalysis> RomDebugAgent::AnalyzeMemory(
    uint32_t address, size_t length) {
  MemoryAnalysis analysis;
  analysis.address = address;
  analysis.length = length;

  // Read the memory
  grpc::ServerContext context;
  MemoryRequest mem_req;
  mem_req.set_address(address);
  mem_req.set_size(length);
  MemoryResponse mem_resp;

  auto status = emulator_service_->ReadMemory(&context, &mem_req, &mem_resp);
  if (!status.ok()) {
    return absl::InternalError("Failed to read memory");
  }

  analysis.data.assign(mem_resp.data().begin(), mem_resp.data().end());

  // Identify the data type and structure
  analysis.data_type = IdentifyDataType(address);

  // Get structure information if available
  auto struct_info = GetStructureInfo(address);
  if (struct_info.has_value()) {
    analysis.structure_name = struct_info.value();
  }

  // Generate description
  analysis.description = DescribeMemoryLocation(address);

  // Parse fields for known structures
  if (address >= SPRITE_TABLE_START && address < SPRITE_TABLE_END) {
    // Parse sprite data
    uint32_t sprite_index = (address - SPRITE_TABLE_START) / 0x10;
    analysis.fields["sprite_index"] = sprite_index;

    if (length >= 0x10) {
      analysis.fields["state"] = analysis.data[0x00];
      analysis.fields["x_pos_low"] = analysis.data[0x01];
      analysis.fields["x_pos_high"] = analysis.data[0x02];
      analysis.fields["y_pos_low"] = analysis.data[0x03];
      analysis.fields["y_pos_high"] = analysis.data[0x04];

      // Check for anomalies
      if (analysis.data[0x00] == 0x00) {
        analysis.anomalies.push_back("Sprite is inactive (state = 0)");
      }
      if (analysis.data[0x02] > 0x01) {
        analysis.anomalies.push_back("Sprite X position exceeds screen bounds");
      }
    }
  } else if (address >= SRAM_START && address <= SRAM_END) {
    // Parse save data
    if (address == PLAYER_HEALTH) {
      analysis.fields["current_health"] = analysis.data[0];
      if (analysis.data[0] == 0) {
        analysis.anomalies.push_back("Player health is zero - death state");
      }
    } else if (address == PLAYER_MAX_HEALTH) {
      analysis.fields["max_health"] = analysis.data[0];
    } else if (address >= INVENTORY_START && address < INVENTORY_START + 0x40) {
      uint32_t item_slot = address - INVENTORY_START;
      analysis.fields["inventory_slot"] = item_slot;
      analysis.fields["item_value"] = analysis.data[0];
    }
  } else if (address >= DMA0_CONTROL && address < DMA0_CONTROL + 0x80) {
    // Parse DMA registers
    uint32_t dma_channel = (address - DMA0_CONTROL) / 0x10;
    analysis.fields["dma_channel"] = dma_channel;

    uint32_t offset = (address - DMA0_CONTROL) % 0x10;
    switch (offset) {
      case 0x00:
        analysis.fields["dma_control"] = analysis.data[0];
        break;
      case 0x01:
        analysis.fields["dma_destination"] = analysis.data[0];
        break;
      case 0x02:
      case 0x03:
      case 0x04:
        // Source address bytes
        break;
      case 0x05:
      case 0x06:
        // Transfer size
        break;
    }

    // Check for DMA issues
    if ((analysis.data[0] & 0x80) && address == DMA0_CONTROL) {
      analysis.anomalies.push_back("DMA channel enabled during active display - may cause glitches");
    }
  }

  // Check for common issues
  if (analysis.data_type == "sprite" && length >= 16) {
    // Check sprite corruption patterns
    bool all_zero = std::all_of(analysis.data.begin(),
                                 analysis.data.begin() + std::min(size_t(16), length),
                                 [](uint8_t b) { return b == 0; });
    bool all_ff = std::all_of(analysis.data.begin(),
                              analysis.data.begin() + std::min(size_t(16), length),
                              [](uint8_t b) { return b == 0xFF; });

    if (all_zero) {
      analysis.anomalies.push_back("Sprite data is all zeros - likely uninitialized");
    }
    if (all_ff) {
      analysis.anomalies.push_back("Sprite data is all 0xFF - possible corruption");
    }
  }

  return analysis;
}

absl::StatusOr<std::string> RomDebugAgent::ExplainExecutionTrace(
    const std::vector<ExecutionTraceBuffer::TraceEntry>& trace) {
  std::stringstream explanation;

  explanation << "Execution Trace Analysis:\n";
  explanation << "=========================\n\n";

  // Track subroutine calls and returns
  std::vector<std::string> call_stack;
  int indent_level = 0;

  for (size_t i = 0; i < trace.size(); ++i) {
    const auto& entry = trace[i];

    // Format the entry with indentation for call hierarchy
    std::string indent(indent_level * 2, ' ');

    // Check if this is a subroutine call
    if (entry.opcode == 0x20 || entry.opcode == 0x22 || entry.opcode == 0xFC) {
      // JSR, JSL, JSR (abs,X)
      uint32_t target_addr = 0;
      if (entry.operands.size() >= 2) {
        target_addr = entry.operands[0] | (entry.operands[1] << 8);
        if (entry.opcode == 0x22 && entry.operands.size() >= 3) {
          target_addr |= (entry.operands[2] << 16);
        }
      }

      std::string target_name = symbol_provider_->HasSymbols()
          ? symbol_provider_->FormatAddress(target_addr)
          : FormatSnesAddress(target_addr);

      explanation << indent << "→ CALL " << target_name << " from "
                  << FormatSnesAddress(entry.address) << "\n";

      call_stack.push_back(target_name);
      indent_level++;

    } else if (IsReturn(entry.opcode)) {
      // Return instruction
      if (!call_stack.empty()) {
        indent_level = std::max(0, indent_level - 1);
        indent = std::string(indent_level * 2, ' ');
        explanation << indent << "← RETURN from " << call_stack.back() << "\n";
        call_stack.pop_back();
      }

    } else if (IsBranchOrJump(entry.opcode)) {
      // Branch or jump
      std::string condition;
      switch (entry.opcode) {
        case 0x10: condition = " (if plus)"; break;
        case 0x30: condition = " (if minus)"; break;
        case 0x50: condition = " (if overflow clear)"; break;
        case 0x70: condition = " (if overflow set)"; break;
        case 0x90: condition = " (if carry clear)"; break;
        case 0xB0: condition = " (if carry set)"; break;
        case 0xD0: condition = " (if not zero)"; break;
        case 0xF0: condition = " (if zero)"; break;
        case 0x80:
        case 0x82:
        case 0x4C:
        case 0x5C: condition = " (unconditional)"; break;
      }

      explanation << indent << "  " << entry.mnemonic << " " << entry.operand_str
                  << condition << "\n";

    } else {
      // Regular instruction - only show significant ones
      bool is_significant = false;
      std::string description;

      // Memory access instructions
      if (entry.mnemonic.find("LD") != std::string::npos ||
          entry.mnemonic.find("ST") != std::string::npos) {
        is_significant = true;

        // Try to identify what's being accessed
        if (entry.operands.size() >= 2) {
          uint32_t addr = entry.operands[0] | (entry.operands[1] << 8);
          if (entry.operands.size() >= 3) {
            addr |= (entry.operands[2] << 16);
          }
          description = " ; " + DescribeMemoryLocation(addr);
        }
      }

      // Stack operations
      if (ModifiesStack(entry.opcode)) {
        is_significant = true;
        description = " ; Stack: ";
        if (entry.opcode == 0x48 || entry.opcode == 0x08) {
          description += "pushing";
        } else if (entry.opcode == 0x68 || entry.opcode == 0x28) {
          description += "pulling";
        }
      }

      // Arithmetic/logic that changes flags significantly
      if (entry.mnemonic.find("CMP") != std::string::npos ||
          entry.mnemonic.find("BIT") != std::string::npos ||
          entry.mnemonic.find("TST") != std::string::npos) {
        is_significant = true;
        description = " ; Testing/comparing";
      }

      if (is_significant) {
        explanation << indent << "  " << FormatSnesAddress(entry.address)
                    << ": " << entry.mnemonic << " " << entry.operand_str
                    << description << "\n";
      }
    }

    // Check for patterns
    if (i > 0) {
      // Detect infinite loops
      if (entry.address == trace[i-1].address && IsBranchOrJump(entry.opcode)) {
        explanation << indent << "  ⚠️  POSSIBLE INFINITE LOOP DETECTED\n";
      }

      // Detect rapid DMA
      if (entry.address >= DMA0_CONTROL && entry.address < DMA0_CONTROL + 0x80) {
        if (i > 0 && trace[i-1].address >= DMA0_CONTROL &&
            trace[i-1].address < DMA0_CONTROL + 0x80) {
          explanation << indent << "  ⚠️  RAPID DMA OPERATIONS - CHECK TIMING\n";
        }
      }
    }
  }

  // Summary
  explanation << "\nSummary:\n";
  explanation << "--------\n";
  explanation << "Total instructions: " << trace.size() << "\n";
  explanation << "Subroutine depth: " << indent_level << "\n";
  if (!call_stack.empty()) {
    explanation << "Unmatched calls: " << absl::StrJoin(call_stack, ", ") << "\n";
  }

  return explanation.str();
}

absl::StatusOr<RomDebugAgent::PatchComparisonResult> RomDebugAgent::ComparePatch(
    uint32_t address, size_t length, const std::vector<uint8_t>& original) {
  PatchComparisonResult result;
  result.address = address;
  result.length = length;
  result.original_code = original;
  result.is_safe = true;  // Assume safe until proven otherwise

  // Read the patched code from emulator
  grpc::ServerContext context;
  MemoryRequest mem_req;
  mem_req.set_address(address);
  mem_req.set_size(length);
  MemoryResponse mem_resp;

  auto status = emulator_service_->ReadMemory(&context, &mem_req, &mem_resp);
  if (!status.ok()) {
    return absl::InternalError("Failed to read patched memory");
  }

  result.patched_code.assign(mem_resp.data().begin(), mem_resp.data().end());

  // Disassemble both versions
  std::stringstream orig_disasm, patch_disasm;
  size_t orig_offset = 0, patch_offset = 0;

  while (orig_offset < original.size() && patch_offset < result.patched_code.size()) {
    // Disassemble original
    std::string orig_mnem, orig_operand;
    std::vector<uint8_t> orig_operands;
    uint8_t orig_size = disassembler_->DisassembleInstruction(
        address + orig_offset,
        original.data() + orig_offset,
        orig_mnem, orig_operand, orig_operands);

    orig_disasm << FormatSnesAddress(address + orig_offset) << ": "
                << orig_mnem << " " << orig_operand << "\n";

    // Disassemble patched
    std::string patch_mnem, patch_operand;
    std::vector<uint8_t> patch_operands;
    uint8_t patch_size = disassembler_->DisassembleInstruction(
        address + patch_offset,
        result.patched_code.data() + patch_offset,
        patch_mnem, patch_operand, patch_operands);

    patch_disasm << FormatSnesAddress(address + patch_offset) << ": "
                 << patch_mnem << " " << patch_operand << "\n";

    // Compare instructions
    if (orig_mnem != patch_mnem || orig_operand != patch_operand) {
      result.differences.push_back(absl::StrFormat(
          "At %s: '%s %s' → '%s %s'",
          FormatSnesAddress(address + orig_offset),
          orig_mnem, orig_operand,
          patch_mnem, patch_operand));

      // Check for potential issues

      // Check if jump target is valid
      if (IsBranchOrJump(result.patched_code[patch_offset])) {
        uint32_t target = 0;
        if (patch_operands.size() >= 2) {
          target = patch_operands[0] | (patch_operands[1] << 8);
          if (patch_operands.size() >= 3) {
            target |= (patch_operands[2] << 16);
          }
        }

        if (!IsValidJumpTarget(target)) {
          result.potential_issues.push_back(absl::StrFormat(
              "Invalid jump target at %s: %s",
              FormatSnesAddress(address + patch_offset),
              FormatSnesAddress(target)));
          result.is_safe = false;
        }
      }

      // Check for stack imbalance
      bool orig_modifies_stack = ModifiesStack(original[orig_offset]);
      bool patch_modifies_stack = ModifiesStack(result.patched_code[patch_offset]);

      if (orig_modifies_stack != patch_modifies_stack) {
        result.potential_issues.push_back(
            "Stack modification mismatch - may cause stack imbalance");
        result.is_safe = false;
      }
    }

    orig_offset += orig_size;
    patch_offset += patch_size;
  }

  result.original_disassembly = orig_disasm.str();
  result.patched_disassembly = patch_disasm.str();

  // Additional safety checks

  // Check if patch overwrites critical areas
  if (IsCriticalMemoryArea(address)) {
    result.potential_issues.push_back(
        "Patch modifies critical system area - verify this is intentional");
    result.is_safe = false;
  }

  // Check for NOP slides (common in bad patches)
  int nop_count = 0;
  for (uint8_t byte : result.patched_code) {
    if (byte == 0xEA) {  // NOP
      nop_count++;
    }
  }
  if (nop_count > 5) {
    result.potential_issues.push_back(absl::StrFormat(
        "Patch contains %d NOPs - possible padding or removed code", nop_count));
  }

  // Check for BRK instructions (usually indicates problems)
  for (size_t i = 0; i < result.patched_code.size(); ++i) {
    if (result.patched_code[i] == 0x00) {  // BRK
      result.potential_issues.push_back(absl::StrFormat(
          "BRK instruction at %s - usually indicates error",
          FormatSnesAddress(address + i)));
      result.is_safe = false;
    }
  }

  return result;
}

std::vector<RomDebugAgent::DetectedIssue> RomDebugAgent::ScanForIssues(
    uint32_t start_address, uint32_t end_address) {
  std::vector<DetectedIssue> issues;

  // Read the code region
  grpc::ServerContext context;
  MemoryRequest mem_req;
  mem_req.set_address(start_address);
  mem_req.set_size(end_address - start_address);
  MemoryResponse mem_resp;

  auto status = emulator_service_->ReadMemory(&context, &mem_req, &mem_resp);
  if (!status.ok()) {
    return issues;
  }

  const uint8_t* code = reinterpret_cast<const uint8_t*>(mem_resp.data().data());
  size_t code_size = mem_resp.data().size();

  size_t offset = 0;
  while (offset < code_size) {
    uint32_t current_addr = start_address + offset;

    // Check for specific patterns
    auto issue = DetectIssuePattern(current_addr, code + offset, code_size - offset);
    if (issue.has_value()) {
      issues.push_back(issue.value());
    }

    // Get instruction size to advance
    uint8_t inst_size = disassembler_->GetInstructionSize(code[offset]);
    if (inst_size == 0) {
      // Invalid opcode
      issues.push_back({
          IssueType::kInvalidOpcode,
          current_addr,
          absl::StrFormat("Invalid opcode $%02X at %s",
                          code[offset], FormatSnesAddress(current_addr)),
          "Check if this is data being executed as code",
          5
      });
      offset++;
    } else {
      offset += inst_size;
    }
  }

  return issues;
}

bool RomDebugAgent::IsValidJumpTarget(uint32_t address) const {
  // Check if address is in valid ROM or RAM range
  if (address < 0x008000) {
    // Low RAM - generally okay for some routines
    return true;
  }
  if (address >= 0x008000 && address < 0x7E0000) {
    // ROM space - valid
    return true;
  }
  if (address >= WRAM_START && address <= WRAM_END) {
    // WRAM - valid but unusual for code
    return true;
  }
  if (address >= 0x800000 && address < 0xC00000) {
    // Extended ROM banks - valid
    return true;
  }

  // Invalid ranges
  return false;
}

bool RomDebugAgent::HasStackImbalance(uint32_t routine_start, uint32_t routine_end) {
  // Read the routine
  grpc::ServerContext context;
  MemoryRequest mem_req;
  mem_req.set_address(routine_start);
  mem_req.set_size(routine_end - routine_start);
  MemoryResponse mem_resp;

  auto status = emulator_service_->ReadMemory(&context, &mem_req, &mem_resp);
  if (!status.ok()) {
    return false;
  }

  const uint8_t* code = reinterpret_cast<const uint8_t*>(mem_resp.data().data());
  size_t code_size = mem_resp.data().size();

  // Track stack depth
  int stack_depth = 0;
  size_t offset = 0;

  while (offset < code_size) {
    uint8_t opcode = code[offset];

    // Track pushes and pulls
    switch (opcode) {
      case 0x48:  // PHA
      case 0x08:  // PHP
      case 0xDA:  // PHX
      case 0x5A:  // PHY
      case 0x8B:  // PHB
      case 0x0B:  // PHD
      case 0x4B:  // PHK
        stack_depth++;
        break;

      case 0x68:  // PLA
      case 0x28:  // PLP
      case 0xFA:  // PLX
      case 0x7A:  // PLY
      case 0xAB:  // PLB
      case 0x2B:  // PLD
        stack_depth--;
        break;

      case 0x62:  // PER (push effective address)
      case 0xD4:  // PEI
      case 0xF4:  // PEA
        stack_depth += 2;  // These push 16-bit values
        break;
    }

    // Check for return
    if (IsReturn(opcode)) {
      // At return, stack should be balanced
      return stack_depth != 0;
    }

    uint8_t inst_size = disassembler_->GetInstructionSize(opcode);
    offset += (inst_size > 0) ? inst_size : 1;
  }

  // If we didn't find a return, check final depth
  return stack_depth != 0;
}

bool RomDebugAgent::IsMemoryWriteSafe(uint32_t address, size_t length) const {
  // Check if write touches critical areas
  uint32_t end_address = address + length;

  // Check system vectors
  if ((address <= 0x00FFFF && end_address > 0x00FFE0)) {
    return false;  // Writing to interrupt vectors
  }

  // Check NMI flag
  if (address <= NMI_FLAG && end_address > NMI_FLAG) {
    return false;  // Modifying NMI flag can break frame timing
  }

  // Check PPU registers during active display
  if (address >= 0x002100 && address <= 0x00213F) {
    // Some PPU registers are unsafe during active display
    // This would need frame timing info to be accurate
    return true;  // Assume safe for now
  }

  // Check DMA registers
  if (address >= DMA0_CONTROL && address < DMA0_CONTROL + 0x80) {
    // DMA writes during active display can cause issues
    return true;  // Assume safe for now
  }

  // Check critical WRAM areas
  if (address >= 0x7E0000 && address < 0x7E2000) {
    // Low WRAM has critical system variables
    if (address < 0x7E0100) {
      return false;  // Direct page and stack area
    }
  }

  return true;
}

std::string RomDebugAgent::DescribeMemoryLocation(uint32_t address) const {
  // Check cache first
  auto it = address_description_cache_.find(address);
  if (it != address_description_cache_.end()) {
    return it->second;
  }

  std::string description;

  // System areas
  if (address < 0x100) {
    description = "Direct Page";
  } else if (address >= 0x100 && address < 0x200) {
    description = "Stack";
  } else if (address >= GAME_MODE && address == GAME_MODE) {
    description = "Game Mode";
  } else if (address == SUBMODULE) {
    description = "Submodule";
  } else if (address == NMI_FLAG) {
    description = "NMI Flag";
  } else if (address == FRAME_COUNTER) {
    description = "Frame Counter";
  }
  // Player/Link
  else if (address == LINK_X_POS) {
    description = "Link X Position";
  } else if (address == LINK_Y_POS) {
    description = "Link Y Position";
  } else if (address == LINK_STATE) {
    description = "Link State";
  } else if (address == LINK_DIRECTION) {
    description = "Link Facing Direction";
  }
  // Sprites
  else if (address >= SPRITE_TABLE_START && address < SPRITE_TABLE_END) {
    uint32_t offset = address - SPRITE_TABLE_START;
    uint32_t sprite_num = offset / 0x10;
    description = absl::StrFormat("Sprite %d Data", sprite_num);
  }
  // OAM
  else if (address >= OAM_BUFFER && address <= OAM_BUFFER_END) {
    description = "OAM Buffer (sprite attributes)";
  }
  // DMA
  else if (address >= DMA0_CONTROL && address < DMA0_CONTROL + 0x80) {
    uint32_t channel = (address - DMA0_CONTROL) / 0x10;
    description = absl::StrFormat("DMA Channel %d", channel);
  } else if (address == DMA_ENABLE) {
    description = "DMA Enable Register";
  } else if (address == HDMA_ENABLE) {
    description = "HDMA Enable Register";
  }
  // PPU
  else if (address == PPU_INIDISP) {
    description = "Screen Display Register";
  } else if (address == PPU_BGMODE) {
    description = "BG Mode Register";
  } else if (address == PPU_CGADD) {
    description = "CGRAM Address";
  } else if (address == PPU_CGDATA) {
    description = "CGRAM Data";
  }
  // Audio
  else if (address >= APU_PORT0 && address <= APU_PORT3) {
    description = absl::StrFormat("APU Port %d", address - APU_PORT0);
  }
  // Save data
  else if (address >= SRAM_START && address <= SRAM_END) {
    if (address >= PLAYER_NAME && address < PLAYER_NAME + 6) {
      description = "Player Name";
    } else if (address == PLAYER_HEALTH) {
      description = "Player Current Health";
    } else if (address == PLAYER_MAX_HEALTH) {
      description = "Player Max Health";
    } else if (address >= INVENTORY_START && address < INVENTORY_START + 0x40) {
      description = absl::StrFormat("Inventory Slot %d", address - INVENTORY_START);
    } else {
      description = "Save Data";
    }
  }
  // ROM banks
  else if (address >= 0x008000 && address < 0x00FFFF) {
    description = absl::StrFormat("ROM Bank $%02X", (address >> 16) & 0xFF);
  } else if (address >= WRAM_START && address <= WRAM_END) {
    description = "WRAM";
  } else if (address >= 0x800000 && address < 0xC00000) {
    description = absl::StrFormat("Extended ROM Bank $%02X", (address >> 16) & 0xFF);
  } else {
    description = "Unknown";
  }

  // Cache the result
  address_description_cache_[address] = description;
  return description;
}

std::string RomDebugAgent::IdentifyDataType(uint32_t address) const {
  auto it = data_type_cache_.find(address);
  if (it != data_type_cache_.end()) {
    return it->second;
  }

  std::string type;

  if (address >= SPRITE_TABLE_START && address < SPRITE_TABLE_END) {
    type = "sprite";
  } else if (address >= OAM_BUFFER && address <= OAM_BUFFER_END) {
    type = "oam";
  } else if (address >= DMA0_CONTROL && address < DMA0_CONTROL + 0x80) {
    type = "dma";
  } else if (address >= 0x002100 && address <= 0x00213F) {
    type = "ppu";
  } else if (address >= APU_PORT0 && address <= APU_PORT3) {
    type = "audio";
  } else if (address >= SRAM_START && address <= SRAM_END) {
    type = "save";
  } else if (address >= INVENTORY_START && address < INVENTORY_START + 0x40) {
    type = "inventory";
  } else if (address >= 0x008000 && address < 0x7E0000) {
    type = "code";
  } else if (address >= WRAM_START && address <= WRAM_END) {
    type = "ram";
  } else {
    type = "unknown";
  }

  data_type_cache_[address] = type;
  return type;
}

std::string RomDebugAgent::FormatRegisterState(
    const std::map<std::string, uint16_t>& regs) const {
  std::stringstream ss;
  ss << "Registers: ";
  ss << absl::StrFormat("A=%04X ", regs.at("A"));
  ss << absl::StrFormat("X=%04X ", regs.at("X"));
  ss << absl::StrFormat("Y=%04X ", regs.at("Y"));
  ss << absl::StrFormat("S=%04X ", regs.at("S"));
  ss << absl::StrFormat("PC=%04X ", regs.at("PC"));
  ss << absl::StrFormat("P=%02X ", regs.at("P"));
  ss << absl::StrFormat("DB=%02X ", regs.at("DB"));
  ss << absl::StrFormat("PB=%02X", regs.at("PB"));
  return ss.str();
}

absl::Status RomDebugAgent::LoadSymbols(const std::string& symbol_file) {
  return symbol_provider_->LoadSymbolFile(symbol_file);
}

void RomDebugAgent::SetOriginalRom(const std::vector<uint8_t>& rom_data) {
  original_rom_ = rom_data;
}

// Private helper methods

absl::StatusOr<std::string> RomDebugAgent::AnalyzeInstruction(
    uint32_t address, const uint8_t* code, size_t max_length) {
  if (max_length == 0) {
    return absl::InvalidArgumentError("No code provided");
  }

  uint8_t opcode = code[0];
  std::string explanation;

  // Analyze based on opcode
  switch (opcode) {
    // Load instructions
    case 0xA9: explanation = "Load immediate value into accumulator"; break;
    case 0xA2: explanation = "Load immediate value into X register"; break;
    case 0xA0: explanation = "Load immediate value into Y register"; break;
    case 0xAD: explanation = "Load accumulator from absolute address"; break;
    case 0xAE: explanation = "Load X register from absolute address"; break;
    case 0xAC: explanation = "Load Y register from absolute address"; break;

    // Store instructions
    case 0x8D: explanation = "Store accumulator to absolute address"; break;
    case 0x8E: explanation = "Store X register to absolute address"; break;
    case 0x8C: explanation = "Store Y register to absolute address"; break;
    case 0x9D: explanation = "Store accumulator to address indexed by X"; break;
    case 0x99: explanation = "Store accumulator to address indexed by Y"; break;

    // Branches
    case 0x10: explanation = "Branch if plus (N flag clear)"; break;
    case 0x30: explanation = "Branch if minus (N flag set)"; break;
    case 0x50: explanation = "Branch if overflow clear"; break;
    case 0x70: explanation = "Branch if overflow set"; break;
    case 0x80: explanation = "Branch always"; break;
    case 0x90: explanation = "Branch if carry clear (less than)"; break;
    case 0xB0: explanation = "Branch if carry set (greater than or equal)"; break;
    case 0xD0: explanation = "Branch if not equal (Z flag clear)"; break;
    case 0xF0: explanation = "Branch if equal (Z flag set)"; break;

    // Jumps and calls
    case 0x20: explanation = "Call subroutine"; break;
    case 0x22: explanation = "Call long subroutine (24-bit)"; break;
    case 0x4C: explanation = "Jump to address"; break;
    case 0x5C: explanation = "Jump long (24-bit)"; break;
    case 0x60: explanation = "Return from subroutine"; break;
    case 0x6B: explanation = "Return from long subroutine"; break;

    // Stack operations
    case 0x48: explanation = "Push accumulator onto stack"; break;
    case 0x68: explanation = "Pull accumulator from stack"; break;
    case 0x08: explanation = "Push processor status onto stack"; break;
    case 0x28: explanation = "Pull processor status from stack"; break;

    // Arithmetic
    case 0x69: explanation = "Add to accumulator with carry"; break;
    case 0xE9: explanation = "Subtract from accumulator with borrow"; break;
    case 0xC9: explanation = "Compare accumulator with value"; break;
    case 0xE0: explanation = "Compare X register with value"; break;
    case 0xC0: explanation = "Compare Y register with value"; break;

    // Logical
    case 0x29: explanation = "AND accumulator with value"; break;
    case 0x09: explanation = "OR accumulator with value"; break;
    case 0x49: explanation = "XOR accumulator with value"; break;

    // Special
    case 0x00: explanation = "Software interrupt (BRK)"; break;
    case 0xEA: explanation = "No operation (NOP)"; break;
    case 0x18: explanation = "Clear carry flag"; break;
    case 0x38: explanation = "Set carry flag"; break;
    case 0xC2: explanation = "Clear processor flags (REP)"; break;
    case 0xE2: explanation = "Set processor flags (SEP)"; break;

    default:
      explanation = absl::StrFormat("Execute opcode $%02X", opcode);
  }

  return explanation;
}

std::vector<std::string> RomDebugAgent::GetDisassemblyContext(
    uint32_t address, int before_lines, int after_lines) {
  std::vector<std::string> context_lines;

  // Get disassembly from emulator service
  grpc::ServerContext ctx;
  DisassemblyRequest req;
  req.set_start_address(address - (before_lines * 3));  // Estimate 3 bytes per instruction
  req.set_count(before_lines + after_lines + 1);
  DisassemblyResponse resp;

  auto status = emulator_service_->GetDisassembly(&ctx, &req, &resp);
  if (!status.ok()) {
    return context_lines;
  }

  for (const auto& inst : resp.lines()) {
    std::string line = absl::StrFormat("%s: %s %s",
                                       FormatSnesAddress(inst.address()),
                                       inst.mnemonic(),
                                       inst.operand_str());
    if (inst.address() == address) {
      line = ">>> " + line + " <<<";  // Highlight current instruction
    }
    context_lines.push_back(line);
  }

  return context_lines;
}

std::vector<std::string> RomDebugAgent::BuildCallStack(uint32_t current_pc) {
  std::vector<std::string> stack;

  // Get execution trace to build call stack
  grpc::ServerContext ctx;
  yaze::agent::TraceRequest req;
  req.set_max_entries(100);  // Get last 100 instructions
  yaze::agent::TraceResponse resp;

  auto status = emulator_service_->GetExecutionTrace(&ctx, &req, &resp);
  if (!status.ok()) {
    return stack;
  }

  // Walk backwards through trace to find calls
  for (int i = resp.entries_size() - 1; i >= 0; --i) {
    const auto& entry = resp.entries(i);
    uint8_t opcode = entry.opcode();

    if (opcode == 0x20 || opcode == 0x22 || opcode == 0xFC) {
      // Found a call
      std::string caller = symbol_provider_->HasSymbols()
          ? symbol_provider_->FormatAddress(entry.address())
          : FormatSnesAddress(entry.address());
      stack.push_back(caller);
    } else if (IsReturn(opcode) && !stack.empty()) {
      // Found a return, pop from our reconstructed stack
      stack.pop_back();
    }
  }

  // Reverse to get top-down order
  std::reverse(stack.begin(), stack.end());

  return stack;
}

std::optional<RomDebugAgent::DetectedIssue> RomDebugAgent::DetectIssuePattern(
    uint32_t address, const uint8_t* code, size_t length) {
  if (length < 1) {
    return std::nullopt;
  }

  uint8_t opcode = code[0];

  // Check for BRK (usually an error)
  if (opcode == 0x00) {
    return DetectedIssue{
        IssueType::kInvalidOpcode,
        address,
        "BRK instruction found - usually indicates an error or unimplemented code",
        "Replace with proper implementation or NOP if intentional padding",
        4
    };
  }

  // Check for infinite loop (branch to self)
  if (opcode == 0x80 && length >= 2 && code[1] == 0xFE) {
    // BRA $-2 (branch to self)
    return DetectedIssue{
        IssueType::kInfiniteLoop,
        address,
        "Infinite loop detected (BRA to self)",
        "Add proper exit condition or loop counter",
        5
    };
  }

  // Check for writes to vector table
  if (opcode == 0x8D && length >= 3) {  // STA abs
    uint16_t dest = code[1] | (code[2] << 8);
    if (dest >= 0xFFE0) {
      return DetectedIssue{
          IssueType::kWramCorruption,
          address,
          absl::StrFormat("Writing to interrupt vector at $%04X", dest),
          "Verify this vector modification is intentional",
          5
      };
    }
  }

  // Check for stack operations without matching pairs
  if (ModifiesStack(opcode)) {
    // This would need more context to properly detect
    // For now, just flag excessive consecutive pushes
    int consecutive_pushes = 0;
    for (size_t i = 0; i < std::min(size_t(10), length); ++i) {
      if (code[i] == 0x48 || code[i] == 0x08 || code[i] == 0xDA || code[i] == 0x5A) {
        consecutive_pushes++;
      } else if (code[i] == 0x68 || code[i] == 0x28 || code[i] == 0xFA || code[i] == 0x7A) {
        consecutive_pushes--;
      }
    }

    if (consecutive_pushes > 5) {
      return DetectedIssue{
          IssueType::kStackImbalance,
          address,
          "Multiple consecutive pushes without pulls - possible stack overflow",
          "Verify stack operations are properly balanced",
          3
      };
    }
  }

  return std::nullopt;
}

bool RomDebugAgent::IsCriticalMemoryArea(uint32_t address) const {
  // Direct page and stack
  if (address < 0x200) {
    return true;
  }

  // Interrupt vectors
  if (address >= 0xFFE0 && address <= 0xFFFF) {
    return true;
  }

  // NMI handler area
  if (address == NMI_FLAG) {
    return true;
  }

  // DMA during critical timing
  if (address >= DMA0_CONTROL && address < DMA0_CONTROL + 0x80) {
    return true;
  }

  return false;
}

std::optional<std::string> RomDebugAgent::GetStructureInfo(uint32_t address) const {
  // Sprite structure
  if (address >= SPRITE_TABLE_START && address < SPRITE_TABLE_END) {
    uint32_t offset = (address - SPRITE_TABLE_START) % 0x10;
    switch (offset) {
      case 0x00: return "Sprite State";
      case 0x01: return "Sprite X Position Low";
      case 0x02: return "Sprite X Position High";
      case 0x03: return "Sprite Y Position Low";
      case 0x04: return "Sprite Y Position High";
      case 0x05: return "Sprite Z Position";
      case 0x06: return "Sprite Velocity X";
      case 0x07: return "Sprite Velocity Y";
      case 0x08: return "Sprite Type";
      case 0x09: return "Sprite Subtype";
      case 0x0A: return "Sprite Graphics";
      case 0x0B: return "Sprite Properties";
      case 0x0C: return "Sprite Health";
      case 0x0D: return "Sprite Damage";
      case 0x0E: return "Sprite Timer";
      case 0x0F: return "Sprite Flags";
    }
  }

  // OAM structure
  if (address >= OAM_BUFFER && address <= OAM_BUFFER_END) {
    uint32_t offset = (address - OAM_BUFFER) % 4;
    switch (offset) {
      case 0: return "OAM X Position";
      case 1: return "OAM Y Position";
      case 2: return "OAM Tile Number";
      case 3: return "OAM Attributes";
    }
  }

  // DMA channel structure
  if (address >= DMA0_CONTROL && address < DMA0_CONTROL + 0x80) {
    uint32_t offset = (address - DMA0_CONTROL) % 0x10;
    switch (offset) {
      case 0x00: return "DMA Control";
      case 0x01: return "DMA Destination";
      case 0x02: return "DMA Source Low";
      case 0x03: return "DMA Source High";
      case 0x04: return "DMA Source Bank";
      case 0x05: return "DMA Size Low";
      case 0x06: return "DMA Size High";
      case 0x07: return "DMA Indirect Bank";
    }
  }

  return std::nullopt;
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze