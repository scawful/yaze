# ImPlot Integration and Visualization Ideas

This repository now ships a thin wrapper for ImPlot under `yaze::gui::plotting`
(`src/app/gui/plots/implot_support.*`). The helpers:
- Ensure the ImPlot context exists (`EnsureImPlotContext()`).
- Apply theme-aware styling (`PlotStyleScope`, `BuildStyleFromTheme`).
- Wrap `BeginPlot`/`EndPlot` with RAII (`PlotGuard`).

Example usage:
```c++
using namespace yaze::gui::plotting;

PlotStyleScope plot_style(ThemeManager::Get().GetCurrentTheme());
PlotConfig config{.id = "Tile Usage", .flags = ImPlotFlags_NoLegend};
PlotGuard plot(config);
if (plot) {
  ImPlot::PlotHistogram("tiles", tile_hist.data(), tile_hist.size());
}
```

Candidate ROM-hacking visualizations worth adding:
- Tile and palette analytics: histograms of tile IDs per region; stacked bars of palette usage; scatter of tile index vs. frequency to find unused art.
- VRAM/CHR timelines: line plots of VRAM writes or DMA burst sizes during playback; overlays comparing two builds to spot regressions.
- Memory watch dashboards: line plots of key WRAM variables (health, rupees, mode flags) with markers for room transitions or boss flags.
- RNG/logic inspection: scatter of RNG seed vs. outcome (drops, patterns); step plots of RNG state across frames to debug determinism.
- Audio/SPC insight: per-channel volume over time; simple spectrum snapshots before/after a music tweak.
- Compression/asset diffs: bar charts of bank sizes and free space; plot of LZ chunk sizes to catch anomalies.
- Overworld/dungeon tuning: line plots of enemy counts, chest density, or item rarity per region; cumulative difficulty curves across progression.
- Performance profiling: stacked bars for frame time breakdowns (render vs. emu vs. scripting); list of slowest frames with hover tooltips describing scene state.
- Script/event timelines: Gantt-like plots for cutscene steps or event triggers, with hover showing script IDs and offsets.
