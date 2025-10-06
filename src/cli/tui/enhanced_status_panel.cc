#include "cli/tui/enhanced_status_panel.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

namespace yaze {
namespace cli {

using namespace ftxui;

EnhancedStatusPanel::EnhancedStatusPanel(Rom* rom_context) 
    : rom_context_(rom_context) {
  // Initialize status data
  CollectRomInfo();
  CollectSystemInfo();
  CollectLayoutInfo();
  
  // Create components
  rom_info_section_ = CreateRomInfoSection();
  system_info_section_ = CreateSystemInfoSection();
  layout_info_section_ = CreateLayoutInfoSection();
  error_section_ = CreateErrorSection();
  status_container_ = CreateStatusContainer();
  
  // Set up event handlers
  status_event_handler_ = [this](Event event) {
    return HandleStatusEvents(event);
  };
}

Component EnhancedStatusPanel::GetComponent() {
  return status_container_;
}

void EnhancedStatusPanel::SetRomContext(Rom* rom_context) {
  rom_context_ = rom_context;
  UpdateRomInfo();
}

void EnhancedStatusPanel::UpdateRomInfo() {
  CollectRomInfo();
}

void EnhancedStatusPanel::UpdateSystemInfo() {
  CollectSystemInfo();
}

void EnhancedStatusPanel::UpdateLayoutInfo() {
  CollectLayoutInfo();
}

void EnhancedStatusPanel::SetError(const std::string& error) {
  current_error_ = error;
}

void EnhancedStatusPanel::ClearError() {
  current_error_.clear();
}

Component EnhancedStatusPanel::CreateStatusContainer() {
  auto container = Container::Vertical({
    rom_info_section_,
    system_info_section_,
    layout_info_section_,
    error_section_
  });
  
  return Renderer(container, [this] {
    return vbox({
      text("📊 Status Panel") | bold | center,
      separator(),
      rom_info_section_->Render(),
      separator(),
      system_info_section_->Render(),
      separator(),
      layout_info_section_->Render(),
      separator(),
      error_section_->Render(),
      separator(),
      RenderStatusBar()
    }) | border;
  });
}

Component EnhancedStatusPanel::CreateRomInfoSection() {
  return Renderer([this] {
    return RenderRomInfo();
  });
}

Component EnhancedStatusPanel::CreateSystemInfoSection() {
  return Renderer([this] {
    return RenderSystemInfo();
  });
}

Component EnhancedStatusPanel::CreateLayoutInfoSection() {
  return Renderer([this] {
    return RenderLayoutInfo();
  });
}

Component EnhancedStatusPanel::CreateErrorSection() {
  return Renderer([this] {
    return RenderErrorInfo();
  });
}

bool EnhancedStatusPanel::HandleStatusEvents(const Event& event) {
  if (event == Event::Character('e')) {
    SetExpanded(!expanded_);
    return true;
  }
  
  if (event == Event::Character('d')) {
    SetShowDetailedInfo(!show_detailed_info_);
    return true;
  }
  
  return false;
}

Element EnhancedStatusPanel::RenderRomInfo() {
  if (!rom_info_.loaded) {
    return vbox({
      text("🎮 ROM Information") | bold | color(Color::Red),
      text("No ROM loaded") | color(Color::Red),
      text("Load a ROM to see information") | dim
    });
  }
  
  return vbox({
    text("🎮 ROM Information") | bold | color(Color::Green),
    text(absl::StrFormat("Title: %s", rom_info_.title)) | color(Color::White),
    text(absl::StrFormat("Size: %s", rom_info_.size)) | color(Color::Cyan),
    text(absl::StrFormat("Checksum: %s", rom_info_.checksum)) | color(Color::Yellow),
    text(absl::StrFormat("Region: %s", rom_info_.region)) | color(Color::Magenta)
  });
}

Element EnhancedStatusPanel::RenderSystemInfo() {
  return vbox({
    text("💻 System Information") | bold | color(Color::Blue),
    text(absl::StrFormat("Time: %s", system_info_.current_time)) | color(Color::White),
    text(absl::StrFormat("Memory: %s", system_info_.memory_usage)) | color(Color::Cyan),
    text(absl::StrFormat("CPU: %s", system_info_.cpu_usage)) | color(Color::Yellow),
    text(absl::StrFormat("Disk: %s", system_info_.disk_space)) | color(Color::Green)
  });
}

Element EnhancedStatusPanel::RenderLayoutInfo() {
  return vbox({
    text("🖥️ Layout Information") | bold | color(Color::Magenta),
    text(absl::StrFormat("Active Panel: %s", layout_info_.active_panel)) | color(Color::White),
    text(absl::StrFormat("Panel Count: %s", layout_info_.panel_count)) | color(Color::Cyan),
    text(absl::StrFormat("Layout Mode: %s", layout_info_.layout_mode)) | color(Color::Yellow),
    text(absl::StrFormat("Focus State: %s", layout_info_.focus_state)) | color(Color::Green)
  });
}

Element EnhancedStatusPanel::RenderErrorInfo() {
  if (current_error_.empty()) {
    return vbox({
      text("✅ No Errors") | color(Color::Green)
    });
  }
  
  return vbox({
    text("⚠️ Error") | bold | color(Color::Red),
    text(current_error_) | color(Color::Yellow)
  });
}

Element EnhancedStatusPanel::RenderStatusBar() {
  return hbox({
    text("e: Expand | d: Details | q: Quit") | dim,
    filler(),
    text(expanded_ ? "EXPANDED" : "COLLAPSED") | color(Color::Cyan),
    filler(),
    text(show_detailed_info_ ? "DETAILED" : "SIMPLE") | color(Color::Yellow)
  });
}

void EnhancedStatusPanel::CollectRomInfo() {
  if (!rom_context_) {
    rom_info_.loaded = false;
    rom_info_.title = "No ROM";
    rom_info_.size = "0 bytes";
    rom_info_.checksum = "N/A";
    rom_info_.region = "N/A";
    return;
  }
  
  rom_info_.loaded = true;
  rom_info_.title = rom_context_->title();
  rom_info_.size = absl::StrFormat("%d bytes", rom_context_->size());
  
  // Calculate checksum (simplified)
  uint32_t checksum = 0;
  if (rom_context_->size() > 0) {
    for (size_t i = 0; i < std::min(rom_context_->size(), size_t(1024)); ++i) {
      checksum += rom_context_->vector()[i];
    }
  }
  rom_info_.checksum = absl::StrFormat("0x%08X", checksum);
  
  // Determine region (simplified)
  if (rom_context_->size() > 0) {
    uint8_t region_byte = rom_context_->vector()[0x7FD9]; // SNES region byte
    switch (region_byte) {
      case 0x00: rom_info_.region = "NTSC"; break;
      case 0x01: rom_info_.region = "PAL"; break;
      default: rom_info_.region = "Unknown"; break;
    }
  } else {
    rom_info_.region = "Unknown";
  }
}

void EnhancedStatusPanel::CollectSystemInfo() {
  // Get current time
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto tm = *std::localtime(&time_t);
  
  std::stringstream ss;
  ss << std::put_time(&tm, "%H:%M:%S");
  system_info_.current_time = ss.str();
  
  // Placeholder system info (in a real implementation, you'd query the system)
  system_info_.memory_usage = "Unknown";
  system_info_.cpu_usage = "Unknown";
  system_info_.disk_space = "Unknown";
}

void EnhancedStatusPanel::CollectLayoutInfo() {
  // Placeholder layout info (in a real implementation, you'd get this from the layout manager)
  layout_info_.active_panel = "Main Menu";
  layout_info_.panel_count = "4";
  layout_info_.layout_mode = "Unified";
  layout_info_.focus_state = "Chat";
}

}  // namespace cli
}  // namespace yaze
