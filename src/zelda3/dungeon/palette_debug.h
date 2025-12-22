#pragma once

#include <string>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"

struct SDL_Surface;

namespace yaze::zelda3 {

enum class PaletteDebugLevel {
  INFO,
  WARNING,
  ERROR
};

struct PaletteDebugEvent {
  std::string location;        // e.g., "Room::RenderRoomGraphics"
  std::string message;         // What happened
  PaletteDebugLevel level;
  int palette_id;              // Which palette
  int color_count;             // How many colors
  std::vector<uint8_t> sample_colors;  // RGB of first 3 colors for verification
  uint64_t timestamp_ms;       // Timestamp for timeline analysis
  int sequence_number;         // Order of events
};

// Color comparison for debugging
struct ColorComparison {
  int x, y;                    // Position sampled
  uint8_t palette_index;       // Palette index at position
  uint8_t actual_r, actual_g, actual_b;     // Actual rendered color
  uint8_t expected_r, expected_g, expected_b; // Expected from palette
  bool matches;                // Do they match?
};

class PaletteDebugger {
 public:
  // Maximum number of events to store (prevents memory exhaustion in WASM)
  static constexpr size_t kMaxEvents = 1000;
  static constexpr size_t kMaxComparisons = 500;

  static PaletteDebugger& Get();

  void LogPaletteLoad(const std::string& location, int palette_id,
                      const gfx::SnesPalette& palette);
  void LogPaletteApplication(const std::string& location, int palette_id,
                             bool success, const std::string& reason = "");
  void LogTextureCreation(const std::string& location,
                          bool has_palette, int color_count);
  void LogSurfaceState(const std::string& location, SDL_Surface* surface);

  // Palette data access for comparison
  void SetCurrentPalette(const gfx::SnesPalette& palette);
  void SetCurrentBitmap(gfx::Bitmap* bitmap);

  // Pixel sampling for debugging
  ColorComparison SamplePixelAt(int x, int y) const;
  std::vector<ColorComparison> GetColorComparisons() const { return comparisons_; }
  void AddComparison(const ColorComparison& comp) { AddComparisonLimited(comp); }
  void ClearComparisons() { comparisons_.clear(); }

  // Compute checksum for palette integrity verification
  uint32_t ComputePaletteChecksum(const gfx::SnesPalette& palette) const;

  const std::vector<PaletteDebugEvent>& GetEvents() const { return events_; }
  void Clear() { events_.clear(); comparisons_.clear(); }

  // WASM exports
#ifdef __EMSCRIPTEN__
  std::string ExportToJSON() const;
  std::string ExportColorComparisonsJSON() const;
  std::string SamplePixelJSON(int x, int y) const;

  // Full state dump for AI analysis (Gemini/Antigravity integration)
  std::string ExportFullStateJSON() const;
  std::string ExportPaletteDataJSON() const;
  std::string ExportTimelineJSON() const;

  // Analysis helpers
  std::string GetDiagnosticSummary() const;
  std::string GetHypothesisAnalysis() const;
#endif

 private:
  PaletteDebugger() = default;

  // Helper to add events with size limit enforcement
  void AddEvent(const PaletteDebugEvent& event) {
    if (events_.size() >= kMaxEvents) {
      // Remove oldest half when at limit (keeps recent events)
      events_.erase(events_.begin(), events_.begin() + kMaxEvents / 2);
    }
    events_.push_back(event);
  }

  // Helper to add comparisons with size limit enforcement
  void AddComparisonLimited(const ColorComparison& comp) {
    if (comparisons_.size() >= kMaxComparisons) {
      comparisons_.erase(comparisons_.begin(), comparisons_.begin() + kMaxComparisons / 2);
    }
    comparisons_.push_back(comp);
  }

  std::vector<PaletteDebugEvent> events_;
  std::vector<ColorComparison> comparisons_;
  gfx::SnesPalette current_palette_;
  gfx::Bitmap* current_bitmap_ = nullptr;
  int sequence_counter_ = 0;
};

}  // namespace yaze::zelda3
