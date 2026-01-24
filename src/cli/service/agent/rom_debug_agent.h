#ifndef YAZE_CLI_SERVICE_AGENT_ROM_DEBUG_AGENT_H_
#define YAZE_CLI_SERVICE_AGENT_ROM_DEBUG_AGENT_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/emu/debug/symbol_provider.h"
#include "app/emu/mesen/mesen_socket_client.h"
#include "cli/service/agent/disassembler_65816.h"
#include "app/service/emulator_service_impl.h"
#include "protos/emulator_service.grpc.pb.h"

namespace yaze {
namespace cli {
namespace agent {

/**
 * @brief ROM Debugging Agent for AI-assisted ROM hacking
 *
 * Provides intelligent analysis of ROM execution, breakpoints, memory state,
 * and ASM patches. Designed to help ROM hackers understand crashes, debug
 * patches, and analyze game behavior.
 *
 * Features:
 * - Breakpoint analysis with disassembly and register state
 * - Memory region analysis (sprites, tiles, game variables)
 * - ASM patch comparison and debugging
 * - Pattern detection for common ROM hacking issues
 * - Execution trace analysis
 */
class RomDebugAgent {
 public:
  /**
   * @brief Analysis result for a breakpoint hit
   */
  struct BreakpointAnalysis {
    uint32_t address;                        // Breakpoint address
    std::string location_description;        // Human-readable location (e.g., "MainGameLoop+$10")
    std::string disassembly;                 // Disassembled instruction
    std::string instruction_explanation;     // AI-friendly explanation of what the instruction does
    std::map<std::string, uint16_t> registers;  // Current register values
    std::vector<std::string> call_stack;     // Call stack leading to this point
    std::vector<std::string> context_lines;  // Surrounding disassembly for context
    std::vector<std::string> suggestions;    // Debugging suggestions
    std::string memory_context;              // Relevant memory state description
  };

  /**
   * @brief Analysis of a memory region
   */
  struct MemoryAnalysis {
    uint32_t address;                // Memory address
    size_t length;                   // Length of analyzed region
    std::string data_type;           // "sprite", "tile", "palette", "dma", "audio", etc.
    std::string structure_name;      // Specific structure name if known
    std::string description;         // Human-readable description
    std::vector<uint8_t> data;       // Raw data
    std::map<std::string, uint32_t> fields;  // Parsed fields (if structured data)
    std::vector<std::string> anomalies;      // Detected issues or unusual values
  };

  /**
   * @brief Results from patch comparison
   */
  struct PatchComparisonResult {
    uint32_t address;                   // Patch location
    size_t length;                      // Patch size
    std::vector<uint8_t> original_code;  // Original ROM code
    std::vector<uint8_t> patched_code;   // Patched code
    std::string original_disassembly;   // Disassembled original
    std::string patched_disassembly;    // Disassembled patch
    std::vector<std::string> differences;  // Key differences explained
    std::vector<std::string> potential_issues;  // Detected problems
    bool is_safe;                       // Whether patch appears safe
  };

  /**
   * @brief Common ROM hacking issue types
   */
  enum class IssueType {
    kBadJumpTarget,       // Jump to invalid address
    kStackImbalance,      // Stack pointer corruption
    kWramCorruption,      // Writing to critical WRAM areas
    kDmaConflict,         // DMA during wrong time
    kBankOverflow,        // Code/data exceeds bank boundary
    kInvalidOpcode,       // Executing data as code
    kInfiniteLoop,        // Detected infinite loop
    kNullPointer,         // Dereferencing zero page incorrectly
    kAudioDesync,         // SPC700 communication issue
    kPpuTimingViolation,  // Writing to PPU at wrong time
  };

  /**
   * @brief Detected issue in ROM execution
   */
  struct DetectedIssue {
    IssueType type;
    uint32_t address;
    std::string description;
    std::string suggested_fix;
    int severity;  // 1-5, 5 being most severe
  };

  // Constructor
  explicit RomDebugAgent(yaze::net::EmulatorServiceImpl* emulator_service);

  // --- Core Analysis Functions ---

  /**
   * @brief Analyze a breakpoint hit with full context
   */
  absl::StatusOr<BreakpointAnalysis> AnalyzeBreakpoint(
      const yaze::agent::BreakpointHitResponse& hit);

  /**
   * @brief Analyze a memory region and identify its purpose
   */
  absl::StatusOr<MemoryAnalysis> AnalyzeMemory(
      uint32_t address, size_t length);

  /**
   * @brief Analyze execution trace and explain program flow
   */
  absl::StatusOr<std::string> ExplainExecutionTrace(
      const std::vector<ExecutionTraceBuffer::TraceEntry>& trace);

  /**
   * @brief Compare original ROM code with patched code
   */
  absl::StatusOr<PatchComparisonResult> ComparePatch(
      uint32_t address, size_t length, const std::vector<uint8_t>& original);

  // --- Pattern Detection ---

  /**
   * @brief Scan for common ROM hacking issues in a code region
   */
  std::vector<DetectedIssue> ScanForIssues(
      uint32_t start_address, uint32_t end_address);

  /**
   * @brief Check if an address is a valid jump target
   */
  bool IsValidJumpTarget(uint32_t address) const;

  /**
   * @brief Detect stack imbalance in a subroutine
   */
  bool HasStackImbalance(uint32_t routine_start, uint32_t routine_end);

  /**
   * @brief Check if memory write is safe
   */
  bool IsMemoryWriteSafe(uint32_t address, size_t length) const;

  // --- Helper Functions ---

  /**
   * @brief Get human-readable description of a memory address
   */
  std::string DescribeMemoryLocation(uint32_t address) const;

  /**
   * @brief Get the data type at a memory address
   */
  std::string IdentifyDataType(uint32_t address) const;

  /**
   * @brief Format register state for debugging output
   */
  std::string FormatRegisterState(const std::map<std::string, uint16_t>& regs) const;

  /**
   * @brief Load symbol table for better disassembly
   */
  absl::Status LoadSymbols(const std::string& symbol_file);

  /**
   * @brief Set the original ROM data for comparison
   */
  void SetOriginalRom(const std::vector<uint8_t>& rom_data);

  // --- Mesen2 Live Debugging Integration ---

  /**
   * @brief Set the Mesen2 socket client for live debugging
   */
  void SetMesenClient(std::shared_ptr<emu::mesen::MesenSocketClient> client);

  /**
   * @brief Check if connected to Mesen2
   */
  bool IsMesenConnected() const;

  /**
   * @brief Get live game state from Mesen2
   */
  absl::StatusOr<emu::mesen::GameState> GetLiveGameState();

  /**
   * @brief Get live sprite list from Mesen2
   */
  absl::StatusOr<std::vector<emu::mesen::SpriteInfo>> GetLiveSprites(bool all = false);

  /**
   * @brief Get live CPU state from Mesen2
   */
  absl::StatusOr<emu::mesen::CpuState> GetLiveCpuState();

  /**
   * @brief Analyze a live breakpoint hit from Mesen2
   */
  absl::StatusOr<BreakpointAnalysis> AnalyzeLiveBreakpoint(uint32_t address);

  /**
   * @brief Generate AI-friendly explanation of current game state
   */
  absl::StatusOr<std::string> ExplainCurrentGameState();

  /**
   * @brief Detect anomalies in current sprite table
   */
  std::vector<std::string> AnalyzeSpriteAnomalies();

 private:
  // --- ALTTP Memory Layout Constants ---

  // WRAM regions ($7E0000-$7FFFFF)
  static constexpr uint32_t WRAM_START = 0x7E0000;
  static constexpr uint32_t WRAM_END = 0x7FFFFF;

  // System variables
  static constexpr uint32_t GAME_MODE = 0x7E0010;
  static constexpr uint32_t SUBMODULE = 0x7E0011;
  static constexpr uint32_t NMI_FLAG = 0x7E0012;
  static constexpr uint32_t FRAME_COUNTER = 0x7E001A;

  // Player/Link state
  static constexpr uint32_t LINK_X_POS = 0x7E0022;
  static constexpr uint32_t LINK_Y_POS = 0x7E0020;
  static constexpr uint32_t LINK_STATE = 0x7E005D;
  static constexpr uint32_t LINK_DIRECTION = 0x7E002F;

  // Sprite tables
  static constexpr uint32_t SPRITE_TABLE_START = 0x7E0D00;
  static constexpr uint32_t SPRITE_TABLE_END = 0x7E0FFF;
  static constexpr uint32_t SPRITE_STATE = 0x7E0D10;
  static constexpr uint32_t SPRITE_X_LOW = 0x7E0D30;
  static constexpr uint32_t SPRITE_X_HIGH = 0x7E0D20;
  static constexpr uint32_t SPRITE_Y_LOW = 0x7E0D00;
  static constexpr uint32_t SPRITE_Y_HIGH = 0x7E0D20;

  // OAM (Object Attribute Memory) buffer
  static constexpr uint32_t OAM_BUFFER = 0x7E0800;
  static constexpr uint32_t OAM_BUFFER_END = 0x7E0A1F;

  // DMA registers
  static constexpr uint32_t DMA0_CONTROL = 0x004300;
  static constexpr uint32_t DMA_ENABLE = 0x00420B;
  static constexpr uint32_t HDMA_ENABLE = 0x00420C;

  // PPU registers
  static constexpr uint32_t PPU_INIDISP = 0x002100;
  static constexpr uint32_t PPU_BGMODE = 0x002105;
  static constexpr uint32_t PPU_CGADD = 0x002121;
  static constexpr uint32_t PPU_CGDATA = 0x002122;

  // Audio communication
  static constexpr uint32_t APU_PORT0 = 0x002140;
  static constexpr uint32_t APU_PORT1 = 0x002141;
  static constexpr uint32_t APU_PORT2 = 0x002142;
  static constexpr uint32_t APU_PORT3 = 0x002143;

  // Save data
  static constexpr uint32_t SRAM_START = 0x7EF000;
  static constexpr uint32_t SRAM_END = 0x7EF4FF;
  static constexpr uint32_t PLAYER_NAME = 0x7EF000;
  static constexpr uint32_t PLAYER_HEALTH = 0x7EF36D;
  static constexpr uint32_t PLAYER_MAX_HEALTH = 0x7EF36C;
  static constexpr uint32_t INVENTORY_START = 0x7EF340;

  // --- Helper Methods ---

  /**
   * @brief Analyze the instruction at an address
   */
  absl::StatusOr<std::string> AnalyzeInstruction(
      uint32_t address, const uint8_t* code, size_t max_length);

  /**
   * @brief Get surrounding context for an address
   */
  std::vector<std::string> GetDisassemblyContext(
      uint32_t address, int before_lines, int after_lines);

  /**
   * @brief Build call stack from execution trace
   */
  std::vector<std::string> BuildCallStack(uint32_t current_pc);

  /**
   * @brief Detect pattern of common issues
   */
  std::optional<DetectedIssue> DetectIssuePattern(
      uint32_t address, const uint8_t* code, size_t length);

  /**
   * @brief Check if address is in a critical system area
   */
  bool IsCriticalMemoryArea(uint32_t address) const;

  /**
   * @brief Get structure information for a memory address
   */
  std::optional<std::string> GetStructureInfo(uint32_t address) const;

  // Member variables
  yaze::net::EmulatorServiceImpl* emulator_service_;  // Non-owning pointer
  std::unique_ptr<Disassembler65816> disassembler_;
  std::unique_ptr<yaze::emu::debug::SymbolProvider> symbol_provider_;
  std::vector<uint8_t> original_rom_;  // Original ROM for comparison
  std::shared_ptr<emu::mesen::MesenSocketClient> mesen_client_;  // Mesen2 integration

  // Cache for performance
  mutable std::map<uint32_t, std::string> address_description_cache_;
  mutable std::map<uint32_t, std::string> data_type_cache_;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AGENT_ROM_DEBUG_AGENT_H_
