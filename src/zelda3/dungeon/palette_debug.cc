#include "zelda3/dungeon/palette_debug.h"

#include "app/platform/sdl_compat.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <map>
#include <sstream>

#include "absl/strings/str_format.h"
#include "util/log.h"

namespace {
uint64_t GetCurrentTimeMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
}  // namespace

namespace yaze::zelda3 {

PaletteDebugger& PaletteDebugger::Get() {
  static PaletteDebugger instance;
  return instance;
}

void PaletteDebugger::LogPaletteLoad(const std::string& location,
                                     int palette_id,
                                     const gfx::SnesPalette& palette) {
  PaletteDebugEvent event;
  event.location = location;
  event.palette_id = palette_id;
  event.color_count = palette.size();
  event.level = PaletteDebugLevel::INFO;
  event.timestamp_ms = GetCurrentTimeMs();
  event.sequence_number = sequence_counter_++;

  // Sample first 3 colors
  for (size_t i = 0; i < std::min(size_t(3), palette.size()); i++) {
    auto rgb = palette[i].rgb();
    event.sample_colors.push_back(static_cast<uint8_t>(rgb.x));
    event.sample_colors.push_back(static_cast<uint8_t>(rgb.y));
    event.sample_colors.push_back(static_cast<uint8_t>(rgb.z));
  }

  std::string sample_str;
  if (event.sample_colors.size() >= 3) {
    sample_str = absl::StrFormat("[Sample: R=%d G=%d B=%d]",
                                 event.sample_colors[0], event.sample_colors[1],
                                 event.sample_colors[2]);
  }

  event.message = absl::StrFormat("Loaded palette %d with %d colors %s",
                                  palette_id, event.color_count, sample_str);

  AddEvent(event);
  LOG_INFO("PaletteDebug", "%s", event.message);
}

void PaletteDebugger::LogPaletteApplication(const std::string& location,
                                            int palette_id, bool success,
                                            const std::string& reason) {
  PaletteDebugEvent event;
  event.location = location;
  event.palette_id = palette_id;
  event.level = success ? PaletteDebugLevel::INFO : PaletteDebugLevel::ERROR;
  event.timestamp_ms = GetCurrentTimeMs();
  event.sequence_number = sequence_counter_++;
  event.message =
      success ? absl::StrFormat("Applied palette %d successfully", palette_id)
              : absl::StrFormat("Failed to apply palette %d: %s", palette_id,
                                reason);

  AddEvent(event);
  if (success) {
    LOG_INFO("PaletteDebug", "%s", event.message);
  } else {
    LOG_ERROR("PaletteDebug", "%s", event.message);
  }
}

void PaletteDebugger::LogTextureCreation(const std::string& location,
                                         bool has_palette, int color_count) {
  PaletteDebugEvent event;
  event.location = location;
  event.color_count = color_count;
  event.level =
      has_palette ? PaletteDebugLevel::INFO : PaletteDebugLevel::WARNING;
  event.timestamp_ms = GetCurrentTimeMs();
  event.sequence_number = sequence_counter_++;
  event.message =
      has_palette
          ? absl::StrFormat("Creating texture with %d-color palette",
                            color_count)
          : "WARNING: Creating texture WITHOUT palette - will use default "
            "colors!";

  AddEvent(event);
  if (!has_palette) {
    LOG_WARN("PaletteDebug", "%s", event.message);
  }
}

void PaletteDebugger::LogSurfaceState(const std::string& location,
                                      SDL_Surface* surface) {
  PaletteDebugEvent event;
  event.location = location;
  event.timestamp_ms = GetCurrentTimeMs();
  event.sequence_number = sequence_counter_++;

  if (!surface) {
    event.level = PaletteDebugLevel::ERROR;
    event.message = "Surface is NULL!";
    AddEvent(event);
    LOG_ERROR("PaletteDebug", "%s", event.message);
    return;
  }

  Uint32 fmt = platform::GetSurfaceFormat(surface);
  if (fmt == SDL_PIXELFORMAT_UNKNOWN) {
    event.level = PaletteDebugLevel::ERROR;
    event.message = "Surface format is unknown!";
    AddEvent(event);
    LOG_ERROR("PaletteDebug", "%s", event.message);
    return;
  }

  bool is_indexed = (fmt == SDL_PIXELFORMAT_INDEX8);
  SDL_Palette* palette = platform::GetSurfacePalette(surface);
  bool has_palette = (palette != nullptr);
  int ncolors = has_palette ? palette->ncolors : 0;

  event.color_count = ncolors;

  if (!is_indexed) {
    event.level = PaletteDebugLevel::WARNING;
    event.message = absl::StrFormat(
        "Surface is NOT indexed (format=0x%08X). Palette will be ignored!",
        fmt);
  } else if (!has_palette) {
    event.level = PaletteDebugLevel::WARNING;
    event.message = "Surface is indexed but has NO palette attached!";
  } else if (ncolors < 90) {
    event.level = PaletteDebugLevel::WARNING;
    event.message = absl::StrFormat(
        "Surface has palette with only %d colors (expected 90)", ncolors);
  } else {
    event.level = PaletteDebugLevel::INFO;
    event.message = absl::StrFormat(
        "Surface OK: indexed format, %d-color palette attached", ncolors);

    // Sample color 56 (palette 7, offset 0) for verification
    if (palette && ncolors > 56) {
      auto& c = palette->colors[56];
      event.sample_colors = {c.r, c.g, c.b};
      event.message += absl::StrFormat(" [Color56: R=%d G=%d B=%d]", c.r, c.g,
                                       c.b);
    }
  }

  AddEvent(event);
  LOG_INFO("PaletteDebug", "%s: %s", location, event.message);
}

void PaletteDebugger::SetCurrentPalette(const gfx::SnesPalette& palette) {
  current_palette_ = palette;
}

void PaletteDebugger::SetCurrentBitmap(gfx::Bitmap* bitmap) {
  current_bitmap_ = bitmap;
}

ColorComparison PaletteDebugger::SamplePixelAt(int x, int y) const {
  ColorComparison comp;
  comp.x = x;
  comp.y = y;
  comp.palette_index = 0;
  comp.actual_r = comp.actual_g = comp.actual_b = 0;
  comp.expected_r = comp.expected_g = comp.expected_b = 0;
  comp.matches = false;

  if (!current_bitmap_ || !current_bitmap_->surface()) {
    return comp;
  }

  auto* surface = current_bitmap_->surface();
  int width = current_bitmap_->width();
  int height = current_bitmap_->height();

  if (x < 0 || x >= width || y < 0 || y >= height) {
    return comp;
  }

  // Get palette index from bitmap data
  const uint8_t* data = current_bitmap_->data();
  size_t data_size = current_bitmap_->size();
  if (!data || data_size == 0) {
    return comp;
  }

  size_t idx = static_cast<size_t>(y * width + x);
  if (idx >= data_size) {
    return comp;
  }

  comp.palette_index = data[idx];

  // Get expected color from our stored palette
  if (comp.palette_index < current_palette_.size()) {
    auto rgb = current_palette_[comp.palette_index].rgb();
    comp.expected_r = static_cast<uint8_t>(rgb.x);
    comp.expected_g = static_cast<uint8_t>(rgb.y);
    comp.expected_b = static_cast<uint8_t>(rgb.z);
  }

  // Get actual color from SDL surface palette
  SDL_Palette* palette = platform::GetSurfacePalette(surface);
  if (palette && comp.palette_index < palette->ncolors) {
    auto& c = palette->colors[comp.palette_index];
    comp.actual_r = c.r;
    comp.actual_g = c.g;
    comp.actual_b = c.b;
  }

  // Check if they match
  comp.matches = (comp.actual_r == comp.expected_r &&
                  comp.actual_g == comp.expected_g &&
                  comp.actual_b == comp.expected_b);

  return comp;
}

uint32_t PaletteDebugger::ComputePaletteChecksum(
    const gfx::SnesPalette& palette) const {
  uint32_t sum = 0;
  for (size_t i = 0; i < palette.size(); i++) {
    auto rgb = palette[i].rgb();
    sum += static_cast<uint32_t>(rgb.x) * (i + 1);
    sum += static_cast<uint32_t>(rgb.y) * (i + 1) * 256;
    sum += static_cast<uint32_t>(rgb.z) * (i + 1) * 65536;
  }
  return sum;
}

#ifdef __EMSCRIPTEN__
std::string PaletteDebugger::ExportToJSON() const {
  std::ostringstream json;
  json << "[";
  for (size_t i = 0; i < events_.size(); i++) {
    const auto& e = events_[i];
    json << "{";
    json << "\"location\":\"" << e.location << "\",";
    json << "\"message\":\"" << e.message << "\",";
    json << "\"level\":\""
         << (e.level == PaletteDebugLevel::ERROR      ? "error"
             : e.level == PaletteDebugLevel::WARNING ? "warning"
                                                      : "info")
         << "\",";
    json << "\"palette_id\":" << e.palette_id << ",";
    json << "\"color_count\":" << e.color_count;

    // Include sample colors if available
    if (!e.sample_colors.empty() && e.sample_colors.size() >= 3) {
      json << ",\"sample_rgb\":[" << (int)e.sample_colors[0] << ","
           << (int)e.sample_colors[1] << "," << (int)e.sample_colors[2] << "]";
    }

    json << "}";
    if (i < events_.size() - 1) json << ",";
  }
  json << "]";
  return json.str();
}

std::string PaletteDebugger::ExportColorComparisonsJSON() const {
  std::ostringstream json;
  json << "[";
  for (size_t i = 0; i < comparisons_.size(); i++) {
    const auto& c = comparisons_[i];
    json << "{";
    json << "\"x\":" << c.x << ",\"y\":" << c.y << ",";
    json << "\"palette_index\":" << (int)c.palette_index << ",";
    json << "\"actual\":[" << (int)c.actual_r << "," << (int)c.actual_g << ","
         << (int)c.actual_b << "],";
    json << "\"expected\":[" << (int)c.expected_r << "," << (int)c.expected_g
         << "," << (int)c.expected_b << "],";
    json << "\"matches\":" << (c.matches ? "true" : "false");
    json << "}";
    if (i < comparisons_.size() - 1) json << ",";
  }
  json << "]";
  return json.str();
}

std::string PaletteDebugger::SamplePixelJSON(int x, int y) const {
  auto comp = SamplePixelAt(x, y);
  std::ostringstream json;
  json << "{";
  json << "\"x\":" << comp.x << ",\"y\":" << comp.y << ",";
  json << "\"palette_index\":" << (int)comp.palette_index << ",";
  json << "\"actual\":[" << (int)comp.actual_r << "," << (int)comp.actual_g
       << "," << (int)comp.actual_b << "],";
  json << "\"expected\":[" << (int)comp.expected_r << ","
       << (int)comp.expected_g << "," << (int)comp.expected_b << "],";
  json << "\"matches\":" << (comp.matches ? "true" : "false");
  json << "}";
  return json.str();
}

std::string PaletteDebugger::ExportFullStateJSON() const {
  std::ostringstream json;
  json << "{";

  // Events timeline
  json << "\"events\":" << ExportToJSON() << ",";

  // Color comparisons
  json << "\"comparisons\":" << ExportColorComparisonsJSON() << ",";

  // Current palette data
  json << "\"palette\":" << ExportPaletteDataJSON() << ",";

  // Timeline analysis
  json << "\"timeline\":" << ExportTimelineJSON() << ",";

  // Diagnostic summary
  json << "\"diagnostic\":\"" << GetDiagnosticSummary() << "\",";

  // Hypothesis analysis
  json << "\"hypothesis\":\"" << GetHypothesisAnalysis() << "\"";

  json << "}";
  return json.str();
}

std::string PaletteDebugger::ExportPaletteDataJSON() const {
  std::ostringstream json;
  json << "{";
  json << "\"size\":" << current_palette_.size() << ",";
  json << "\"checksum\":" << ComputePaletteChecksum(current_palette_) << ",";
  json << "\"colors\":[";

  for (size_t i = 0; i < current_palette_.size(); i++) {
    auto rgb = current_palette_[i].rgb();
    json << "{\"index\":" << i << ",\"r\":" << (int)rgb.x << ",\"g\":"
         << (int)rgb.y << ",\"b\":" << (int)rgb.z << "}";
    if (i < current_palette_.size() - 1) json << ",";
  }

  json << "]}";
  return json.str();
}

std::string PaletteDebugger::ExportTimelineJSON() const {
  std::ostringstream json;
  json << "{";

  // Find first and last timestamps
  uint64_t first_ts = 0, last_ts = 0;
  if (!events_.empty()) {
    first_ts = events_[0].timestamp_ms;
    last_ts = events_[events_.size() - 1].timestamp_ms;
  }

  json << "\"start_ms\":" << first_ts << ",";
  json << "\"end_ms\":" << last_ts << ",";
  json << "\"duration_ms\":" << (last_ts - first_ts) << ",";
  json << "\"event_count\":" << events_.size() << ",";

  // Group events by location
  json << "\"by_location\":{";
  std::map<std::string, int> location_counts;
  for (const auto& e : events_) {
    location_counts[e.location]++;
  }
  bool first = true;
  for (const auto& [loc, count] : location_counts) {
    if (!first) json << ",";
    json << "\"" << loc << "\":" << count;
    first = false;
  }
  json << "},";

  // Count by level
  int info_count = 0, warn_count = 0, error_count = 0;
  for (const auto& e : events_) {
    if (e.level == PaletteDebugLevel::INFO)
      info_count++;
    else if (e.level == PaletteDebugLevel::WARNING)
      warn_count++;
    else
      error_count++;
  }
  json << "\"info_count\":" << info_count << ",";
  json << "\"warning_count\":" << warn_count << ",";
  json << "\"error_count\":" << error_count;

  json << "}";
  return json.str();
}

std::string PaletteDebugger::GetDiagnosticSummary() const {
  std::ostringstream summary;

  // Count warnings and errors
  int warnings = 0, errors = 0;
  bool has_palette_timing_issue = false;
  bool has_missing_palette = false;
  bool has_color_mismatch = false;

  for (const auto& e : events_) {
    if (e.level == PaletteDebugLevel::WARNING) warnings++;
    if (e.level == PaletteDebugLevel::ERROR) errors++;

    // Detect specific issues
    if (e.message.find("WITHOUT palette") != std::string::npos) {
      has_missing_palette = true;
    }
    if (e.message.find("only") != std::string::npos &&
        e.message.find("colors") != std::string::npos) {
      has_palette_timing_issue = true;
    }
  }

  // Check color comparisons for mismatches
  for (const auto& c : comparisons_) {
    if (!c.matches) {
      has_color_mismatch = true;
      break;
    }
  }

  summary << "Events: " << events_.size() << ", Warnings: " << warnings
          << ", Errors: " << errors << ". ";

  if (has_missing_palette) {
    summary << "ISSUE: Texture created without palette. ";
  }
  if (has_palette_timing_issue) {
    summary << "ISSUE: Palette may not be fully loaded. ";
  }
  if (has_color_mismatch) {
    summary << "ISSUE: Color mismatch detected between expected and actual. ";
  }
  if (!has_missing_palette && !has_palette_timing_issue && !has_color_mismatch) {
    summary << "No obvious issues detected. ";
  }

  return summary.str();
}

std::string PaletteDebugger::GetHypothesisAnalysis() const {
  std::ostringstream analysis;

  // Analyze events for the timing hypothesis (75% confidence from plan)
  bool texture_before_palette = false;
  int last_palette_load_seq = -1;
  int first_texture_create_seq = INT_MAX;

  for (const auto& e : events_) {
    if (e.message.find("Loaded palette") != std::string::npos) {
      last_palette_load_seq =
          std::max(last_palette_load_seq, e.sequence_number);
    }
    if (e.message.find("Creating texture") != std::string::npos) {
      first_texture_create_seq =
          std::min(first_texture_create_seq, e.sequence_number);
    }
  }

  if (first_texture_create_seq < last_palette_load_seq) {
    texture_before_palette = true;
    analysis << "TIMING HYPOTHESIS CONFIRMED: Texture created (seq "
             << first_texture_create_seq << ") before palette loaded (seq "
             << last_palette_load_seq << "). ";
    analysis << "FIX: Ensure palette is applied to surface BEFORE queuing "
                "texture creation. ";
  } else if (last_palette_load_seq >= 0 && first_texture_create_seq < INT_MAX) {
    analysis << "Timing appears correct: palette loaded (seq "
             << last_palette_load_seq << ") before texture created (seq "
             << first_texture_create_seq << "). ";
  }

  // Check for palette group selection hypothesis (60% confidence)
  bool wrong_palette_group = false;
  for (const auto& e : events_) {
    if (e.color_count > 0 && e.color_count < 90) {
      wrong_palette_group = true;
      analysis << "PALETTE GROUP ISSUE: Only " << e.color_count
               << " colors (expected 90). Check palette group selection. ";
      break;
    }
  }

  // Check for surface format mismatch (40% confidence)
  for (const auto& e : events_) {
    if (e.message.find("NOT indexed") != std::string::npos) {
      analysis << "FORMAT MISMATCH: Surface is not indexed, palette ignored. ";
      break;
    }
  }

  if (analysis.str().empty()) {
    analysis << "No hypothesis conditions detected. Render pipeline may be "
                "correct. Check actual colors vs expected in comparisons. ";
  }

  return analysis.str();
}
#endif

}  // namespace yaze::zelda3
