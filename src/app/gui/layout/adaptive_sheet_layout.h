#ifndef YAZE_APP_GUI_LAYOUT_ADAPTIVE_SHEET_LAYOUT_H_
#define YAZE_APP_GUI_LAYOUT_ADAPTIVE_SHEET_LAYOUT_H_

#include <algorithm>
#include <cmath>
#include <string_view>

namespace yaze::gui {

enum class AdaptiveSheetScaleMode {
  kFit,
  k1x,
  k2x,
  k4x,
};

inline std::string_view AdaptiveSheetScaleModeLabel(
    AdaptiveSheetScaleMode mode) {
  switch (mode) {
    case AdaptiveSheetScaleMode::k1x:
      return "1x";
    case AdaptiveSheetScaleMode::k2x:
      return "2x";
    case AdaptiveSheetScaleMode::k4x:
      return "4x";
    case AdaptiveSheetScaleMode::kFit:
    default:
      return "Fit";
  }
}

inline float AdaptiveSheetFixedScale(AdaptiveSheetScaleMode mode) {
  switch (mode) {
    case AdaptiveSheetScaleMode::k1x:
      return 1.0f;
    case AdaptiveSheetScaleMode::k2x:
      return 2.0f;
    case AdaptiveSheetScaleMode::k4x:
      return 4.0f;
    case AdaptiveSheetScaleMode::kFit:
    default:
      return 0.0f;
  }
}

struct AdaptiveSheetLayout {
  float display_scale = 1.0f;
  float displayed_width = 0.0f;
  float displayed_height = 0.0f;
  float usable_width = 0.0f;
};

inline AdaptiveSheetLayout ResolveAdaptiveSheetLayout(
    float available_width, int source_width, int source_height,
    AdaptiveSheetScaleMode mode = AdaptiveSheetScaleMode::kFit,
    float min_scale = 0.35f, float max_scale = 4.0f,
    float horizontal_chrome = 24.0f) {
  min_scale = std::max(0.01f, min_scale);
  max_scale = std::max(min_scale, max_scale);
  const float usable_width =
      std::max(1.0f, available_width - std::max(0.0f, horizontal_chrome));

  float scale = AdaptiveSheetFixedScale(mode);
  if (mode == AdaptiveSheetScaleMode::kFit) {
    scale = source_width > 0 ? usable_width / static_cast<float>(source_width)
                             : min_scale;
  }
  scale = std::clamp(scale, min_scale, max_scale);

  return {
      .display_scale = scale,
      .displayed_width =
          source_width > 0 ? static_cast<float>(source_width) * scale : 0.0f,
      .displayed_height =
          source_height > 0 ? static_cast<float>(source_height) * scale : 0.0f,
      .usable_width = usable_width,
  };
}

struct AdaptiveSheetGridLayout {
  float display_scale = 1.0f;
  int columns = 1;
  int rows = 0;
  float item_width = 0.0f;
  float item_height = 0.0f;
  float content_width = 0.0f;
  float content_height = 0.0f;
};

inline AdaptiveSheetGridLayout ResolveAdaptiveSheetGridLayout(
    float available_width, int source_item_width, int source_item_height,
    int item_count, int preferred_columns = 2,
    AdaptiveSheetScaleMode mode = AdaptiveSheetScaleMode::kFit,
    float min_scale = 0.35f, float max_scale = 4.0f, float gap = 4.0f,
    float horizontal_chrome = 24.0f) {
  if (source_item_width <= 0 || source_item_height <= 0 || item_count <= 0) {
    return {};
  }

  min_scale = std::max(0.01f, min_scale);
  max_scale = std::max(min_scale, max_scale);
  gap = std::max(0.0f, gap);
  const float usable =
      std::max(1.0f, available_width - std::max(0.0f, horizontal_chrome));
  int columns = std::clamp(preferred_columns, 1, item_count);

  auto fit_scale = [&](int candidate_columns) {
    const float gaps = gap * static_cast<float>(candidate_columns - 1);
    return std::max(0.01f,
                    (usable - gaps) / (static_cast<float>(source_item_width) *
                                       static_cast<float>(candidate_columns)));
  };

  float scale = AdaptiveSheetFixedScale(mode);
  if (mode == AdaptiveSheetScaleMode::kFit) {
    while (columns > 1 && fit_scale(columns) < min_scale) {
      --columns;
    }
    while (columns < item_count) {
      const int next_columns = columns + 1;
      const float next_width =
          static_cast<float>(next_columns * source_item_width) * max_scale +
          gap * static_cast<float>(next_columns - 1);
      if (next_width > usable) {
        break;
      }
      columns = next_columns;
    }
    scale = std::clamp(fit_scale(columns), min_scale, max_scale);
  } else {
    scale = std::clamp(scale, min_scale, max_scale);
    columns = std::clamp(
        static_cast<int>(
            std::floor((usable + gap) / (source_item_width * scale + gap))),
        1, item_count);
  }

  const int rows = (item_count + columns - 1) / columns;
  const float item_width = source_item_width * scale;
  const float item_height = source_item_height * scale;
  return {
      .display_scale = scale,
      .columns = columns,
      .rows = rows,
      .item_width = item_width,
      .item_height = item_height,
      .content_width =
          item_width * columns + gap * static_cast<float>(columns - 1),
      .content_height =
          item_height * rows + gap * static_cast<float>(std::max(0, rows - 1)),
  };
}

}  // namespace yaze::gui

#endif  // YAZE_APP_GUI_LAYOUT_ADAPTIVE_SHEET_LAYOUT_H_
