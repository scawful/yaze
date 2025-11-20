#include "app/emu/debug/breakpoint_manager.h"

#include <algorithm>
#include "util/log.h"

namespace yaze {
namespace emu {

uint32_t BreakpointManager::AddBreakpoint(uint32_t address, Type type,
                                          CpuType cpu,
                                          const std::string& condition,
                                          const std::string& description) {
  Breakpoint bp;
  bp.id = next_id_++;
  bp.address = address;
  bp.type = type;
  bp.cpu = cpu;
  bp.enabled = true;
  bp.condition = condition;
  bp.hit_count = 0;
  bp.description =
      description.empty()
          ? (cpu == CpuType::CPU_65816 ? "CPU Breakpoint" : "SPC700 Breakpoint")
          : description;

  breakpoints_[bp.id] = bp;

  LOG_INFO("Breakpoint", "Added breakpoint #%d: %s at $%06X (type=%d, cpu=%d)",
           bp.id, bp.description.c_str(), address, static_cast<int>(type),
           static_cast<int>(cpu));

  return bp.id;
}

void BreakpointManager::RemoveBreakpoint(uint32_t id) {
  auto it = breakpoints_.find(id);
  if (it != breakpoints_.end()) {
    LOG_INFO("Breakpoint", "Removed breakpoint #%d", id);
    breakpoints_.erase(it);
  }
}

void BreakpointManager::SetEnabled(uint32_t id, bool enabled) {
  auto it = breakpoints_.find(id);
  if (it != breakpoints_.end()) {
    it->second.enabled = enabled;
    LOG_INFO("Breakpoint", "Breakpoint #%d %s", id,
             enabled ? "enabled" : "disabled");
  }
}

bool BreakpointManager::ShouldBreakOnExecute(uint32_t pc, CpuType cpu) {
  for (auto& [id, bp] : breakpoints_) {
    if (!bp.enabled || bp.cpu != cpu || bp.type != Type::EXECUTE) {
      continue;
    }

    if (bp.address == pc) {
      bp.hit_count++;
      last_hit_ = &bp;

      // Check condition if present
      if (!bp.condition.empty()) {
        if (!EvaluateCondition(bp.condition, pc, pc, 0)) {
          continue;  // Condition not met
        }
      }

      LOG_INFO("Breakpoint", "Hit breakpoint #%d at PC=$%06X (hits=%d)", id, pc,
               bp.hit_count);
      return true;
    }
  }
  return false;
}

bool BreakpointManager::ShouldBreakOnMemoryAccess(uint32_t address,
                                                  bool is_write, uint8_t value,
                                                  uint32_t pc) {
  for (auto& [id, bp] : breakpoints_) {
    if (!bp.enabled || bp.address != address) {
      continue;
    }

    // Check if this breakpoint applies to this access type
    bool applies = false;
    switch (bp.type) {
      case Type::READ:
        applies = !is_write;
        break;
      case Type::WRITE:
        applies = is_write;
        break;
      case Type::ACCESS:
        applies = true;
        break;
      default:
        continue;  // Not a memory breakpoint
    }

    if (applies) {
      bp.hit_count++;
      last_hit_ = &bp;

      // Check condition if present
      if (!bp.condition.empty()) {
        if (!EvaluateCondition(bp.condition, pc, address, value)) {
          continue;
        }
      }

      LOG_INFO(
          "Breakpoint",
          "Hit %s breakpoint #%d at $%06X (value=$%02X, PC=$%06X, hits=%d)",
          is_write ? "WRITE" : "READ", id, address, value, pc, bp.hit_count);
      return true;
    }
  }
  return false;
}

std::vector<BreakpointManager::Breakpoint>
BreakpointManager::GetAllBreakpoints() const {
  std::vector<Breakpoint> result;
  result.reserve(breakpoints_.size());
  for (const auto& [id, bp] : breakpoints_) {
    result.push_back(bp);
  }
  // Sort by ID for consistent ordering
  std::sort(
      result.begin(), result.end(),
      [](const Breakpoint& a, const Breakpoint& b) { return a.id < b.id; });
  return result;
}

std::vector<BreakpointManager::Breakpoint> BreakpointManager::GetBreakpoints(
    CpuType cpu) const {
  std::vector<Breakpoint> result;
  for (const auto& [id, bp] : breakpoints_) {
    if (bp.cpu == cpu) {
      result.push_back(bp);
    }
  }
  std::sort(
      result.begin(), result.end(),
      [](const Breakpoint& a, const Breakpoint& b) { return a.id < b.id; });
  return result;
}

void BreakpointManager::ClearAll() {
  LOG_INFO("Breakpoint", "Cleared all breakpoints (%zu total)",
           breakpoints_.size());
  breakpoints_.clear();
  last_hit_ = nullptr;
}

void BreakpointManager::ClearAll(CpuType cpu) {
  auto it = breakpoints_.begin();
  int cleared = 0;
  while (it != breakpoints_.end()) {
    if (it->second.cpu == cpu) {
      it = breakpoints_.erase(it);
      cleared++;
    } else {
      ++it;
    }
  }
  LOG_INFO("Breakpoint", "Cleared %d breakpoints for %s", cleared,
           cpu == CpuType::CPU_65816 ? "CPU" : "SPC700");
}

void BreakpointManager::ResetHitCounts() {
  for (auto& [id, bp] : breakpoints_) {
    bp.hit_count = 0;
  }
}

bool BreakpointManager::EvaluateCondition(const std::string& condition,
                                          uint32_t pc, uint32_t address,
                                          uint8_t value) {
  // Simple condition evaluation for now
  // Future: Could integrate Lua or expression parser

  if (condition.empty()) {
    return true;  // No condition = always true
  }

  // Support simple comparisons: "value > 10", "value == 0xFF", etc.
  // Format: "value OPERATOR number"

  // For now, just return true (conditions not implemented yet)
  // TODO: Implement proper expression evaluation
  return true;
}

}  // namespace emu
}  // namespace yaze
