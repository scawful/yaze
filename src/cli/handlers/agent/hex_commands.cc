#include "cli/handlers/agent/hex_commands.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "app/rom.h"

namespace yaze {
namespace cli {
namespace agent {

namespace {

// Parse hex address from string (supports 0x prefix)
absl::StatusOr<uint32_t> ParseHexAddress(const std::string& str) {
  try {
    size_t pos;
    uint32_t addr = std::stoul(str, &pos, 16);
    if (pos != str.size()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid hex address: %s", str));
    }
    return addr;
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse address '%s': %s", str, e.what()));
  }
}

// Parse hex byte pattern (supports wildcards ??)
std::vector<std::pair<uint8_t, bool>> ParseHexPattern(const std::string& pattern) {
  std::vector<std::pair<uint8_t, bool>> result;
  std::vector<std::string> bytes = absl::StrSplit(pattern, ' ');
  
  for (const auto& byte_str : bytes) {
    if (byte_str.empty()) continue;
    
    if (byte_str == "??" || byte_str == "?") {
      result.push_back({0x00, false});  // Wildcard
    } else {
      try {
        uint8_t value = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        result.push_back({value, true});  // Exact match
      } catch (const std::exception&) {
        std::cerr << "Warning: Invalid byte in pattern: " << byte_str << std::endl;
      }
    }
  }
  
  return result;
}

// Format bytes as hex string
std::string FormatHexBytes(const uint8_t* data, size_t length,
                           const std::string& format) {
  std::ostringstream oss;
  
  if (format == "hex" || format == "both") {
    for (size_t i = 0; i < length; ++i) {
      oss << std::hex << std::setw(2) << std::setfill('0')
          << static_cast<int>(data[i]);
      if (i < length - 1) oss << " ";
    }
  }
  
  if (format == "both") {
    oss << "  |  ";
  }
  
  if (format == "ascii" || format == "both") {
    for (size_t i = 0; i < length; ++i) {
      char c = static_cast<char>(data[i]);
      oss << (std::isprint(c) ? c : '.');
    }
  }
  
  return oss.str();
}

}  // namespace

absl::Status HandleHexRead(const std::vector<std::string>& args,
                            Rom* rom_context) {
  if (!rom_context || !rom_context->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Parse arguments
  std::string address_str;
  int length = 16;
  std::string format = "both";
  
  for (const auto& arg : args) {
    if (arg.rfind("--address=", 0) == 0) {
      address_str = arg.substr(10);
    } else if (arg.rfind("--length=", 0) == 0) {
      length = std::stoi(arg.substr(9));
    } else if (arg.rfind("--format=", 0) == 0) {
      format = arg.substr(9);
    }
  }
  
  if (address_str.empty()) {
    return absl::InvalidArgumentError("--address required");
  }
  
  // Parse address
  auto addr_or = ParseHexAddress(address_str);
  if (!addr_or.ok()) {
    return addr_or.status();
  }
  uint32_t address = addr_or.value();
  
  // Validate range
  if (address + length > rom_context->size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Read beyond ROM: 0x%X+%d > %zu",
                       address, length, rom_context->size()));
  }
  
  // Read and format data
  const uint8_t* data = rom_context->data() + address;
  std::string formatted = FormatHexBytes(data, length, format);
  
  // Output
  std::cout << absl::StrFormat("Address 0x%06X [%d bytes]:\n", address, length);
  std::cout << formatted << std::endl;
  
  return absl::OkStatus();
}

absl::Status HandleHexWrite(const std::vector<std::string>& args,
                             Rom* rom_context) {
  if (!rom_context || !rom_context->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Parse arguments
  std::string address_str;
  std::string data_str;
  
  for (const auto& arg : args) {
    if (arg.rfind("--address=", 0) == 0) {
      address_str = arg.substr(10);
    } else if (arg.rfind("--data=", 0) == 0) {
      data_str = arg.substr(7);
    }
  }
  
  if (address_str.empty() || data_str.empty()) {
    return absl::InvalidArgumentError("--address and --data required");
  }
  
  // Parse address
  auto addr_or = ParseHexAddress(address_str);
  if (!addr_or.ok()) {
    return addr_or.status();
  }
  uint32_t address = addr_or.value();
  
  // Parse data bytes
  std::vector<std::string> byte_strs = absl::StrSplit(data_str, ' ');
  std::vector<uint8_t> bytes;
  
  for (const auto& byte_str : byte_strs) {
    if (byte_str.empty()) continue;
    try {
      uint8_t value = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
      bytes.push_back(value);
    } catch (const std::exception& e) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid byte '%s': %s", byte_str, e.what()));
    }
  }
  
  if (bytes.empty()) {
    return absl::InvalidArgumentError("No valid bytes to write");
  }
  
  // Validate range
  if (address + bytes.size() > rom_context->size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Write beyond ROM: 0x%X+%zu > %zu",
                       address, bytes.size(), rom_context->size()));
  }
  
  // Write data
  for (size_t i = 0; i < bytes.size(); ++i) {
    rom_context->WriteByte(address + i, bytes[i]);
  }
  
  // Output confirmation
  std::cout << absl::StrFormat("✓ Wrote %zu bytes to address 0x%06X\n",
                               bytes.size(), address);
  std::cout << "  Data: " << FormatHexBytes(bytes.data(), bytes.size(), "hex")
            << std::endl;
  
  // Note: In a full implementation, this would create a proposal
  std::cout << "Note: Changes written directly to ROM (proposal system TBD)\n";
  
  return absl::OkStatus();
}

absl::Status HandleHexSearch(const std::vector<std::string>& args,
                              Rom* rom_context) {
  if (!rom_context || !rom_context->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Parse arguments
  std::string pattern_str;
  uint32_t start_address = 0;
  uint32_t end_address = rom_context->size();
  
  for (const auto& arg : args) {
    if (arg.rfind("--pattern=", 0) == 0) {
      pattern_str = arg.substr(10);
    } else if (arg.rfind("--start=", 0) == 0) {
      auto addr_or = ParseHexAddress(arg.substr(8));
      if (addr_or.ok()) {
        start_address = addr_or.value();
      }
    } else if (arg.rfind("--end=", 0) == 0) {
      auto addr_or = ParseHexAddress(arg.substr(6));
      if (addr_or.ok()) {
        end_address = addr_or.value();
      }
    }
  }
  
  if (pattern_str.empty()) {
    return absl::InvalidArgumentError("--pattern required");
  }
  
  // Parse pattern
  auto pattern = ParseHexPattern(pattern_str);
  if (pattern.empty()) {
    return absl::InvalidArgumentError("Empty or invalid pattern");
  }
  
  // Search for pattern
  std::vector<uint32_t> matches;
  const uint8_t* rom_data = rom_context->data();
  
  for (uint32_t i = start_address; i <= end_address - pattern.size(); ++i) {
    bool match = true;
    for (size_t j = 0; j < pattern.size(); ++j) {
      if (pattern[j].second &&  // If not wildcard
          rom_data[i + j] != pattern[j].first) {
        match = false;
        break;
      }
    }
    if (match) {
      matches.push_back(i);
    }
  }
  
  // Output results
  std::cout << absl::StrFormat("Pattern: %s\n", pattern_str);
  std::cout << absl::StrFormat("Search range: 0x%06X - 0x%06X\n",
                               start_address, end_address);
  std::cout << absl::StrFormat("Found %zu match(es):\n", matches.size());
  
  for (size_t i = 0; i < matches.size() && i < 100; ++i) {  // Limit output
    uint32_t addr = matches[i];
    std::string context = FormatHexBytes(rom_data + addr, pattern.size(), "hex");
    std::cout << absl::StrFormat("  0x%06X: %s\n", addr, context);
  }
  
  if (matches.size() > 100) {
    std::cout << absl::StrFormat("  ... and %zu more matches\n",
                                 matches.size() - 100);
  }
  
  return absl::OkStatus();
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
