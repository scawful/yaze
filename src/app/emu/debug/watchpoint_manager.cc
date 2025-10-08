#include "app/emu/debug/watchpoint_manager.h"

#include <fstream>
#include <algorithm>
#include "absl/strings/str_format.h"
#include "util/log.h"

namespace yaze {
namespace emu {

uint32_t WatchpointManager::AddWatchpoint(uint32_t start_address, uint32_t end_address,
                                          bool track_reads, bool track_writes,
                                          bool break_on_access,
                                          const std::string& description) {
  Watchpoint wp;
  wp.id = next_id_++;
  wp.start_address = start_address;
  wp.end_address = end_address;
  wp.track_reads = track_reads;
  wp.track_writes = track_writes;
  wp.break_on_access = break_on_access;
  wp.enabled = true;
  wp.description = description.empty() 
      ? absl::StrFormat("Watch $%06X-$%06X", start_address, end_address)
      : description;
  
  watchpoints_[wp.id] = wp;
  
  LOG_INFO("Watchpoint", "Added watchpoint #%d: %s (R=%d, W=%d, Break=%d)",
           wp.id, wp.description.c_str(), track_reads, track_writes, break_on_access);
  
  return wp.id;
}

void WatchpointManager::RemoveWatchpoint(uint32_t id) {
  auto it = watchpoints_.find(id);
  if (it != watchpoints_.end()) {
    LOG_INFO("Watchpoint", "Removed watchpoint #%d", id);
    watchpoints_.erase(it);
  }
}

void WatchpointManager::SetEnabled(uint32_t id, bool enabled) {
  auto it = watchpoints_.find(id);
  if (it != watchpoints_.end()) {
    it->second.enabled = enabled;
    LOG_INFO("Watchpoint", "Watchpoint #%d %s", id, enabled ? "enabled" : "disabled");
  }
}

bool WatchpointManager::OnMemoryAccess(uint32_t pc, uint32_t address, bool is_write,
                                       uint8_t old_value, uint8_t new_value, 
                                       uint64_t cycle_count) {
  bool should_break = false;
  
  for (auto& [id, wp] : watchpoints_) {
    if (!wp.enabled || !IsInRange(wp, address)) {
      continue;
    }
    
    // Check if this access type is tracked
    bool should_log = (is_write && wp.track_writes) || (!is_write && wp.track_reads);
    if (!should_log) {
      continue;
    }
    
    // Log the access
    AccessLog log;
    log.pc = pc;
    log.address = address;
    log.old_value = old_value;
    log.new_value = new_value;
    log.is_write = is_write;
    log.cycle_count = cycle_count;
    log.description = absl::StrFormat("%s at $%06X: $%02X -> $%02X (PC=$%06X)",
                                     is_write ? "WRITE" : "READ",
                                     address, old_value, new_value, pc);
    
    wp.history.push_back(log);
    
    // Limit history size
    if (wp.history.size() > Watchpoint::kMaxHistorySize) {
      wp.history.pop_front();
    }
    
    // Check if should break
    if (wp.break_on_access) {
      should_break = true;
      LOG_INFO("Watchpoint", "Hit watchpoint #%d: %s", id, log.description.c_str());
    }
  }
  
  return should_break;
}

std::vector<WatchpointManager::Watchpoint> WatchpointManager::GetAllWatchpoints() const {
  std::vector<Watchpoint> result;
  result.reserve(watchpoints_.size());
  for (const auto& [id, wp] : watchpoints_) {
    result.push_back(wp);
  }
  std::sort(result.begin(), result.end(),
            [](const Watchpoint& a, const Watchpoint& b) { return a.id < b.id; });
  return result;
}

std::vector<WatchpointManager::AccessLog> WatchpointManager::GetHistory(
    uint32_t address, int max_entries) const {
  std::vector<AccessLog> result;
  
  for (const auto& [id, wp] : watchpoints_) {
    if (IsInRange(wp, address)) {
      for (const auto& log : wp.history) {
        if (log.address == address) {
          result.push_back(log);
          if (result.size() >= static_cast<size_t>(max_entries)) {
            break;
          }
        }
      }
    }
  }
  
  return result;
}

void WatchpointManager::ClearAll() {
  LOG_INFO("Watchpoint", "Cleared all watchpoints (%zu total)", watchpoints_.size());
  watchpoints_.clear();
}

void WatchpointManager::ClearHistory() {
  for (auto& [id, wp] : watchpoints_) {
    wp.history.clear();
  }
  LOG_INFO("Watchpoint", "Cleared all watchpoint history");
}

bool WatchpointManager::ExportHistoryToCSV(const std::string& filepath) const {
  std::ofstream out(filepath);
  if (!out.is_open()) {
    return false;
  }
  
  // CSV Header
  out << "Watchpoint,PC,Address,Type,OldValue,NewValue,Cycle,Description\n";
  
  for (const auto& [id, wp] : watchpoints_) {
    for (const auto& log : wp.history) {
      out << absl::StrFormat("%d,$%06X,$%06X,%s,$%02X,$%02X,%llu,\"%s\"\n",
                            id, log.pc, log.address,
                            log.is_write ? "WRITE" : "READ",
                            log.old_value, log.new_value, log.cycle_count,
                            log.description);
    }
  }
  
  out.close();
  LOG_INFO("Watchpoint", "Exported watchpoint history to %s", filepath.c_str());
  return true;
}

}  // namespace emu
}  // namespace yaze

