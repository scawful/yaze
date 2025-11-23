#ifndef YAZE_APP_EMU_DEBUG_SYMBOL_PROVIDER_H_
#define YAZE_APP_EMU_DEBUG_SYMBOL_PROVIDER_H_

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace emu {
namespace debug {

/**
 * @brief Information about a symbol (label, constant, or address)
 */
struct Symbol {
  std::string name;         // Symbol name (e.g., "MainGameLoop", "Reset")
  uint32_t address;         // 24-bit SNES address
  std::string file;         // Source file (if known)
  int line = 0;             // Line number (if known)
  std::string comment;      // Optional comment or description
  bool is_local = false;    // True for local labels (starting with .)

  Symbol() = default;
  Symbol(const std::string& n, uint32_t addr)
      : name(n), address(addr) {}
  Symbol(const std::string& n, uint32_t addr, const std::string& f, int l)
      : name(n), address(addr), file(f), line(l) {}
};

/**
 * @brief Supported symbol file formats
 */
enum class SymbolFormat {
  kAuto,      // Auto-detect based on file extension/content
  kAsar,      // Asar-style .asm/.sym files (label: at address #_XXXXXX:)
  kWlaDx,     // WLA-DX .sym format (bank:address name)
  kMesen,     // Mesen .mlb format (address:name)
  kBsnes,     // bsnes .sym format (address name)
  kNo$snes,   // No$snes .sym format (bank:addr name)
};

/**
 * @brief Provider for symbol (label) resolution in disassembly
 *
 * This class manages symbol tables from multiple sources:
 * - Parsed ASM files (usdasm disassembly)
 * - Symbol files from various emulators/assemblers
 * - Asar patches (runtime symbols)
 *
 * AI agents use this to see meaningful label names instead of raw addresses
 * when debugging 65816 assembly code.
 *
 * Usage:
 *   SymbolProvider symbols;
 *   symbols.LoadAsarAsmFile("bank_00.asm");
 *   symbols.LoadAsarAsmFile("bank_01.asm");
 *
 *   auto name = symbols.GetSymbolName(0x008034);  // Returns "MainGameLoop"
 *   auto addr = symbols.FindSymbol("Reset");       // Returns Symbol at $008000
 */
class SymbolProvider {
 public:
  SymbolProvider() = default;

  /**
   * @brief Load symbols from an Asar-style ASM file (usdasm format)
   *
   * Parses labels like:
   *   MainGameLoop:
   *   #_008034: LDA.b $12
   *
   * @param path Path to .asm file
   * @return Status indicating success or failure
   */
  absl::Status LoadAsarAsmFile(const std::string& path);

  /**
   * @brief Load symbols from a directory of ASM files
   *
   * Scans for all bank_XX.asm files and loads them
   *
   * @param directory_path Path to directory containing ASM files
   * @return Status indicating success or failure
   */
  absl::Status LoadAsarAsmDirectory(const std::string& directory_path);

  /**
   * @brief Load symbols from a .sym file (various formats)
   *
   * @param path Path to symbol file
   * @param format Symbol file format (kAuto for auto-detect)
   * @return Status indicating success or failure
   */
  absl::Status LoadSymbolFile(const std::string& path,
                               SymbolFormat format = SymbolFormat::kAuto);

  /**
   * @brief Add a single symbol manually
   */
  void AddSymbol(const Symbol& symbol);

  /**
   * @brief Add symbols from Asar patch results
   */
  void AddAsarSymbols(const std::vector<Symbol>& symbols);

  /**
   * @brief Clear all loaded symbols
   */
  void Clear();

  /**
   * @brief Get symbol name for an address
   * @return Symbol name if found, empty string otherwise
   */
  std::string GetSymbolName(uint32_t address) const;

  /**
   * @brief Get full symbol info for an address
   * @return Symbol if found, nullopt otherwise
   */
  std::optional<Symbol> GetSymbol(uint32_t address) const;

  /**
   * @brief Get all symbols at an address (there may be multiple)
   */
  std::vector<Symbol> GetSymbolsAtAddress(uint32_t address) const;

  /**
   * @brief Find symbol by name
   * @return Symbol if found, nullopt otherwise
   */
  std::optional<Symbol> FindSymbol(const std::string& name) const;

  /**
   * @brief Find symbols matching a pattern (supports wildcards)
   * @param pattern Pattern with * as wildcard (e.g., "Module*", "*_Init")
   * @return Matching symbols
   */
  std::vector<Symbol> FindSymbolsMatching(const std::string& pattern) const;

  /**
   * @brief Get all symbols in an address range
   */
  std::vector<Symbol> GetSymbolsInRange(uint32_t start, uint32_t end) const;

  /**
   * @brief Get nearest symbol at or before an address
   *
   * Useful for showing "MainGameLoop+$10" style offsets
   */
  std::optional<Symbol> GetNearestSymbol(uint32_t address) const;

  /**
   * @brief Format an address with symbol info
   *
   * Returns formats like:
   *   "MainGameLoop" (exact match)
   *   "MainGameLoop+$10" (offset from nearest symbol)
   *   "$00804D" (no nearby symbol)
   */
  std::string FormatAddress(uint32_t address,
                            uint32_t max_offset = 0x100) const;

  /**
   * @brief Get total number of loaded symbols
   */
  size_t GetSymbolCount() const { return symbols_by_address_.size(); }

  /**
   * @brief Check if any symbols are loaded
   */
  bool HasSymbols() const { return !symbols_by_address_.empty(); }

  /**
   * @brief Create a symbol resolver function for the disassembler
   */
  std::function<std::string(uint32_t)> CreateResolver() const;

 private:
  // Parse different symbol file formats
  absl::Status ParseAsarAsmContent(const std::string& content,
                                    const std::string& filename);
  absl::Status ParseWlaDxSymFile(const std::string& content);
  absl::Status ParseMesenMlbFile(const std::string& content);
  absl::Status ParseBsnesSymFile(const std::string& content);

  // Detect format from file content
  SymbolFormat DetectFormat(const std::string& content,
                            const std::string& extension) const;

  // Primary storage: address -> symbols (may have multiple per address)
  std::multimap<uint32_t, Symbol> symbols_by_address_;

  // Secondary index: name -> symbol (for reverse lookup)
  std::map<std::string, Symbol> symbols_by_name_;
};

}  // namespace debug
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_DEBUG_SYMBOL_PROVIDER_H_
