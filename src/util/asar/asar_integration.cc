#include "util/asar/asar_integration.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include <fstream>
#include <sstream>

namespace yaze {
namespace util {
namespace asar {

AsarIntegration::AsarIntegration() : initialized_(false) {
}

AsarIntegration::~AsarIntegration() {
  if (initialized_) {
    asar_reset();
  }
}

absl::Status AsarIntegration::Initialize() {
  // Check API version compatibility
  int api_version = asar_apiversion();
  if (api_version < 303) {
    return absl::InternalError(
        absl::StrFormat("Incompatible Asar API version: %d (expected >= 303)", api_version));
  }

  // Reset Asar to clean state
  RETURN_IF_ERROR(ResetAsar());
  
  initialized_ = true;
  return absl::OkStatus();
}

absl::Status AsarIntegration::ResetAsar() {
  if (!asar_reset()) {
    return absl::InternalError("Failed to reset Asar");
  }
  return absl::OkStatus();
}

absl::StatusOr<PatchResult> AsarIntegration::PatchRom(
    const std::string& patch_file,
    const std::vector<uint8_t>& rom_data,
    const std::vector<std::string>& include_paths,
    const std::unordered_map<std::string, std::string>& defines) {
  
  if (!initialized_) {
    return absl::FailedPreconditionError("Asar not initialized");
  }

  // Reset Asar state
  RETURN_IF_ERROR(ResetAsar());

  // Prepare ROM data for patching
  current_rom_data_ = rom_data;
  int rom_len = static_cast<int>(rom_data.size());
  int buf_len = asar_maxromsize(); // Get maximum possible ROM size
  
  // Ensure buffer is large enough
  if (buf_len > static_cast<int>(current_rom_data_.size())) {
    current_rom_data_.resize(buf_len);
  }

  // Apply the patch
  bool success = asar_patch(patch_file.c_str(), 
                           reinterpret_cast<char*>(current_rom_data_.data()),
                           buf_len, &rom_len);

  PatchResult result;
  result.success = success;
  result.rom_size_before = static_cast<int>(rom_data.size());
  result.rom_size_after = rom_len;
  
  // Get errors and warnings
  result.errors = GetErrorMessages();
  result.warnings = GetWarningMessages();
  
  // Get symbols if patch was successful
  if (success) {
    result.symbols = ConvertLabelsToSymbols();
    auto defines_result = ConvertDefinesToSymbols();
    result.symbols.insert(result.symbols.end(), defines_result.begin(), defines_result.end());
    
    // Get written blocks
    int block_count = 0;
    const writtenblockdata* blocks = asar_getwrittenblocks(&block_count);
    if (blocks) {
      result.written_blocks.assign(blocks, blocks + block_count);
    }
  }

  return result;
}

absl::StatusOr<std::vector<SymbolInfo>> AsarIntegration::ExtractSymbols(
    const std::string& patch_file,
    const std::vector<std::string>& include_paths) {
  
  if (!initialized_) {
    return absl::FailedPreconditionError("Asar not initialized");
  }

  // Create a dummy ROM for symbol extraction
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0); // 1MB dummy ROM
  
  // Apply patch to dummy ROM to extract symbols
  auto result = PatchRom(patch_file, dummy_rom, include_paths);
  if (!result.ok()) {
    return result.status();
  }

  return result->symbols;
}

std::vector<OpcodeInfo> AsarIntegration::Get65816Opcodes() {
  // 65816 opcodes with their addressing modes and sizes
  return {
    // Load/Store Instructions
    {"LDA", "immediate", 2, "Load Accumulator"},
    {"LDA", "direct", 2, "Load Accumulator"},
    {"LDA", "direct_indexed_x", 2, "Load Accumulator"},
    {"LDA", "direct_indexed_y", 2, "Load Accumulator"},
    {"LDA", "absolute", 3, "Load Accumulator"},
    {"LDA", "absolute_indexed_x", 3, "Load Accumulator"},
    {"LDA", "absolute_indexed_y", 3, "Load Accumulator"},
    {"LDA", "long", 4, "Load Accumulator"},
    {"LDA", "long_indexed_x", 4, "Load Accumulator"},
    {"LDA", "direct_indirect", 2, "Load Accumulator"},
    {"LDA", "direct_indirect_long", 2, "Load Accumulator"},
    {"LDA", "direct_indirect_indexed_y", 2, "Load Accumulator"},
    {"LDA", "direct_indirect_long_indexed_y", 2, "Load Accumulator"},
    {"LDA", "stack_relative", 2, "Load Accumulator"},
    {"LDA", "stack_relative_indirect_indexed_y", 2, "Load Accumulator"},
    
    {"LDX", "immediate", 2, "Load X Register"},
    {"LDX", "direct", 2, "Load X Register"},
    {"LDX", "direct_indexed_y", 2, "Load X Register"},
    {"LDX", "absolute", 3, "Load X Register"},
    {"LDX", "absolute_indexed_y", 3, "Load X Register"},
    
    {"LDY", "immediate", 2, "Load Y Register"},
    {"LDY", "direct", 2, "Load Y Register"},
    {"LDY", "direct_indexed_x", 2, "Load Y Register"},
    {"LDY", "absolute", 3, "Load Y Register"},
    {"LDY", "absolute_indexed_x", 3, "Load Y Register"},
    
    {"STA", "direct", 2, "Store Accumulator"},
    {"STA", "direct_indexed_x", 2, "Store Accumulator"},
    {"STA", "direct_indexed_y", 2, "Store Accumulator"},
    {"STA", "absolute", 3, "Store Accumulator"},
    {"STA", "absolute_indexed_x", 3, "Store Accumulator"},
    {"STA", "absolute_indexed_y", 3, "Store Accumulator"},
    {"STA", "long", 4, "Store Accumulator"},
    {"STA", "long_indexed_x", 4, "Store Accumulator"},
    {"STA", "direct_indirect", 2, "Store Accumulator"},
    {"STA", "direct_indirect_long", 2, "Store Accumulator"},
    {"STA", "direct_indirect_indexed_y", 2, "Store Accumulator"},
    {"STA", "direct_indirect_long_indexed_y", 2, "Store Accumulator"},
    {"STA", "stack_relative", 2, "Store Accumulator"},
    {"STA", "stack_relative_indirect_indexed_y", 2, "Store Accumulator"},
    
    {"STX", "direct", 2, "Store X Register"},
    {"STX", "direct_indexed_y", 2, "Store X Register"},
    {"STX", "absolute", 3, "Store X Register"},
    
    {"STY", "direct", 2, "Store Y Register"},
    {"STY", "direct_indexed_x", 2, "Store Y Register"},
    {"STY", "absolute", 3, "Store Y Register"},
    
    // Arithmetic Instructions
    {"ADC", "immediate", 2, "Add with Carry"},
    {"ADC", "direct", 2, "Add with Carry"},
    {"ADC", "direct_indexed_x", 2, "Add with Carry"},
    {"ADC", "absolute", 3, "Add with Carry"},
    {"ADC", "absolute_indexed_x", 3, "Add with Carry"},
    {"ADC", "absolute_indexed_y", 3, "Add with Carry"},
    {"ADC", "long", 4, "Add with Carry"},
    {"ADC", "long_indexed_x", 4, "Add with Carry"},
    {"ADC", "direct_indirect", 2, "Add with Carry"},
    {"ADC", "direct_indirect_long", 2, "Add with Carry"},
    {"ADC", "direct_indirect_indexed_y", 2, "Add with Carry"},
    {"ADC", "direct_indirect_long_indexed_y", 2, "Add with Carry"},
    {"ADC", "stack_relative", 2, "Add with Carry"},
    {"ADC", "stack_relative_indirect_indexed_y", 2, "Add with Carry"},
    
    {"SBC", "immediate", 2, "Subtract with Carry"},
    {"SBC", "direct", 2, "Subtract with Carry"},
    {"SBC", "direct_indexed_x", 2, "Subtract with Carry"},
    {"SBC", "absolute", 3, "Subtract with Carry"},
    {"SBC", "absolute_indexed_x", 3, "Subtract with Carry"},
    {"SBC", "absolute_indexed_y", 3, "Subtract with Carry"},
    {"SBC", "long", 4, "Subtract with Carry"},
    {"SBC", "long_indexed_x", 4, "Subtract with Carry"},
    {"SBC", "direct_indirect", 2, "Subtract with Carry"},
    {"SBC", "direct_indirect_long", 2, "Subtract with Carry"},
    {"SBC", "direct_indirect_indexed_y", 2, "Subtract with Carry"},
    {"SBC", "direct_indirect_long_indexed_y", 2, "Subtract with Carry"},
    {"SBC", "stack_relative", 2, "Subtract with Carry"},
    {"SBC", "stack_relative_indirect_indexed_y", 2, "Subtract with Carry"},
    
    // Logical Instructions
    {"AND", "immediate", 2, "Logical AND"},
    {"AND", "direct", 2, "Logical AND"},
    {"AND", "direct_indexed_x", 2, "Logical AND"},
    {"AND", "absolute", 3, "Logical AND"},
    {"AND", "absolute_indexed_x", 3, "Logical AND"},
    {"AND", "absolute_indexed_y", 3, "Logical AND"},
    {"AND", "long", 4, "Logical AND"},
    {"AND", "long_indexed_x", 4, "Logical AND"},
    {"AND", "direct_indirect", 2, "Logical AND"},
    {"AND", "direct_indirect_long", 2, "Logical AND"},
    {"AND", "direct_indirect_indexed_y", 2, "Logical AND"},
    {"AND", "direct_indirect_long_indexed_y", 2, "Logical AND"},
    {"AND", "stack_relative", 2, "Logical AND"},
    {"AND", "stack_relative_indirect_indexed_y", 2, "Logical AND"},
    
    {"ORA", "immediate", 2, "Logical OR"},
    {"ORA", "direct", 2, "Logical OR"},
    {"ORA", "direct_indexed_x", 2, "Logical OR"},
    {"ORA", "absolute", 3, "Logical OR"},
    {"ORA", "absolute_indexed_x", 3, "Logical OR"},
    {"ORA", "absolute_indexed_y", 3, "Logical OR"},
    {"ORA", "long", 4, "Logical OR"},
    {"ORA", "long_indexed_x", 4, "Logical OR"},
    {"ORA", "direct_indirect", 2, "Logical OR"},
    {"ORA", "direct_indirect_long", 2, "Logical OR"},
    {"ORA", "direct_indirect_indexed_y", 2, "Logical OR"},
    {"ORA", "direct_indirect_long_indexed_y", 2, "Logical OR"},
    {"ORA", "stack_relative", 2, "Logical OR"},
    {"ORA", "stack_relative_indirect_indexed_y", 2, "Logical OR"},
    
    {"EOR", "immediate", 2, "Exclusive OR"},
    {"EOR", "direct", 2, "Exclusive OR"},
    {"EOR", "direct_indexed_x", 2, "Exclusive OR"},
    {"EOR", "absolute", 3, "Exclusive OR"},
    {"EOR", "absolute_indexed_x", 3, "Exclusive OR"},
    {"EOR", "absolute_indexed_y", 3, "Exclusive OR"},
    {"EOR", "long", 4, "Exclusive OR"},
    {"EOR", "long_indexed_x", 4, "Exclusive OR"},
    {"EOR", "direct_indirect", 2, "Exclusive OR"},
    {"EOR", "direct_indirect_long", 2, "Exclusive OR"},
    {"EOR", "direct_indirect_indexed_y", 2, "Exclusive OR"},
    {"EOR", "direct_indirect_long_indexed_y", 2, "Exclusive OR"},
    {"EOR", "stack_relative", 2, "Exclusive OR"},
    {"EOR", "stack_relative_indirect_indexed_y", 2, "Exclusive OR"},
    
    // Branch Instructions
    {"BCC", "relative", 2, "Branch if Carry Clear"},
    {"BCS", "relative", 2, "Branch if Carry Set"},
    {"BEQ", "relative", 2, "Branch if Equal"},
    {"BMI", "relative", 2, "Branch if Minus"},
    {"BNE", "relative", 2, "Branch if Not Equal"},
    {"BPL", "relative", 2, "Branch if Plus"},
    {"BRA", "relative", 2, "Branch Always"},
    {"BVC", "relative", 2, "Branch if Overflow Clear"},
    {"BVS", "relative", 2, "Branch if Overflow Set"},
    
    // Jump Instructions
    {"JMP", "absolute", 3, "Jump"},
    {"JMP", "absolute_indirect", 3, "Jump"},
    {"JMP", "absolute_indirect_long", 3, "Jump"},
    {"JMP", "absolute_indexed_indirect", 3, "Jump"},
    {"JMP", "long", 4, "Jump"},
    
    {"JSR", "absolute", 3, "Jump to Subroutine"},
    {"JSR", "absolute_indexed_indirect", 3, "Jump to Subroutine"},
    {"JSR", "long", 4, "Jump to Subroutine"},
    
    {"RTS", "implied", 1, "Return from Subroutine"},
    {"RTL", "implied", 1, "Return from Subroutine Long"},
    
    // Stack Instructions
    {"PHA", "implied", 1, "Push Accumulator"},
    {"PHP", "implied", 1, "Push Processor Status"},
    {"PHX", "implied", 1, "Push X Register"},
    {"PHY", "implied", 1, "Push Y Register"},
    {"PLA", "implied", 1, "Pull Accumulator"},
    {"PLP", "implied", 1, "Pull Processor Status"},
    {"PLX", "implied", 1, "Pull X Register"},
    {"PLY", "implied", 1, "Pull Y Register"},
    
    // Processor Status Instructions
    {"CLC", "implied", 1, "Clear Carry"},
    {"CLD", "implied", 1, "Clear Decimal"},
    {"CLI", "implied", 1, "Clear Interrupt Disable"},
    {"CLV", "implied", 1, "Clear Overflow"},
    {"SEC", "implied", 1, "Set Carry"},
    {"SED", "implied", 1, "Set Decimal"},
    {"SEI", "implied", 1, "Set Interrupt Disable"},
    
    // Register Transfer Instructions
    {"TAX", "implied", 1, "Transfer Accumulator to X"},
    {"TAY", "implied", 1, "Transfer Accumulator to Y"},
    {"TSX", "implied", 1, "Transfer Stack Pointer to X"},
    {"TXA", "implied", 1, "Transfer X to Accumulator"},
    {"TXS", "implied", 1, "Transfer X to Stack Pointer"},
    {"TYA", "implied", 1, "Transfer Y to Accumulator"},
    
    // Increment/Decrement Instructions
    {"INC", "direct", 2, "Increment Memory"},
    {"INC", "direct_indexed_x", 2, "Increment Memory"},
    {"INC", "absolute", 3, "Increment Memory"},
    {"INC", "absolute_indexed_x", 3, "Increment Memory"},
    {"INX", "implied", 1, "Increment X Register"},
    {"INY", "implied", 1, "Increment Y Register"},
    
    {"DEC", "direct", 2, "Decrement Memory"},
    {"DEC", "direct_indexed_x", 2, "Decrement Memory"},
    {"DEC", "absolute", 3, "Decrement Memory"},
    {"DEC", "absolute_indexed_x", 3, "Decrement Memory"},
    {"DEX", "implied", 1, "Decrement X Register"},
    {"DEY", "implied", 1, "Decrement Y Register"},
    
    // Compare Instructions
    {"CMP", "immediate", 2, "Compare Accumulator"},
    {"CMP", "direct", 2, "Compare Accumulator"},
    {"CMP", "direct_indexed_x", 2, "Compare Accumulator"},
    {"CMP", "absolute", 3, "Compare Accumulator"},
    {"CMP", "absolute_indexed_x", 3, "Compare Accumulator"},
    {"CMP", "absolute_indexed_y", 3, "Compare Accumulator"},
    {"CMP", "long", 4, "Compare Accumulator"},
    {"CMP", "long_indexed_x", 4, "Compare Accumulator"},
    {"CMP", "direct_indirect", 2, "Compare Accumulator"},
    {"CMP", "direct_indirect_long", 2, "Compare Accumulator"},
    {"CMP", "direct_indirect_indexed_y", 2, "Compare Accumulator"},
    {"CMP", "direct_indirect_long_indexed_y", 2, "Compare Accumulator"},
    {"CMP", "stack_relative", 2, "Compare Accumulator"},
    {"CMP", "stack_relative_indirect_indexed_y", 2, "Compare Accumulator"},
    
    {"CPX", "immediate", 2, "Compare X Register"},
    {"CPX", "direct", 2, "Compare X Register"},
    {"CPX", "absolute", 3, "Compare X Register"},
    
    {"CPY", "immediate", 2, "Compare Y Register"},
    {"CPY", "direct", 2, "Compare Y Register"},
    {"CPY", "absolute", 3, "Compare Y Register"},
    
    // Bit Operations
    {"BIT", "direct", 2, "Test Bits"},
    {"BIT", "absolute", 3, "Test Bits"},
    {"BIT", "absolute_indexed_x", 3, "Test Bits"},
    {"BIT", "immediate", 2, "Test Bits"},
    
    // Shift Instructions
    {"ASL", "direct", 2, "Arithmetic Shift Left"},
    {"ASL", "direct_indexed_x", 2, "Arithmetic Shift Left"},
    {"ASL", "absolute", 3, "Arithmetic Shift Left"},
    {"ASL", "absolute_indexed_x", 3, "Arithmetic Shift Left"},
    {"ASL", "accumulator", 1, "Arithmetic Shift Left"},
    
    {"LSR", "direct", 2, "Logical Shift Right"},
    {"LSR", "direct_indexed_x", 2, "Logical Shift Right"},
    {"LSR", "absolute", 3, "Logical Shift Right"},
    {"LSR", "absolute_indexed_x", 3, "Logical Shift Right"},
    {"LSR", "accumulator", 1, "Logical Shift Right"},
    
    {"ROL", "direct", 2, "Rotate Left"},
    {"ROL", "direct_indexed_x", 2, "Rotate Left"},
    {"ROL", "absolute", 3, "Rotate Left"},
    {"ROL", "absolute_indexed_x", 3, "Rotate Left"},
    {"ROL", "accumulator", 1, "Rotate Left"},
    
    {"ROR", "direct", 2, "Rotate Right"},
    {"ROR", "direct_indexed_x", 2, "Rotate Right"},
    {"ROR", "absolute", 3, "Rotate Right"},
    {"ROR", "absolute_indexed_x", 3, "Rotate Right"},
    {"ROR", "accumulator", 1, "Rotate Right"},
    
    // Special Instructions
    {"BRK", "implied", 1, "Break"},
    {"COP", "immediate", 2, "Coprocessor"},
    {"NOP", "implied", 1, "No Operation"},
    {"RTI", "implied", 1, "Return from Interrupt"},
    {"WAI", "implied", 1, "Wait for Interrupt"},
    {"STP", "implied", 1, "Stop"},
    
    // 65816 Specific Instructions
    {"MVN", "block_move", 3, "Move Negative"},
    {"MVP", "block_move", 3, "Move Positive"},
    {"PEA", "immediate", 3, "Push Effective Address"},
    {"PEI", "direct_indirect", 2, "Push Effective Indirect Address"},
    {"PER", "relative", 3, "Push Effective Relative Address"},
    {"PHB", "implied", 1, "Push Data Bank Register"},
    {"PHD", "implied", 1, "Push Direct Page Register"},
    {"PHK", "implied", 1, "Push Program Bank Register"},
    {"PLB", "implied", 1, "Pull Data Bank Register"},
    {"PLD", "implied", 1, "Pull Direct Page Register"},
    {"REP", "immediate", 2, "Reset Processor Status Bits"},
    {"SEP", "immediate", 2, "Set Processor Status Bits"},
    {"STP", "implied", 1, "Stop Processor"},
    {"TCD", "implied", 1, "Transfer Accumulator to Direct Page Register"},
    {"TCS", "implied", 1, "Transfer Accumulator to Stack Pointer"},
    {"TDC", "implied", 1, "Transfer Direct Page Register to Accumulator"},
    {"TSC", "implied", 1, "Transfer Stack Pointer to Accumulator"},
    {"TXY", "implied", 1, "Transfer X to Y"},
    {"TYX", "implied", 1, "Transfer Y to X"},
    {"WDM", "immediate", 2, "Reserved for Future Expansion"},
    {"XBA", "implied", 1, "Exchange A and B"},
    {"XCE", "implied", 1, "Exchange Carry and Emulation"},
  };
}

absl::StatusOr<int> AsarIntegration::GetSymbolValue(const std::string& symbol_name) {
  if (!initialized_) {
    return absl::FailedPreconditionError("Asar not initialized");
  }
  
  int value = asar_getlabelval(symbol_name.c_str());
  if (value == -1) {
    return absl::NotFoundError(absl::StrFormat("Symbol '%s' not found", symbol_name));
  }
  
  return value;
}

absl::StatusOr<std::vector<SymbolInfo>> AsarIntegration::GetAllLabels() {
  if (!initialized_) {
    return absl::FailedPreconditionError("Asar not initialized");
  }
  
  return ConvertLabelsToSymbols();
}

absl::StatusOr<std::vector<SymbolInfo>> AsarIntegration::GetAllDefines() {
  if (!initialized_) {
    return absl::FailedPreconditionError("Asar not initialized");
  }
  
  return ConvertDefinesToSymbols();
}

absl::StatusOr<std::vector<writtenblockdata>> AsarIntegration::GetWrittenBlocks() {
  if (!initialized_) {
    return absl::FailedPreconditionError("Asar not initialized");
  }
  
  int block_count = 0;
  const writtenblockdata* blocks = asar_getwrittenblocks(&block_count);
  if (!blocks) {
    return absl::InternalError("Failed to get written blocks");
  }
  
  return std::vector<writtenblockdata>(blocks, blocks + block_count);
}

absl::StatusOr<std::string> AsarIntegration::GenerateSymbolsFile(const std::string& format) {
  if (!initialized_) {
    return absl::FailedPreconditionError("Asar not initialized");
  }
  
  const char* symbols_data = asar_getsymbolsfile(format.c_str());
  if (!symbols_data) {
    return absl::InternalError("Failed to generate symbols file");
  }
  
  return std::string(symbols_data);
}

std::string AsarIntegration::GetVersion() {
  int version = asar_version();
  int major = version / 10000;
  int minor = (version % 10000) / 100;
  int patch = version % 100;
  return absl::StrFormat("%d.%d.%d", major, minor, patch);
}

std::string AsarIntegration::GetApiVersion() {
  int api_version = asar_apiversion();
  int major = api_version / 100;
  int minor = api_version % 100;
  return absl::StrFormat("%d.%d", major, minor);
}

std::vector<SymbolInfo> AsarIntegration::ConvertLabelsToSymbols() {
  std::vector<SymbolInfo> symbols;
  
  int label_count = 0;
  const labeldata* labels = asar_getalllabels(&label_count);
  if (labels) {
    for (int i = 0; i < label_count; i++) {
      SymbolInfo symbol;
      symbol.name = labels[i].name;
      symbol.location = labels[i].location;
      symbol.type = "label";
      symbols.push_back(symbol);
    }
  }
  
  return symbols;
}

std::vector<SymbolInfo> AsarIntegration::ConvertDefinesToSymbols() {
  std::vector<SymbolInfo> symbols;
  
  int define_count = 0;
  const definedata* defines = asar_getalldefines(&define_count);
  if (defines) {
    for (int i = 0; i < define_count; i++) {
      SymbolInfo symbol;
      symbol.name = defines[i].name;
      symbol.value = defines[i].contents;
      symbol.type = "define";
      symbols.push_back(symbol);
    }
  }
  
  return symbols;
}

std::vector<std::string> AsarIntegration::GetErrorMessages() {
  std::vector<std::string> errors;
  
  int error_count = 0;
  const errordata* error_data = asar_geterrors(&error_count);
  if (error_data) {
    for (int i = 0; i < error_count; i++) {
      errors.push_back(error_data[i].fullerrdata);
    }
  }
  
  return errors;
}

std::vector<std::string> AsarIntegration::GetWarningMessages() {
  std::vector<std::string> warnings;
  
  int warning_count = 0;
  const errordata* warning_data = asar_getwarnings(&warning_count);
  if (warning_data) {
    for (int i = 0; i < warning_count; i++) {
      warnings.push_back(warning_data[i].fullerrdata);
    }
  }
  
  return warnings;
}

}  // namespace asar
}  // namespace util
}  // namespace yaze