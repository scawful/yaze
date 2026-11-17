#include "app/editor/agent/ui/review_boards/review_boards_view.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/core/panel_registration.h"
#include "app/editor/registry/content_registry.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/canvas/canvas_geometry.h"
#include "app/gui/core/draw_sampling.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/empty_state.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "util/i18n/translator.h"

namespace yaze::editor {
namespace {

using i18n::tr;

constexpr size_t kMaxDownscaleCacheEntries = 32;

constexpr const char* kFilterLabels[] = {
    "Waiting on you", "Current", "Superseded", "Stale", "All",
};
constexpr const char* kFilterTabIds[] = {
    "tab_waiting", "tab_current", "tab_superseded", "tab_stale", "tab_all",
};
constexpr const char* kFilterIcons[] = {
    ICON_MD_RADIO_BUTTON_UNCHECKED,
    ICON_MD_RATE_REVIEW,
    ICON_MD_HISTORY,
    ICON_MD_SCHEDULE,
    ICON_MD_INBOX,
};
constexpr const char* kSortLabels[] = {
    "Newest",
    "Oldest",
    "Order",
    "Priority",
};
constexpr const char* kCategories[] = {"general", "layout"};

struct AxisSpan {
  int start = 0;
  uint32_t offset = 0;
  uint32_t count = 0;
};

void BuildAxisSpans(int src_len, int dst_len, std::vector<AxisSpan>* out_spans,
                    std::vector<uint32_t>* out_weights) {
  out_spans->assign(static_cast<size_t>(dst_len), AxisSpan{});
  out_weights->clear();
  if (dst_len >= src_len) {
    out_weights->resize(static_cast<size_t>(dst_len), 1024u);
    for (int d = 0; d < dst_len; ++d) {
      const int idx = std::clamp(
          static_cast<int>((static_cast<int64_t>(d) * src_len) / dst_len), 0,
          src_len - 1);
      auto& span = (*out_spans)[static_cast<size_t>(d)];
      span.start = idx;
      span.offset = static_cast<uint32_t>(d);
      span.count = 1u;
    }
    return;
  }

  constexpr double kWeightScale = 1024.0;
  out_weights->reserve(static_cast<size_t>(src_len) +
                       static_cast<size_t>(dst_len) * 2u);
  for (int d = 0; d < dst_len; ++d) {
    const double s0 = (static_cast<double>(d) * src_len) / dst_len;
    const double s1 = (static_cast<double>(d + 1) * src_len) / dst_len;
    const int i0 = std::clamp(static_cast<int>(std::floor(s0)), 0, src_len - 1);
    const int i1 =
        std::clamp(static_cast<int>(std::ceil(s1)) - 1, i0, src_len - 1);
    AxisSpan span;
    span.start = i0;
    span.offset = static_cast<uint32_t>(out_weights->size());
    span.count = static_cast<uint32_t>(i1 - i0 + 1);
    uint32_t total = 0;
    for (int i = i0; i <= i1; ++i) {
      const double left = std::max(s0, static_cast<double>(i));
      const double right = std::min(s1, static_cast<double>(i + 1));
      const double overlap = std::max(0.0, right - left);
      const auto w = static_cast<uint32_t>(std::lround(overlap * kWeightScale));
      out_weights->push_back(w);
      total += w;
    }
    if (total == 0 && span.count > 0) {
      (*out_weights)[span.offset] = 1024u;
    }
    (*out_spans)[static_cast<size_t>(d)] = span;
  }
}

std::vector<uint8_t> HalveRgba2x2(const std::vector<uint8_t>& src, int src_w,
                                  int src_h) {
  const int dst_w = src_w / 2;
  const int dst_h = src_h / 2;
  std::vector<uint8_t> out(static_cast<size_t>(dst_w) *
                           static_cast<size_t>(dst_h) * 4u);
  const size_t src_stride = static_cast<size_t>(src_w) * 4u;
  for (int dy = 0; dy < dst_h; ++dy) {
    const uint8_t* row0 = src.data() + static_cast<size_t>(dy * 2) * src_stride;
    const uint8_t* row1 = row0 + src_stride;
    uint8_t* dst_row =
        out.data() + static_cast<size_t>(dy) * static_cast<size_t>(dst_w) * 4u;
    for (int dx = 0; dx < dst_w; ++dx) {
      const uint8_t* p0 = row0 + static_cast<size_t>(dx * 2) * 4u;
      const uint8_t* p1 = p0 + 4u;
      const uint8_t* p2 = row1 + static_cast<size_t>(dx * 2) * 4u;
      const uint8_t* p3 = p2 + 4u;
      uint8_t* dst = dst_row + static_cast<size_t>(dx) * 4u;
      if ((p0[3] & p1[3] & p2[3] & p3[3]) == 255u) {
        dst[0] = static_cast<uint8_t>(
            (uint32_t{p0[0]} + p1[0] + p2[0] + p3[0] + 2u) >> 2);
        dst[1] = static_cast<uint8_t>(
            (uint32_t{p0[1]} + p1[1] + p2[1] + p3[1] + 2u) >> 2);
        dst[2] = static_cast<uint8_t>(
            (uint32_t{p0[2]} + p1[2] + p2[2] + p3[2] + 2u) >> 2);
        dst[3] = 255u;
      } else {
        const uint32_t a0 = p0[3];
        const uint32_t a1 = p1[3];
        const uint32_t a2 = p2[3];
        const uint32_t a3 = p3[3];
        const uint32_t sum_a = a0 + a1 + a2 + a3;
        if (sum_a > 0) {
          dst[0] = static_cast<uint8_t>((p0[0] * a0 + p1[0] * a1 + p2[0] * a2 +
                                         p3[0] * a3 + (sum_a >> 1)) /
                                        sum_a);
          dst[1] = static_cast<uint8_t>((p0[1] * a0 + p1[1] * a1 + p2[1] * a2 +
                                         p3[1] * a3 + (sum_a >> 1)) /
                                        sum_a);
          dst[2] = static_cast<uint8_t>((p0[2] * a0 + p1[2] * a1 + p2[2] * a2 +
                                         p3[2] * a3 + (sum_a >> 1)) /
                                        sum_a);
          dst[3] = static_cast<uint8_t>((sum_a + 2u) >> 2);
        }
      }
    }
  }
  return out;
}

bool IsStale(const ReviewBoard& board) {
  return absl::StartsWith(board.status, "stale");
}

struct StateLook {
  const char* icon;
  ImVec4 color;
  const char* label;
};

StateLook DecisionLook(const std::string& decision) {
  if (decision == "looks_good")
    return {ICON_MD_CHECK_CIRCLE, gui::GetSuccessColor(), "Looks good"};
  if (decision == "needs_changes")
    return {ICON_MD_EDIT_NOTE, gui::GetWarningColor(), "Needs changes"};
  return {ICON_MD_CHAT_BUBBLE_OUTLINE, gui::GetInfoColor(), "Comment"};
}

StateLook BoardLook(const ReviewBoard& board) {
  if (ReviewBoardWaitingOnUser(board)) {
    return {ICON_MD_RADIO_BUTTON_UNCHECKED, gui::GetAccentColor(),
            "Waiting on you"};
  }
  const std::string decision = ReviewBoardDecision(board);
  if (!decision.empty())
    return DecisionLook(decision);
  if (board.status == "current") {
    return {ICON_MD_CHECK_CIRCLE, gui::GetSuccessColor(), "Answered"};
  }
  if (IsStale(board))
    return {ICON_MD_SCHEDULE, gui::GetTextDisabledVec4(), "Stale"};
  return {ICON_MD_HISTORY, gui::GetTextDisabledVec4(), "Superseded"};
}

std::string SubjectLine(const ReviewBoard& board) {
  if (board.subject_kind.empty())
    return {};
  std::string kind = board.subject_kind;
  kind[0] =
      static_cast<char>(std::toupper(static_cast<unsigned char>(kind[0])));
  std::string line = kind;
  if (!board.subject_id.empty())
    line += " " + board.subject_id;
  if (!board.subject_label.empty())
    line += " - " + board.subject_label;
  return line;
}

std::string FormatDoubleInput(double value) {
  if (value == std::trunc(value))
    return absl::StrFormat("%.0f", value);
  return absl::StrFormat("%g", value);
}

bool ParseDoubleStrict(const std::string& text, double* out) {
  const std::string trimmed(absl::StripAsciiWhitespace(text));
  if (trimmed.empty())
    return false;
  char* end = nullptr;
  const double val = std::strtod(trimmed.c_str(), &end);
  if (end == trimmed.c_str() || *end != '\0' || !std::isfinite(val))
    return false;
  *out = val;
  return true;
}

bool IsEmptyAnswerEntry(const ReviewBoardAnswer& ans) {
  if (!absl::StripAsciiWhitespace(ans.other).empty())
    return false;
  if (std::holds_alternative<std::monostate>(ans.value))
    return true;
  if (std::holds_alternative<std::string>(ans.value))
    return std::get<std::string>(ans.value).empty();
  if (std::holds_alternative<std::vector<std::string>>(ans.value))
    return std::get<std::vector<std::string>>(ans.value).empty();
  return false;
}

bool AnswerMapsSemanticallyEqual(const ReviewBoardAnswerMap& a,
                                 const ReviewBoardAnswerMap& b) {
  for (const auto& [k, va] : a) {
    const auto it = b.find(k);
    if (it == b.end()) {
      if (!IsEmptyAnswerEntry(va))
        return false;
    } else if (!(va == it->second)) {
      if (!(IsEmptyAnswerEntry(va) && IsEmptyAnswerEntry(it->second)))
        return false;
    }
  }
  for (const auto& [k, vb] : b) {
    if (!a.contains(k) && !IsEmptyAnswerEntry(vb))
      return false;
  }
  return true;
}

}  // namespace

ImVec2 SanitizeReviewBoardFramebufferScale(ImVec2 fb_scale) {
  const float sx =
      (std::isfinite(fb_scale.x) && fb_scale.x > 0.0f) ? fb_scale.x : 1.0f;
  const float sy =
      (std::isfinite(fb_scale.y) && fb_scale.y > 0.0f) ? fb_scale.y : 1.0f;
  return ImVec2(sx, sy);
}

float ComputeReviewBoardFitZoom(ImVec2 image_size, ImVec2 avail_logical,
                                ImVec2 fb_scale) {
  if (image_size.x <= 0.0f || image_size.y <= 0.0f ||
      !std::isfinite(image_size.x) || !std::isfinite(image_size.y)) {
    return 1.0f;
  }
  const ImVec2 fb = SanitizeReviewBoardFramebufferScale(fb_scale);
  const float avail_phys_w =
      std::max(1.0f, std::floor(std::max(1.0f, avail_logical.x) * fb.x));
  const float avail_phys_h =
      std::max(1.0f, std::floor(std::max(1.0f, avail_logical.y) * fb.y));
  const float raw =
      std::min(avail_phys_w / image_size.x, avail_phys_h / image_size.y);
  return std::clamp(raw, kReviewBoardViewerMinZoom, 4.0f);
}

ImVec2 ClampReviewBoardViewerPan(ImVec2 pan, ImVec2 drawn_logical_size,
                                 ImVec2 avail_logical) {
  const float avail_w = std::max(1.0f, avail_logical.x);
  const float avail_h = std::max(1.0f, avail_logical.y);
  const float drawn_w = std::max(1.0f, drawn_logical_size.x);
  const float drawn_h = std::max(1.0f, drawn_logical_size.y);
  const float keep_x = std::min(64.0f, std::min(drawn_w, avail_w) * 0.5f);
  const float keep_y = std::min(64.0f, std::min(drawn_h, avail_h) * 0.5f);
  const float min_x = keep_x - drawn_w;
  const float max_x = avail_w - keep_x;
  const float min_y = keep_y - drawn_h;
  const float max_y = avail_h - keep_y;
  const float px = std::isfinite(pan.x) ? pan.x : 0.0f;
  const float py = std::isfinite(pan.y) ? pan.y : 0.0f;
  return ImVec2(std::clamp(px, std::min(min_x, max_x), std::max(min_x, max_x)),
                std::clamp(py, std::min(min_y, max_y), std::max(min_y, max_y)));
}

ReviewBoardViewerLayout ResolveReviewBoardViewerLayout(
    const ReviewBoardViewerState& state, ImVec2 image_size,
    ImVec2 avail_logical, ImVec2 fb_scale) {
  ReviewBoardViewerLayout out;
  const ImVec2 fb = SanitizeReviewBoardFramebufferScale(fb_scale);
  const ImVec2 avail(std::max(1.0f, avail_logical.x),
                     std::max(1.0f, avail_logical.y));
  if (image_size.x <= 0.0f || image_size.y <= 0.0f ||
      !std::isfinite(image_size.x) || !std::isfinite(image_size.y)) {
    return out;
  }

  if (state.fit_mode) {
    out.physical_zoom = ComputeReviewBoardFitZoom(image_size, avail, fb);
  } else {
    const float requested = std::isfinite(state.zoom) ? state.zoom : 1.0f;
    out.physical_zoom = std::clamp(requested, kReviewBoardViewerMinZoom,
                                   kReviewBoardViewerMaxZoom);
  }

  out.target_physical_width = std::max(
      1, static_cast<int>(std::lround(image_size.x * out.physical_zoom)));
  out.target_physical_height = std::max(
      1, static_cast<int>(std::lround(image_size.y * out.physical_zoom)));
  const int src_w = std::max(1, static_cast<int>(std::lround(image_size.x)));
  const int src_h = std::max(1, static_cast<int>(std::lround(image_size.y)));
  out.use_nearest_sampling = (out.target_physical_width >= src_w &&
                              out.target_physical_height >= src_h);

  out.drawn_logical_size =
      ImVec2(static_cast<float>(out.target_physical_width) / fb.x,
             static_cast<float>(out.target_physical_height) / fb.y);
  out.logical_scale = ImVec2(out.drawn_logical_size.x / image_size.x,
                             out.drawn_logical_size.y / image_size.y);
  out.zoom_percent =
      std::max(1, static_cast<int>(std::lround(out.physical_zoom * 100.0f)));

  if (state.fit_mode) {
    out.pan = ImVec2(
        std::round((avail.x - out.drawn_logical_size.x) * 0.5f * fb.x) / fb.x,
        std::round((avail.y - out.drawn_logical_size.y) * 0.5f * fb.y) / fb.y);
  } else {
    out.pan =
        ClampReviewBoardViewerPan(state.pan, out.drawn_logical_size, avail);
  }
  return out;
}

ReviewBoardViewerState ZoomReviewBoardAroundScreenPos(
    const ReviewBoardViewerState& state, float target_physical_zoom,
    ImVec2 anchor_screen_pos, ImVec2 canvas_p0, ImVec2 image_size,
    ImVec2 avail_logical, ImVec2 fb_scale) {
  const ImVec2 fb = SanitizeReviewBoardFramebufferScale(fb_scale);
  const ImVec2 avail(std::max(1.0f, avail_logical.x),
                     std::max(1.0f, avail_logical.y));
  const float clamped_zoom =
      std::isfinite(target_physical_zoom)
          ? std::clamp(target_physical_zoom, kReviewBoardViewerMinZoom,
                       kReviewBoardViewerMaxZoom)
          : 1.0f;

  ReviewBoardViewerState next;
  next.fit_mode = false;
  next.zoom = clamped_zoom;
  if (image_size.x <= 0.0f || image_size.y <= 0.0f ||
      !std::isfinite(image_size.x) || !std::isfinite(image_size.y)) {
    return next;
  }

  const ReviewBoardViewerLayout cur =
      ResolveReviewBoardViewerLayout(state, image_size, avail, fb);

  ReviewBoardViewerState target_probe = next;
  target_probe.pan = cur.pan;
  const ReviewBoardViewerLayout next_geom =
      ResolveReviewBoardViewerLayout(target_probe, image_size, avail, fb);

  gui::CanvasGeometry geom;
  geom.canvas_p0 = canvas_p0;
  geom.scrolling = cur.pan;
  const ImVec2 scroll_x = gui::ComputeScrollForZoomAtScreenPos(
      geom, cur.logical_scale.x, next_geom.logical_scale.x, anchor_screen_pos);
  ImVec2 new_pan = scroll_x;
  if (std::abs(cur.logical_scale.x - cur.logical_scale.y) > 1e-6f ||
      std::abs(next_geom.logical_scale.x - next_geom.logical_scale.y) > 1e-6f) {
    const ImVec2 scroll_y = gui::ComputeScrollForZoomAtScreenPos(
        geom, cur.logical_scale.y, next_geom.logical_scale.y,
        anchor_screen_pos);
    new_pan.y = scroll_y.y;
  }
  next.pan =
      ClampReviewBoardViewerPan(new_pan, next_geom.drawn_logical_size, avail);
  return next;
}

ReviewBoardViewerState StepReviewBoardWheelZoom(
    const ReviewBoardViewerState& state, float wheel_y,
    ImVec2 anchor_screen_pos, ImVec2 canvas_p0, ImVec2 image_size,
    ImVec2 avail_logical, ImVec2 fb_scale) {
  if (!std::isfinite(wheel_y) || wheel_y == 0.0f) {
    return state;
  }
  const ReviewBoardViewerLayout cur = ResolveReviewBoardViewerLayout(
      state, image_size, avail_logical, fb_scale);
  const float bounded_notches = std::clamp(wheel_y, -32.0f, 32.0f);
  const float target_zoom =
      std::clamp(cur.physical_zoom *
                     std::pow(kReviewBoardViewerWheelStep, bounded_notches),
                 kReviewBoardViewerMinZoom, kReviewBoardViewerMaxZoom);
  return ZoomReviewBoardAroundScreenPos(state, target_zoom, anchor_screen_pos,
                                        canvas_p0, image_size, avail_logical,
                                        fb_scale);
}

ReviewBoardViewerState ToggleReviewBoardViewerFitOrActual(
    const ReviewBoardViewerState& state, ImVec2 anchor_screen_pos,
    ImVec2 canvas_p0, ImVec2 image_size, ImVec2 avail_logical,
    ImVec2 fb_scale) {
  if (!state.fit_mode) {
    return ReviewBoardViewerState{};
  }
  return ZoomReviewBoardAroundScreenPos(state, 1.0f, anchor_screen_pos,
                                        canvas_p0, image_size, avail_logical,
                                        fb_scale);
}

bool ShouldHandleReviewBoardCanvasWheel(bool canvas_hovered,
                                        bool want_text_input, float wheel_y) {
  return canvas_hovered && !want_text_input && std::isfinite(wheel_y) &&
         wheel_y != 0.0f;
}

std::vector<uint8_t> BoxDownscaleRgba(std::span<const uint8_t> src_rgba,
                                      int src_w, int src_h, int dst_w,
                                      int dst_h) {
  if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
    return {};
  }
  const size_t expected_bytes =
      static_cast<size_t>(src_w) * static_cast<size_t>(src_h) * 4u;
  if (src_rgba.size() < expected_bytes) {
    return {};
  }
  if (src_w == dst_w && src_h == dst_h) {
    return std::vector<uint8_t>(src_rgba.begin(),
                                src_rgba.begin() + expected_bytes);
  }

  std::vector<AxisSpan> x_spans;
  std::vector<uint32_t> x_weights;
  std::vector<AxisSpan> y_spans;
  std::vector<uint32_t> y_weights;
  BuildAxisSpans(src_w, dst_w, &x_spans, &x_weights);
  BuildAxisSpans(src_h, dst_h, &y_spans, &y_weights);

  std::vector<uint8_t> out(
      static_cast<size_t>(dst_w) * static_cast<size_t>(dst_h) * 4u, 0u);
  const size_t src_stride = static_cast<size_t>(src_w) * 4u;
  for (int dy = 0; dy < dst_h; ++dy) {
    const AxisSpan& ys = y_spans[static_cast<size_t>(dy)];
    uint8_t* dst_row =
        out.data() + static_cast<size_t>(dy) * static_cast<size_t>(dst_w) * 4u;
    for (int dx = 0; dx < dst_w; ++dx) {
      const AxisSpan& xs = x_spans[static_cast<size_t>(dx)];
      uint64_t sum_w = 0;
      uint64_t sum_a = 0;
      uint64_t sum_r = 0;
      uint64_t sum_g = 0;
      uint64_t sum_b = 0;
      for (uint32_t ky = 0; ky < ys.count; ++ky) {
        const uint64_t wy = y_weights[ys.offset + ky];
        if (wy == 0)
          continue;
        const uint8_t* src_row =
            src_rgba.data() +
            static_cast<size_t>(ys.start + static_cast<int>(ky)) * src_stride;
        for (uint32_t kx = 0; kx < xs.count; ++kx) {
          const uint64_t wx = x_weights[xs.offset + kx];
          if (wx == 0)
            continue;
          const uint64_t w = wy * wx;
          const uint8_t* px =
              src_row +
              static_cast<size_t>(xs.start + static_cast<int>(kx)) * 4u;
          const uint64_t a = px[3];
          sum_w += w;
          sum_a += w * a;
          sum_r += w * px[0] * a;
          sum_g += w * px[1] * a;
          sum_b += w * px[2] * a;
        }
      }
      uint8_t* dst = dst_row + static_cast<size_t>(dx) * 4u;
      if (sum_w > 0 && sum_a > 0) {
        dst[0] = static_cast<uint8_t>((sum_r + (sum_a >> 1)) / sum_a);
        dst[1] = static_cast<uint8_t>((sum_g + (sum_a >> 1)) / sum_a);
        dst[2] = static_cast<uint8_t>((sum_b + (sum_a >> 1)) / sum_a);
        dst[3] = static_cast<uint8_t>((sum_a + (sum_w >> 1)) / sum_w);
      }
    }
  }
  return out;
}

std::vector<ReviewBoardRgbaMip> BuildReviewBoardMipChain(
    int width, int height, std::shared_ptr<const std::vector<uint8_t>> rgba) {
  std::vector<ReviewBoardRgbaMip> chain;
  if (width <= 0 || height <= 0 || !rgba ||
      rgba->size() <
          static_cast<size_t>(width) * static_cast<size_t>(height) * 4u) {
    return chain;
  }
  chain.push_back(ReviewBoardRgbaMip{width, height, std::move(rgba)});
  while (chain.back().width >= 4 && chain.back().height >= 4) {
    const ReviewBoardRgbaMip& prev = chain.back();
    const int next_w = prev.width / 2;
    const int next_h = prev.height / 2;
    std::vector<uint8_t> level_rgba;
    if (prev.width == next_w * 2 && prev.height == next_h * 2) {
      level_rgba = HalveRgba2x2(*prev.rgba, prev.width, prev.height);
    } else {
      level_rgba =
          BoxDownscaleRgba(*prev.rgba, prev.width, prev.height, next_w, next_h);
    }
    if (level_rgba.empty())
      break;
    chain.push_back(ReviewBoardRgbaMip{
        next_w, next_h,
        std::make_shared<const std::vector<uint8_t>>(std::move(level_rgba))});
  }
  return chain;
}

std::vector<uint8_t> DownscaleReviewBoardFromMips(
    const std::vector<ReviewBoardRgbaMip>& mips, int dst_w, int dst_h) {
  if (mips.empty() || dst_w <= 0 || dst_h <= 0) {
    return {};
  }
  size_t best = 0;
  for (size_t i = 1; i < mips.size(); ++i) {
    if (mips[i].width >= dst_w && mips[i].height >= dst_h) {
      best = i;
    } else {
      break;
    }
  }
  const ReviewBoardRgbaMip& src = mips[best];
  return BoxDownscaleRgba(*src.rgba, src.width, src.height, dst_w, dst_h);
}

ReviewBoardsView::~ReviewBoardsView() {
  ReleaseImages();
}

void ReviewBoardsView::ReleaseImages() {
  for (auto& entry : downscale_cache_) {
    if (entry.bitmap)
      gfx::Arena::Get().RetireBitmap(*entry.bitmap);
  }
  downscale_cache_.clear();
  image_mips_.clear();
  image_sources_.clear();
  for (auto& bitmap : images_) {
    if (bitmap)
      gfx::Arena::Get().RetireBitmap(*bitmap);
  }
  images_.clear();
  image_errors_.clear();
  images_for_.clear();
}

void ReviewBoardsView::NotifyToast(const std::string& message, bool is_error) {
  if (auto* tm = ContentRegistry::Context::toast_manager()) {
    tm->Show(message, is_error ? ToastType::kError : ToastType::kSuccess);
  }
}

void ReviewBoardsView::ForceRefresh() {
  Rescan(/*reset_watcher=*/false);
}

void ReviewBoardsView::SetBoardsDirectory(const std::filesystem::path& dir) {
  dir_ = dir;
  scanned_ = false;
  drafts_.clear();
  selected_id_.clear();
  ReleaseImages();
  Rescan(/*reset_watcher=*/true);
}

void ReviewBoardsView::Rescan(bool reset_watcher) {
  scanned_ = true;
  last_rescan_at_ = absl::Now();
  dir_error_.clear();
  if (dir_.empty()) {
    auto dir = DefaultReviewBoardsDirectory();
    if (!dir.ok()) {
      dir_error_ = std::string(dir.status().message());
      scan_ = {};
      visible_.clear();
      return;
    }
    dir_ = *dir;
  }
  if (reset_watcher) {
    watcher_state_ = {};
    scan_ = {};
  } else {
    // Manual refresh forces a full mtime recheck while keeping initial_ids so
    // `new` badges remain meaningful for this session.
    watcher_state_.stamps.clear();
  }
  RescanReviewBoardsIncremental(dir_, scan_, watcher_state_);
  SyncCleanDraftsAfterScan();
  ReleaseImages();
  RefreshVisible(selected_id_);
  if (!Selected() && !visible_.empty()) {
    selected_id_ = scan_.boards[visible_.front()].id;
  }
}

void ReviewBoardsView::PollIncrementalRescan() {
  const absl::Time now = absl::Now();
  if (now - last_rescan_at_ < absl::Seconds(3))
    return;
  last_rescan_at_ = now;
  if (dir_.empty())
    return;
  if (RescanReviewBoardsIncremental(dir_, scan_, watcher_state_)) {
    SyncCleanDraftsAfterScan();
    RefreshVisible(selected_id_);
    if (!Selected() && !visible_.empty()) {
      selected_id_ = scan_.boards[visible_.front()].id;
    }
  }
}

void ReviewBoardsView::SyncCleanDraftsAfterScan() {
  for (const auto& board : scan_.boards) {
    auto it = drafts_.find(board.id);
    if (it == drafts_.end() || !it->second.initialized)
      continue;
    if (!HasUnsavedDraft(board)) {
      it->second = BoardDraft{};
      EnsureDraft(board);
    } else if (!it->second.draft_board_sha256.empty() &&
               it->second.draft_board_sha256 != board.board_sha256) {
      it->second.stale_after_republish = true;
      const ReviewBoardAnswerMap fresh_defaults =
          InitialReviewBoardAnswers(board);
      for (const auto& q : board.questions) {
        if (!it->second.answers.contains(q.id)) {
          const auto def_it = fresh_defaults.find(q.id);
          const ReviewBoardAnswer seeded = def_it != fresh_defaults.end()
                                               ? def_it->second
                                               : ReviewBoardAnswer{};
          it->second.answers[q.id] = seeded;
          if (q.kind == "number" &&
              std::holds_alternative<double>(seeded.value)) {
            it->second.number_inputs[q.id] =
                FormatDoubleInput(std::get<double>(seeded.value));
          }
        }
      }
    }
  }
}

void ReviewBoardsView::RefreshVisible(const std::string& keep_id) {
  visible_.clear();
  const std::vector<int> sorted = SortedReviewBoardIndices(scan_.boards, sort_);
  for (int idx : sorted) {
    const ReviewBoard& board = scan_.boards[idx];
    const bool keep = !keep_id.empty() && board.id == keep_id;
    if (keep || (MatchesBoard(board, filter_) &&
                 MatchesReviewBoardQuery(board, search_query_, group_filter_,
                                         author_filter_))) {
      visible_.push_back(idx);
    }
  }
}

bool ReviewBoardsView::MatchesBoard(const ReviewBoard& board,
                                    ReviewBoardFilter filter) const {
  return MatchesReviewBoardFilter(board, filter);
}

ReviewBoard* ReviewBoardsView::Selected() {
  for (auto& board : scan_.boards) {
    if (board.id == selected_id_)
      return &board;
  }
  return nullptr;
}

const ReviewBoard* ReviewBoardsView::Selected() const {
  for (const auto& board : scan_.boards) {
    if (board.id == selected_id_)
      return &board;
  }
  return nullptr;
}

void ReviewBoardsView::Select(const std::string& id) {
  if (id == selected_id_)
    return;
  selected_id_ = id;
  status_.clear();
  if (const ReviewBoard* board = Selected()) {
    EnsureDraft(*board);
  }
}

void ReviewBoardsView::SelectBoardById(const std::string& id) {
  if (!scanned_)
    Rescan(/*reset_watcher=*/true);
  Select(id);
  RefreshVisible(id);
}

void ReviewBoardsView::SelectNextWaiting() {
  if (!scanned_)
    Rescan(/*reset_watcher=*/true);
  if (filter_ != ReviewBoardFilter::kWaitingOnYou) {
    filter_ = ReviewBoardFilter::kWaitingOnYou;
    force_select_filter_tab_ = true;
    RefreshVisible({});
  }
  const std::vector<int> ordered =
      SortedReviewBoardIndices(scan_.boards, sort_);
  if (ordered.empty())
    return;
  int start = 0;
  for (int i = 0; i < static_cast<int>(ordered.size()); ++i) {
    if (scan_.boards[ordered[i]].id == selected_id_) {
      start = i + 1;
      break;
    }
  }
  for (int n = 0; n < static_cast<int>(ordered.size()); ++n) {
    const ReviewBoard& candidate =
        scan_.boards[ordered[(start + n) % ordered.size()]];
    if (ReviewBoardWaitingOnUser(candidate)) {
      Select(candidate.id);
      RefreshVisible(candidate.id);
      return;
    }
  }
}

ReviewBoardsView::BoardDraft& ReviewBoardsView::EnsureDraft(
    const ReviewBoard& board) {
  BoardDraft& draft = drafts_[board.id];
  if (draft.initialized)
    return draft;

  draft.answers = InitialReviewBoardAnswers(board);
  for (const auto& q : board.questions) {
    (void)draft.answers[q.id];
  }
  draft.baseline_answers = draft.answers;
  const std::string last_dec = ReviewBoardDecision(board);
  draft.decision = last_dec.empty() ? "looks_good" : last_dec;
  draft.baseline_decision = draft.decision;
  draft.category = 0;
  if (!board.feedback.empty() && board.feedback.back().category == "layout") {
    draft.category = 1;
  }
  draft.comment[0] = '\0';
  draft.single_other_selected.clear();
  draft.number_inputs.clear();

  for (const auto& q : board.questions) {
    const auto it = draft.answers.find(q.id);
    if (it == draft.answers.end())
      continue;
    if (q.kind == "number" &&
        std::holds_alternative<double>(it->second.value)) {
      draft.number_inputs[q.id] =
          FormatDoubleInput(std::get<double>(it->second.value));
    } else if (q.kind == "single" && q.allow_other &&
               !it->second.other.empty()) {
      bool matches_opt = false;
      if (std::holds_alternative<std::string>(it->second.value)) {
        const auto& sel = std::get<std::string>(it->second.value);
        for (const auto& opt : q.options) {
          if (opt.id == sel) {
            matches_opt = true;
            break;
          }
        }
      }
      if (!matches_opt) {
        draft.single_other_selected[q.id] = true;
      }
    }
  }

  draft.draft_board_sha256 = board.board_sha256;
  draft.stale_after_republish = false;
  draft.initialized = true;
  return draft;
}

void ReviewBoardsView::SetDraftAnswer(const std::string& board_id,
                                      const std::string& question_id,
                                      ReviewBoardAnswer answer) {
  for (const auto& board : scan_.boards) {
    if (board.id == board_id) {
      BoardDraft& draft = EnsureDraft(board);
      draft.answers[question_id] = std::move(answer);
      return;
    }
  }
}

bool ReviewBoardsView::IsDraftStaleAfterRepublish(
    const std::string& board_id) const {
  const auto it = drafts_.find(board_id);
  return it != drafts_.end() && it->second.initialized &&
         it->second.stale_after_republish;
}

void ReviewBoardsView::ReloadDraftFromBoard(const ReviewBoard& board) {
  drafts_[board.id] = BoardDraft{};
  EnsureDraft(board);
}

void ReviewBoardsView::KeepDraftEditsOnUpdatedBoard(const ReviewBoard& board) {
  BoardDraft& draft = EnsureDraft(board);
  draft.answers = board.questions.empty() ? ReviewBoardAnswerMap{}
                                          : FilterReviewBoardAnswersToQuestions(
                                                board, draft.answers);

  std::unordered_set<std::string> valid_ids;
  valid_ids.reserve(board.questions.size());
  for (const auto& q : board.questions) {
    valid_ids.insert(q.id);
  }
  for (auto it = draft.single_other_selected.begin();
       it != draft.single_other_selected.end();) {
    if (!valid_ids.contains(it->first)) {
      it = draft.single_other_selected.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = draft.number_inputs.begin();
       it != draft.number_inputs.end();) {
    if (!valid_ids.contains(it->first)) {
      it = draft.number_inputs.erase(it);
    } else {
      ++it;
    }
  }

  ReviewBoardAnswerMap fresh_baseline = InitialReviewBoardAnswers(board);
  for (const auto& q : board.questions) {
    if (!draft.answers.contains(q.id)) {
      const auto base_it = fresh_baseline.find(q.id);
      const ReviewBoardAnswer seeded = base_it != fresh_baseline.end()
                                           ? base_it->second
                                           : ReviewBoardAnswer{};
      draft.answers[q.id] = seeded;
      if (q.kind == "number" && std::holds_alternative<double>(seeded.value)) {
        draft.number_inputs[q.id] =
            FormatDoubleInput(std::get<double>(seeded.value));
      }
    }
    (void)fresh_baseline[q.id];
  }
  draft.baseline_answers = std::move(fresh_baseline);
  draft.draft_board_sha256 = board.board_sha256;
  draft.stale_after_republish = false;
}

bool ReviewBoardsView::HasUnsavedDraft(const ReviewBoard& board) const {
  const auto it = drafts_.find(board.id);
  if (it == drafts_.end() || !it->second.initialized)
    return false;
  const BoardDraft& draft = it->second;
  if (!absl::StripAsciiWhitespace(draft.comment).empty())
    return true;
  if (draft.decision != draft.baseline_decision)
    return true;
  return !AnswerMapsSemanticallyEqual(draft.answers, draft.baseline_answers);
}

const ReviewBoardAnswerMap* ReviewBoardsView::DraftAnswersFor(
    const std::string& board_id) const {
  const auto it = drafts_.find(board_id);
  if (it == drafts_.end() || !it->second.initialized)
    return nullptr;
  return &it->second.answers;
}

void ReviewBoardsView::EnsureImagesLoaded(const ReviewBoard& board) {
  const std::string key = board.id + "@" + board.board_sha256;
  if (key == images_for_)
    return;
  ReleaseImages();
  images_for_ = key;
  image_sources_.resize(board.media.size());
  image_mips_.resize(board.media.size());
  for (size_t i = 0; i < board.media.size(); ++i) {
    auto bitmap = std::make_unique<gfx::Bitmap>();
    std::string error;
    if (board.media[i].kind == "image") {
      auto image = LoadReviewBoardImage(board, i);
      if (image.ok() && image->rgba) {
        bitmap->Create(image->width, image->height, 32, *image->rgba);
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::CREATE, bitmap.get());
        image_sources_[i] = std::move(*image);
      } else if (!image.ok()) {
        error = std::string(image.status().message());
      }
    }
    images_.push_back(std::move(bitmap));
    image_errors_.push_back(std::move(error));
  }
}

void ReviewBoardsView::EnsureImageMipsBuilt(size_t media_index) {
  if (media_index >= image_sources_.size() ||
      media_index >= image_mips_.size()) {
    return;
  }
  if (!image_mips_[media_index].empty())
    return;
  const ReviewBoardImage& src = image_sources_[media_index];
  if (src.width > 0 && src.height > 0 && src.rgba) {
    image_mips_[media_index] =
        BuildReviewBoardMipChain(src.width, src.height, src.rgba);
  }
}

gfx::TextureHandle ReviewBoardsView::GetTextureForPhysicalSize(
    size_t media_index, int target_phys_w, int target_phys_h,
    bool* out_nearest_sampling) {
  if (media_index >= images_.size() || !images_[media_index] ||
      !images_[media_index]->is_active()) {
    if (out_nearest_sampling != nullptr)
      *out_nearest_sampling = true;
    return nullptr;
  }
  const int src_w = images_[media_index]->width();
  const int src_h = images_[media_index]->height();
  if (src_w <= 0 || src_h <= 0) {
    if (out_nearest_sampling != nullptr)
      *out_nearest_sampling = true;
    return nullptr;
  }
  const int phys_w = std::max(1, target_phys_w);
  const int phys_h = std::max(1, target_phys_h);
  if (phys_w >= src_w && phys_h >= src_h) {
    if (out_nearest_sampling != nullptr)
      *out_nearest_sampling = true;
    return images_[media_index]->texture();
  }

  if (out_nearest_sampling != nullptr)
    *out_nearest_sampling = false;

  ++downscale_cache_tick_;
  gfx::TextureHandle closest_ready = nullptr;
  int closest_dist = std::numeric_limits<int>::max();
  DownscaleCacheEntry* exact_entry = nullptr;
  for (auto& entry : downscale_cache_) {
    if (entry.media_index != media_index)
      continue;
    if (entry.phys_width == phys_w && entry.phys_height == phys_h) {
      exact_entry = &entry;
    }
    if (entry.bitmap && entry.bitmap->texture() != nullptr) {
      const int dist = std::abs(entry.phys_width - phys_w) +
                       std::abs(entry.phys_height - phys_h);
      if (dist < closest_dist) {
        closest_dist = dist;
        closest_ready = entry.bitmap->texture();
      }
    }
  }

  if (exact_entry != nullptr) {
    exact_entry->last_used_tick = downscale_cache_tick_;
    if (exact_entry->bitmap && exact_entry->bitmap->texture() != nullptr) {
      return exact_entry->bitmap->texture();
    }
    return closest_ready != nullptr ? closest_ready
                                    : images_[media_index]->texture();
  }

  EnsureImageMipsBuilt(media_index);
  if (media_index >= image_mips_.size() || image_mips_[media_index].empty()) {
    return images_[media_index]->texture();
  }
  const std::vector<uint8_t> scaled_rgba =
      DownscaleReviewBoardFromMips(image_mips_[media_index], phys_w, phys_h);
  if (scaled_rgba.empty()) {
    return images_[media_index]->texture();
  }

  if (downscale_cache_.size() >= kMaxDownscaleCacheEntries) {
    auto lru_it = std::min_element(
        downscale_cache_.begin(), downscale_cache_.end(),
        [](const DownscaleCacheEntry& a, const DownscaleCacheEntry& b) {
          return a.last_used_tick < b.last_used_tick;
        });
    if (lru_it != downscale_cache_.end()) {
      if (lru_it->bitmap)
        gfx::Arena::Get().RetireBitmap(*lru_it->bitmap);
      downscale_cache_.erase(lru_it);
    }
  }

  auto scaled_bmp = std::make_unique<gfx::Bitmap>();
  scaled_bmp->Create(phys_w, phys_h, 32, scaled_rgba);
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        scaled_bmp.get());
  downscale_cache_.push_back(DownscaleCacheEntry{media_index, phys_w, phys_h,
                                                 downscale_cache_tick_,
                                                 std::move(scaled_bmp)});

  return closest_ready != nullptr ? closest_ready
                                  : images_[media_index]->texture();
}

void ReviewBoardsView::OpenImageViewer(int media_index) {
  viewer_media_index_ = media_index;
  viewer_state_ = ReviewBoardViewerState{};
  viewer_last_canvas_avail_ = ImVec2(0.0f, 0.0f);
  viewer_last_modal_size_ = ImVec2(0.0f, 0.0f);
  viewer_dragging_ = false;
  image_viewer_open_ = true;
  open_image_viewer_popup_ = true;
}

void ReviewBoardsView::DrawImageViewerModal(const ReviewBoard& board) {
  const std::string modal_title =
      absl::StrFormat("%s###review_board_image_viewer", tr("Image Viewer"));
  if (open_image_viewer_popup_) {
    ImGui::OpenPopup(modal_title.c_str());
    open_image_viewer_popup_ = false;
  }
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImVec2 vp_size = (viewport != nullptr && viewport->WorkSize.x > 0.0f &&
                          viewport->WorkSize.y > 0.0f)
                             ? viewport->WorkSize
                             : ImVec2(1280.0f, 800.0f);
  ImGui::SetNextWindowSize(
      ImVec2(std::floor(vp_size.x * 0.90f), std::floor(vp_size.y * 0.90f)),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(viewport != nullptr
                              ? viewport->GetWorkCenter()
                              : ImVec2(vp_size.x * 0.5f, vp_size.y * 0.5f),
                          ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(420.0f, 320.0f), ImVec2(std::max(420.0f, vp_size.x * 0.98f),
                                     std::max(320.0f, vp_size.y * 0.98f)));
  if (!ImGui::BeginPopupModal(modal_title.c_str(), &image_viewer_open_,
                              ImGuiWindowFlags_NoSavedSettings)) {
    viewer_dragging_ = false;
    return;
  }
  EnsureImagesLoaded(board);
  const int count = static_cast<int>(board.media.size());
  if (count <= 0) {
    viewer_dragging_ = false;
    ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return;
  }
  const int prev_index = viewer_media_index_;
  viewer_media_index_ = std::clamp(viewer_media_index_, 0, count - 1);
  if (viewer_media_index_ != prev_index) {
    viewer_state_ = ReviewBoardViewerState{};
    viewer_dragging_ = false;
  }
  const auto& media = board.media[static_cast<size_t>(viewer_media_index_)];
  const ImGuiIO& io = ImGui::GetIO();
  const ImVec2 fb =
      SanitizeReviewBoardFramebufferScale(io.DisplayFramebufferScale);
  const ImGuiStyle& style = ImGui::GetStyle();
  const float footer =
      media.caption.empty() ? 0.0f : ImGui::GetTextLineHeightWithSpacing() * 2;
  const size_t idx = static_cast<size_t>(viewer_media_index_);
  const bool has_valid_image = media.kind == "image" && idx < images_.size() &&
                               images_[idx] && images_[idx]->is_active() &&
                               images_[idx]->width() > 0 &&
                               images_[idx]->height() > 0;
  const ImVec2 image_size =
      has_valid_image ? ImVec2(static_cast<float>(images_[idx]->width()),
                               static_cast<float>(images_[idx]->height()))
                      : ImVec2(0.0f, 0.0f);

  const ImVec2 modal_win_size = ImGui::GetWindowSize();
  ImVec2 toolbar_canvas_avail = viewer_last_canvas_avail_;
  if (toolbar_canvas_avail.x <= 1.0f || toolbar_canvas_avail.y <= 1.0f ||
      std::abs(modal_win_size.x - viewer_last_modal_size_.x) > 0.5f ||
      std::abs(modal_win_size.y - viewer_last_modal_size_.y) > 0.5f) {
    const ImVec2 modal_avail = ImGui::GetContentRegionAvail();
    const float est_toolbar_h =
        ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y + 1.0f;
    toolbar_canvas_avail =
        ImVec2(std::max(1.0f, modal_avail.x - style.ChildBorderSize * 2.0f),
               std::max(1.0f, modal_avail.y - est_toolbar_h - footer -
                                  style.ChildBorderSize * 2.0f));
  }
  viewer_last_modal_size_ = modal_win_size;

  ImGui::BeginDisabled(viewer_media_index_ <= 0);
  if (gui::ThemedButton(ICON_MD_CHEVRON_LEFT)) {
    --viewer_media_index_;
    viewer_state_ = ReviewBoardViewerState{};
    viewer_dragging_ = false;
  }
  ImGui::EndDisabled();
  if (viewer_media_index_ <= 0 &&
      ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("%s", tr("Already at the first media item."));
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(viewer_media_index_ >= count - 1);
  if (gui::ThemedButton(ICON_MD_CHEVRON_RIGHT)) {
    ++viewer_media_index_;
    viewer_state_ = ReviewBoardViewerState{};
    viewer_dragging_ = false;
  }
  ImGui::EndDisabled();
  if (viewer_media_index_ >= count - 1 &&
      ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("%s", tr("Already at the last media item."));
  }

  std::optional<float> pending_toolbar_zoom;
  ImGui::SameLine();
  if (gui::ThemedButton(tr("Fit"))) {
    viewer_state_ = ReviewBoardViewerState{};
    viewer_dragging_ = false;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "%s",
        tr("Fit image in viewer (double-click canvas to toggle Fit / 100%)"));
  }
  ImGui::SameLine();
  if (gui::ThemedButton(tr("Actual pixels"))) {
    pending_toolbar_zoom = 1.0f;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tr("1 image pixel = 1 physical pixel (100%)"));
  }
  ImGui::SameLine();
  if (gui::ThemedButton(tr("1x"))) {
    pending_toolbar_zoom = 1.0f;
  }
  ImGui::SameLine();
  if (gui::ThemedButton(tr("2x"))) {
    pending_toolbar_zoom = 2.0f;
  }
  ImGui::SameLine();
  if (gui::ThemedButton(tr("4x"))) {
    pending_toolbar_zoom = 4.0f;
  }
  if (has_valid_image) {
    ReviewBoardViewerState label_state = viewer_state_;
    if (pending_toolbar_zoom.has_value()) {
      label_state.fit_mode = false;
      label_state.zoom = *pending_toolbar_zoom;
    }
    const ReviewBoardViewerLayout label_layout = ResolveReviewBoardViewerLayout(
        label_state, image_size, toolbar_canvas_avail, fb);
    ImGui::SameLine();
    if (label_state.fit_mode) {
      ImGui::TextDisabled("%d%% (%s)", label_layout.zoom_percent, tr("Fit"));
    } else {
      ImGui::TextDisabled("%d%%", label_layout.zoom_percent);
    }
  }
  const std::string info_text =
      has_valid_image
          ? absl::StrFormat("%d/%d  %s (%dx%d)", viewer_media_index_ + 1, count,
                            media.path.c_str(), images_[idx]->width(),
                            images_[idx]->height())
          : absl::StrFormat("%d/%d  %s", viewer_media_index_ + 1, count,
                            media.path.c_str());
  const float close_w =
      ImGui::CalcTextSize(tr("Close")).x + style.FramePadding.x * 2.0f;
  const float info_w = ImGui::CalcTextSize(info_text.c_str()).x;
  ImGui::SameLine();
  const float rem_after_zoom = ImGui::GetContentRegionAvail().x;
  if (rem_after_zoom >= info_w + style.ItemSpacing.x + close_w) {
    ImGui::TextDisabled("%s", info_text.c_str());
    ImGui::SameLine();
    const float right_offset = ImGui::GetContentRegionAvail().x - close_w;
    if (right_offset > 0.0f) {
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + right_offset);
    }
    if (gui::ThemedButton(tr("Close"))) {
      viewer_dragging_ = false;
      image_viewer_open_ = false;
      ImGui::CloseCurrentPopup();
    }
  } else {
    if (rem_after_zoom < close_w) {
      ImGui::NewLine();
    } else {
      const float right_offset = rem_after_zoom - close_w;
      if (right_offset > 0.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + right_offset);
      }
    }
    if (gui::ThemedButton(tr("Close"))) {
      viewer_dragging_ = false;
      image_viewer_open_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::TextDisabled("%s", info_text.c_str());
  }
  ImGui::Separator();

  if (ImGui::BeginChild(
          "##viewer_canvas", ImVec2(0.0f, -footer), ImGuiChildFlags_Borders,
          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    const ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    const ImVec2 raw_avail = ImGui::GetContentRegionAvail();
    const ImVec2 canvas_avail(std::max(1.0f, raw_avail.x),
                              std::max(1.0f, raw_avail.y));
    viewer_last_canvas_avail_ = canvas_avail;

    if (has_valid_image) {
      if (pending_toolbar_zoom.has_value()) {
        const ImVec2 center_screen(canvas_p0.x + canvas_avail.x * 0.5f,
                                   canvas_p0.y + canvas_avail.y * 0.5f);
        viewer_state_ = ZoomReviewBoardAroundScreenPos(
            viewer_state_, *pending_toolbar_zoom, center_screen, canvas_p0,
            image_size, canvas_avail, fb);
        viewer_dragging_ = false;
      }

      ImGui::InvisibleButton("##viewer_canvas_surface", canvas_avail,
                             ImGuiButtonFlags_MouseButtonLeft |
                                 ImGuiButtonFlags_MouseButtonMiddle);
      const bool canvas_hovered = ImGui::IsItemHovered();
      const bool canvas_active = ImGui::IsItemActive();

      if (!canvas_active) {
        viewer_dragging_ = false;
      }

      if (canvas_hovered && !io.WantTextInput &&
          ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        viewer_state_ = ToggleReviewBoardViewerFitOrActual(
            viewer_state_, io.MousePos, canvas_p0, image_size, canvas_avail,
            fb);
        viewer_dragging_ = false;
      } else {
        if (canvas_active && !viewer_dragging_) {
          const bool left_drag = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
          const bool mid_drag =
              !left_drag && ImGui::IsMouseDragging(ImGuiMouseButton_Middle);
          if (left_drag || mid_drag) {
            const ReviewBoardViewerLayout base = ResolveReviewBoardViewerLayout(
                viewer_state_, image_size, canvas_avail, fb);
            viewer_dragging_ = true;
            viewer_drag_button_ =
                left_drag ? ImGuiMouseButton_Left : ImGuiMouseButton_Middle;
            viewer_drag_start_pan_ = base.pan;
            viewer_state_.fit_mode = false;
            viewer_state_.zoom = base.physical_zoom;
            viewer_state_.pan = base.pan;
          }
        }
        if (viewer_dragging_) {
          if (!ImGui::IsMouseDown(viewer_drag_button_)) {
            viewer_dragging_ = false;
          } else {
            const ImVec2 delta =
                ImGui::GetMouseDragDelta(viewer_drag_button_, 0.0f);
            const ReviewBoardViewerLayout cur = ResolveReviewBoardViewerLayout(
                viewer_state_, image_size, canvas_avail, fb);
            viewer_state_.pan = ClampReviewBoardViewerPan(
                ImVec2(viewer_drag_start_pan_.x + delta.x,
                       viewer_drag_start_pan_.y + delta.y),
                cur.drawn_logical_size, canvas_avail);
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
          }
        }
      }

      if (ShouldHandleReviewBoardCanvasWheel(canvas_hovered, io.WantTextInput,
                                             io.MouseWheel)) {
        viewer_state_ =
            StepReviewBoardWheelZoom(viewer_state_, io.MouseWheel, io.MousePos,
                                     canvas_p0, image_size, canvas_avail, fb);
        if (viewer_dragging_) {
          const ImVec2 delta =
              ImGui::GetMouseDragDelta(viewer_drag_button_, 0.0f);
          viewer_drag_start_pan_ = ImVec2(viewer_state_.pan.x - delta.x,
                                          viewer_state_.pan.y - delta.y);
        }
      }

      const ReviewBoardViewerLayout layout = ResolveReviewBoardViewerLayout(
          viewer_state_, image_size, canvas_avail, fb);
      if (!viewer_state_.fit_mode) {
        viewer_state_.pan = layout.pan;
      }

      bool use_nearest = layout.use_nearest_sampling;
      gfx::TextureHandle tex = GetTextureForPhysicalSize(
          idx, layout.target_physical_width, layout.target_physical_height,
          &use_nearest);
      ImDrawList* draw = ImGui::GetWindowDrawList();
      draw->PushClipRect(
          canvas_p0,
          ImVec2(canvas_p0.x + canvas_avail.x, canvas_p0.y + canvas_avail.y),
          true);
      if (tex != nullptr) {
        const ImVec2 img_min(
            std::round((canvas_p0.x + layout.pan.x) * fb.x) / fb.x,
            std::round((canvas_p0.y + layout.pan.y) * fb.y) / fb.y);
        const ImVec2 img_max(img_min.x + layout.drawn_logical_size.x,
                             img_min.y + layout.drawn_logical_size.y);
        if (use_nearest) {
          gui::ScopedNearestSampling nearest(draw);
          draw->AddImage((ImTextureID)(intptr_t)tex, img_min, img_max);
        } else {
          draw->AddImage((ImTextureID)(intptr_t)tex, img_min, img_max);
        }
      } else {
        const std::string loading =
            absl::StrFormat("%s %s", ICON_MD_IMAGE, tr("Loading image..."));
        draw->AddText(ImVec2(canvas_p0.x + style.FramePadding.x,
                             canvas_p0.y + style.FramePadding.y),
                      ImGui::GetColorU32(ImGuiCol_TextDisabled),
                      loading.c_str());
      }
      draw->PopClipRect();
    } else if (idx < image_errors_.size() && !image_errors_[idx].empty()) {
      gui::StyleColorGuard err(ImGuiCol_Text, gui::GetErrorColor());
      ImGui::TextWrapped("%s %s", ICON_MD_BROKEN_IMAGE,
                         image_errors_[idx].c_str());
    } else if (media.kind != "image") {
      ImGui::TextDisabled("%s %s", ICON_MD_VIDEOCAM,
                          tr("Videos are listed, not played, in yaze."));
    } else {
      ImGui::TextDisabled("%s", tr("Image unavailable."));
    }
  }
  ImGui::EndChild();

  if (!media.caption.empty()) {
    ImGui::TextWrapped("%s", media.caption.c_str());
  }
  ImGui::EndPopup();
}

void ReviewBoardsView::Draw(bool* /*p_open*/) {
  if (!scanned_) {
    Rescan(/*reset_watcher=*/true);
  } else {
    PollIncrementalRescan();
  }

  DrawToolbar();
  ImGui::Separator();

  if (!dir_error_.empty()) {
    gui::EmptyStateOptions empty;
    empty.icon = ICON_MD_FOLDER_OPEN;
    empty.title = tr("Review boards folder unavailable");
    empty.detail = dir_error_.c_str();
    empty.action_label = tr("Retry");
    empty.on_action = [this]() {
      Rescan(/*reset_watcher=*/true);
    };
    gui::DrawEmptyState(empty);
    return;
  }
  if (scan_.boards.empty()) {
    const std::string detail = absl::StrFormat(
        "%s: %s",
        tr("Agents publish boards with Scripts/Review/publish_board.py. "
           "Folder"),
        dir_.string());
    gui::EmptyStateOptions empty;
    empty.icon = ICON_MD_RATE_REVIEW;
    empty.title = tr("No review boards yet");
    empty.detail = detail.c_str();
    empty.action_label = tr("Refresh");
    empty.on_action = [this]() {
      ForceRefresh();
    };
    gui::DrawEmptyState(empty);
    return;
  }

  const float list_width = std::max(260.0f, ImGui::GetFontSize() * 21.0f);
  if (ImGui::BeginChild("##boards_list", ImVec2(list_width, 0),
                        ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX)) {
    DrawList();
  }
  ImGui::EndChild();
  ImGui::SameLine();

  ImGui::BeginGroup();
  ReviewBoard* board = Selected();
  const ImGuiStyle& style = ImGui::GetStyle();
  const float reply_height =
      board ? style.ItemSpacing.y + 1.0f +
                  ImGui::GetFrameHeightWithSpacing() * 2.25f +
                  ImGui::GetTextLineHeight() * 2.6f +
                  style.FramePadding.y * 2.0f + style.ItemSpacing.y +
                  ImGui::GetTextLineHeightWithSpacing()
            : 0.0f;
  if (ImGui::BeginChild("##board_detail", ImVec2(0, -reply_height))) {
    if (board) {
      DrawDetail(*board);
    } else {
      gui::DrawEmptyState(gui::EmptyNoSelection());
    }
  }
  ImGui::EndChild();
  if (board) {
    DrawReply(*board);
    DrawImageViewerModal(*board);
  }
  ImGui::EndGroup();
}

void ReviewBoardsView::DrawToolbar() {
  int counts[5] = {};
  std::set<std::string> groups;
  std::set<std::string> authors;
  for (const auto& board : scan_.boards) {
    if (!board.group.empty())
      groups.insert(board.group);
    if (!board.author.empty())
      authors.insert(board.author);
    if (!MatchesReviewBoardQuery(board, search_query_, group_filter_,
                                 author_filter_)) {
      continue;
    }
    for (int f = 0; f < 5; ++f) {
      if (MatchesBoard(board, static_cast<ReviewBoardFilter>(f)))
        ++counts[f];
    }
  }

  if (gui::BeginThemedTabBar("##board_filter_tabs")) {
    for (int f = 0; f < 5; ++f) {
      const auto candidate = static_cast<ReviewBoardFilter>(f);
      const std::string tab_label =
          absl::StrFormat("%s %s (%d)###%s", kFilterIcons[f],
                          tr(kFilterLabels[f]), counts[f], kFilterTabIds[f]);
      ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
      if (force_select_filter_tab_ && filter_ == candidate) {
        flags |= ImGuiTabItemFlags_SetSelected;
      }
      if (ImGui::BeginTabItem(tab_label.c_str(), nullptr, flags)) {
        if (!force_select_filter_tab_ && filter_ != candidate) {
          filter_ = candidate;
          RefreshVisible({});
          if (!Selected() && !visible_.empty()) {
            Select(scan_.boards[visible_.front()].id);
          }
        }
        ImGui::EndTabItem();
      }
    }
    force_select_filter_tab_ = false;
    gui::EndThemedTabBar();
  }

  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14.0f);
  if (ImGui::InputTextWithHint("##board_search",
                               tr("Search title, ask, id, subject, group..."),
                               search_query_, sizeof(search_query_))) {
    RefreshVisible({});
  }
  if (search_query_[0] != '\0') {
    ImGui::SameLine();
    if (gui::ThemedIconButton(ICON_MD_CLEAR, tr("Clear search"))) {
      search_query_[0] = '\0';
      RefreshVisible({});
    }
  }

  ImGui::SameLine();
  const int cur_sort = static_cast<int>(sort_);
  const std::string sort_preview =
      absl::StrFormat("%s %s", ICON_MD_SORT, tr(kSortLabels[cur_sort]));
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.5f);
  if (ImGui::BeginCombo("##board_sort", sort_preview.c_str())) {
    for (int s = 0; s < 4; ++s) {
      if (ImGui::Selectable(tr(kSortLabels[s]), s == cur_sort)) {
        sort_ = static_cast<ReviewBoardSort>(s);
        RefreshVisible(selected_id_);
      }
    }
    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tr("Sort order for the review board list."));
  }

  ImGui::SameLine();
  const std::string group_preview =
      group_filter_.empty() ? std::string(tr("All groups")) : group_filter_;
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.0f);
  if (ImGui::BeginCombo("##board_group", group_preview.c_str())) {
    if (ImGui::Selectable(tr("All groups"), group_filter_.empty())) {
      group_filter_.clear();
      RefreshVisible({});
    }
    for (const auto& g : groups) {
      if (ImGui::Selectable(g.c_str(), group_filter_ == g)) {
        group_filter_ = g;
        RefreshVisible({});
      }
    }
    ImGui::EndCombo();
  }

  ImGui::SameLine();
  const std::string author_preview =
      author_filter_.empty() ? std::string(tr("All authors")) : author_filter_;
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.5f);
  if (ImGui::BeginCombo("##board_author", author_preview.c_str())) {
    if (ImGui::Selectable(tr("All authors"), author_filter_.empty())) {
      author_filter_.clear();
      RefreshVisible({});
    }
    for (const auto& a : authors) {
      if (ImGui::Selectable(a.c_str(), author_filter_ == a)) {
        author_filter_ = a;
        RefreshVisible({});
      }
    }
    ImGui::EndCombo();
  }

  ImGui::SameLine();
  if (gui::ThemedIconButton(ICON_MD_SKIP_NEXT,
                            tr("Jump to next board waiting on you"))) {
    SelectNextWaiting();
  }
  ImGui::SameLine();
  if (gui::ThemedIconButton(ICON_MD_REFRESH,
                            tr("Read the boards folder again"))) {
    ForceRefresh();
  }

  if (!scan_.errors.empty()) {
    ImGui::SameLine();
    gui::StyleColorGuard warn(ImGuiCol_Text, gui::GetWarningColor());
    ImGui::Text("%s %d %s", ICON_MD_WARNING,
                static_cast<int>(scan_.errors.size()), tr("skipped"));
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      for (size_t i = 0; i < scan_.errors.size() && i < 12; ++i)
        ImGui::TextUnformatted(scan_.errors[i].c_str());
      if (scan_.errors.size() > 12) {
        ImGui::Text("...%s (%d)", tr("more"),
                    static_cast<int>(scan_.errors.size() - 12));
      }
      ImGui::EndTooltip();
    }
  }
}

void ReviewBoardsView::DrawList() {
  if (visible_.empty()) {
    if (filter_ == ReviewBoardFilter::kWaitingOnYou &&
        search_query_[0] == '\0' && group_filter_.empty() &&
        author_filter_.empty()) {
      gui::StyleColorGuard done(ImGuiCol_Text, gui::GetSuccessColor());
      ImGui::TextWrapped("%s %s", ICON_MD_TASK_ALT,
                         tr("Every current board waiting on you is answered."));
    } else {
      ImGui::TextDisabled("%s", tr("No boards match the active filters."));
    }
    return;
  }

  const absl::Time now = absl::Now();
  const ImGuiStyle& style = ImGui::GetStyle();
  const float line_h = ImGui::GetTextLineHeight();
  const float row_h = line_h * 2.15f + style.FramePadding.y * 2.0f;
  const float icon_col_w = ImGui::GetFontSize() * 1.3f;
  ImDrawList* draw = ImGui::GetWindowDrawList();

  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(visible_.size()), row_h);
  while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
      const ReviewBoard& board = scan_.boards[visible_[row]];
      const StateLook look = BoardLook(board);
      const bool unsaved = HasUnsavedDraft(board);
      const bool is_new = watcher_state_.new_ids.contains(board.id);

      const ReviewBoardAnswerMap* active_answers =
          DraftAnswersFor(board.id) != nullptr ? DraftAnswersFor(board.id)
                                               : &board.latest_answers;
      const int total_q = static_cast<int>(board.questions.size());
      const int answered_q =
          total_q > 0 ? CountAnsweredQuestions(board, *active_answers) : 0;
      const std::string age = FormatReviewBoardAge(board.created_at, now);

      std::string right_meta;
      if (total_q > 0)
        right_meta = absl::StrFormat("%d/%d", answered_q, total_q);
      if (!age.empty()) {
        if (!right_meta.empty())
          right_meta += "  ";
        right_meta += age;
      }

      ImGui::PushID(board.id.c_str());
      const ImVec2 pos = ImGui::GetCursorScreenPos();
      const float avail_w = ImGui::GetContentRegionAvail().x;
      if (ImGui::Selectable("##board_row", board.id == selected_id_,
                            ImGuiSelectableFlags_None,
                            ImVec2(avail_w, row_h - style.ItemSpacing.y))) {
        Select(board.id);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s %s  [%s]", look.icon, tr(look.label),
                    board.priority.c_str());
        ImGui::TextDisabled("%s", board.id.c_str());
        if (!board.ask.empty())
          ImGui::TextUnformatted(board.ask.c_str());
        if (!board.group.empty())
          ImGui::TextDisabled("%s: %s", tr("Group"), board.group.c_str());
        ImGui::EndTooltip();
      }

      const ImVec2 row_max(pos.x + avail_w - style.FramePadding.x,
                           pos.y + row_h);
      draw->PushClipRect(pos, row_max, true);

      // Status icon + priority indicator + unsaved dot.
      float x = pos.x + style.FramePadding.x;
      const float y1 = pos.y + style.FramePadding.y;
      draw->AddText(ImVec2(x, y1), ImGui::GetColorU32(look.color), look.icon);
      x += icon_col_w;

      if (board.priority == "high") {
        draw->AddText(ImVec2(x, y1), ImGui::GetColorU32(gui::GetErrorColor()),
                      ICON_MD_PRIORITY_HIGH);
        x += ImGui::GetFontSize() * 1.0f;
      } else if (board.priority == "low") {
        draw->AddText(ImVec2(x, y1),
                      ImGui::GetColorU32(gui::GetTextDisabledVec4()),
                      ICON_MD_ARROW_DOWNWARD);
        x += ImGui::GetFontSize() * 1.0f;
      }

      if (unsaved) {
        draw->AddText(ImVec2(x, y1), ImGui::GetColorU32(gui::GetWarningColor()),
                      ICON_MD_FIBER_MANUAL_RECORD);
        x += ImGui::GetFontSize() * 0.95f;
      }

      if (is_new) {
        const std::string new_tag = absl::StrFormat("[%s] ", tr("new"));
        draw->AddText(ImVec2(x, y1), ImGui::GetColorU32(gui::GetAccentColor()),
                      new_tag.c_str());
        x += ImGui::CalcTextSize(new_tag.c_str()).x;
      }

      const float right_w =
          right_meta.empty()
              ? 0.0f
              : ImGui::CalcTextSize(right_meta.c_str()).x + style.ItemSpacing.x;
      const ImVec2 title_clip_max(row_max.x - right_w, y1 + line_h);
      draw->PushClipRect(ImVec2(x, y1), title_clip_max, true);
      const std::string& title = board.title.empty() ? board.id : board.title;
      draw->AddText(
          ImVec2(x, y1),
          ImGui::GetColorU32(board.status == "current" ? ImGuiCol_Text
                                                       : ImGuiCol_TextDisabled),
          title.c_str());
      draw->PopClipRect();

      if (!right_meta.empty()) {
        const float rx = row_max.x - ImGui::CalcTextSize(right_meta.c_str()).x;
        draw->AddText(ImVec2(rx, y1), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                      right_meta.c_str());
      }

      // Second line: ask (or subject) + group.
      const float y2 = y1 + line_h + 2.0f;
      std::string subtitle = board.ask.empty() ? SubjectLine(board) : board.ask;
      if (!board.group.empty()) {
        if (!subtitle.empty())
          subtitle += "  [" + board.group + "]";
        else
          subtitle = "[" + board.group + "]";
      }
      if (!subtitle.empty()) {
        draw->AddText(ImVec2(pos.x + style.FramePadding.x + icon_col_w, y2),
                      ImGui::GetColorU32(ImGuiCol_TextDisabled),
                      subtitle.c_str());
      }

      draw->PopClipRect();
      ImGui::PopID();
    }
  }
}

void ReviewBoardsView::DrawDetail(ReviewBoard& board) {
  EnsureImagesLoaded(board);
  BoardDraft& draft = EnsureDraft(board);

  // Title + priority badge.
  if (board.priority == "high") {
    gui::StyleColorGuard hi(ImGuiCol_Text, gui::GetErrorColor());
    ImGui::Text("%s [%s]", ICON_MD_PRIORITY_HIGH, tr("high"));
    ImGui::SameLine();
  } else if (board.priority == "low") {
    ImGui::TextDisabled("[%s]", tr("low"));
    ImGui::SameLine();
  }
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(board.title.empty() ? board.id.c_str()
                                             : board.title.c_str());
  ImGui::PopTextWrapPos();

  const std::string subject = SubjectLine(board);
  if (!subject.empty()) {
    ImGui::TextDisabled("%s %s", ICON_MD_PLACE, subject.c_str());
  }
  ImGui::TextDisabled("%s  %s  %s", board.author.c_str(),
                      board.created_at.c_str(), board.group.c_str());

  // Board path + Copy button.
  if (!board.folder.empty()) {
    if (ImGui::SmallButton(absl::StrFormat("%s %s###copy_board_path",
                                           ICON_MD_CONTENT_COPY,
                                           tr("Copy path"))
                               .c_str())) {
      ImGui::SetClipboardText(board.folder.c_str());
      NotifyToast(tr("Copied board folder path to clipboard"),
                  /*is_error=*/false);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", board.folder.c_str());
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", board.folder.c_str());
  }

  if (board.status != "current") {
    gui::StyleColorGuard warn(ImGuiCol_Text, gui::GetWarningColor());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::Text("%s %s%s%s", ICON_MD_HISTORY, board.status.c_str(),
                board.status_reason.empty() ? "" : ": ",
                board.status_reason.c_str());
    ImGui::PopTextWrapPos();
  }

  auto link_row = [this](const char* label,
                         const std::vector<std::string>& ids) {
    if (ids.empty())
      return;
    ImGui::TextDisabled("%s", label);
    for (const auto& id : ids) {
      ImGui::SameLine();
      if (ImGui::SmallButton(id.c_str())) {
        filter_ = ReviewBoardFilter::kAll;
        force_select_filter_tab_ = true;
        RefreshVisible({});
        Select(id);
      }
    }
  };
  link_row(tr("Replaces:"), board.supersedes);
  link_row(tr("Replaced by:"), board.superseded_by);

  if (!board.feedback_error.empty()) {
    gui::StyleColorGuard error(ImGuiCol_Text, gui::GetErrorColor());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::Text("%s %s", ICON_MD_ERROR, board.feedback_error.c_str());
    ImGui::PopTextWrapPos();
  }

  for (const auto& warning : board.question_warnings) {
    gui::StyleColorGuard warn(ImGuiCol_Text, gui::GetWarningColor());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::Text("%s %s", ICON_MD_WARNING, warning.c_str());
    ImGui::PopTextWrapPos();
  }

  if (draft.stale_after_republish) {
    {
      gui::StyleColorGuard warn(ImGuiCol_Text, gui::GetWarningColor());
      ImGui::PushTextWrapPos(0.0f);
      ImGui::Text(
          "%s %s", ICON_MD_WARNING,
          tr("This board was updated on disk while you had unsaved draft "
             "answers."));
      ImGui::PopTextWrapPos();
    }
    if (ImGui::SmallButton(tr("Reload from board"))) {
      ReloadDraftFromBoard(board);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(tr("Keep my edits"))) {
      KeepDraftEditsOnUpdatedBoard(board);
    }
  }

  ImGui::Separator();

  // Detail order per SPEC.md #6: ask first, then questions, then media, then
  // why / details / comments.
  if (!board.ask.empty()) {
    {
      gui::StyleColorGuard ask_col(ImGuiCol_Text, gui::GetAccentColor());
      ImGui::PushTextWrapPos(0.0f);
      ImGui::Text("%s %s", ICON_MD_HELP_OUTLINE, board.ask.c_str());
      ImGui::PopTextWrapPos();
    }
    ImGui::Spacing();
  }

  if (!board.questions.empty()) {
    DrawQuestions(board, draft);
    ImGui::Spacing();
  }

  DrawMedia(board);

  if (!board.why.empty()) {
    ImGui::SeparatorText(tr("Context"));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(board.why.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
  }

  if (!board.details.empty() &&
      ImGui::CollapsingHeader(tr("Details"), ImGuiTreeNodeFlags_None)) {
    for (const auto& [label, text] : board.details) {
      ImGui::TextDisabled("%s", label.c_str());
      ImGui::SameLine();
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextUnformatted(text.c_str());
      ImGui::PopTextWrapPos();
    }
  }

  DrawConversation(board);
}

void ReviewBoardsView::DrawOptionThumbnail(const ReviewBoard& board,
                                           int media_index) {
  if (media_index < 0 ||
      static_cast<size_t>(media_index) >= board.media.size() ||
      static_cast<size_t>(media_index) >= images_.size()) {
    return;
  }
  const size_t idx = static_cast<size_t>(media_index);
  if (!images_[idx] || !images_[idx]->is_active())
    return;
  const float w = static_cast<float>(images_[idx]->width());
  const float h = static_cast<float>(images_[idx]->height());
  if (w <= 0.0f || h <= 0.0f)
    return;
  const ImVec2 fb = SanitizeReviewBoardFramebufferScale(
      ImGui::GetIO().DisplayFramebufferScale);
  const float max_side = ImGui::GetFontSize() * 8.5f;
  const float scale = std::min(max_side / w, max_side / h);
  const int phys_w =
      std::max(1, static_cast<int>(std::lround(w * scale * fb.x)));
  const int phys_h =
      std::max(1, static_cast<int>(std::lround(h * scale * fb.y)));
  bool use_nearest = false;
  gfx::TextureHandle tex =
      GetTextureForPhysicalSize(idx, phys_w, phys_h, &use_nearest);
  if (tex == nullptr)
    return;
  const ImVec2 thumb_size(static_cast<float>(phys_w) / fb.x,
                          static_cast<float>(phys_h) / fb.y);
  bool clicked = false;
  if (use_nearest) {
    gui::ScopedNearestSampling nearest(ImGui::GetWindowDrawList());
    clicked = ImGui::ImageButton("##opt_thumb", (ImTextureID)(intptr_t)tex,
                                 thumb_size);
  } else {
    clicked = ImGui::ImageButton("##opt_thumb", (ImTextureID)(intptr_t)tex,
                                 thumb_size);
  }
  if (clicked) {
    OpenImageViewer(media_index);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s (%s)", tr("Click to open full image"),
                      board.media[idx].path.c_str());
  }
}

void ReviewBoardsView::DrawQuestions(const ReviewBoard& board,
                                     BoardDraft& draft) {
  ImGui::SeparatorText(tr("Questions"));
  for (size_t qi = 0; qi < board.questions.size(); ++qi) {
    const ReviewBoardQuestion& q = board.questions[qi];
    const bool answered = IsReviewBoardQuestionAnswered(q, draft.answers);
    ImGui::PushID(q.id.c_str());

    // Question prompt + required / answered status.
    ImGui::PushTextWrapPos(0.0f);
    ImGui::Text("%d. %s", static_cast<int>(qi + 1), q.prompt.c_str());
    ImGui::PopTextWrapPos();
    if (q.required) {
      ImGui::SameLine();
      gui::StyleColorGuard req_col(ImGuiCol_Text, answered
                                                      ? gui::GetSuccessColor()
                                                      : gui::GetWarningColor());
      ImGui::Text("[%s]", answered ? tr("answered") : tr("required"));
    }

    ReviewBoardAnswer& ans = draft.answers[q.id];

    if (q.kind == "single") {
      std::string cur_opt;
      if (std::holds_alternative<std::string>(ans.value))
        cur_opt = std::get<std::string>(ans.value);
      bool& other_sel = draft.single_other_selected[q.id];

      for (const auto& opt : q.options) {
        ImGui::PushID(opt.id.c_str());
        const bool selected = !other_sel && (cur_opt == opt.id);
        if (ImGui::RadioButton(opt.label.c_str(), selected)) {
          ans.value = opt.id;
          other_sel = false;
        }
        if (opt.recommended) {
          ImGui::SameLine();
          gui::StyleColorGuard pick_col(ImGuiCol_Text, gui::GetSuccessColor());
          ImGui::Text("[%s]", tr("pick"));
        }
        if (!opt.detail.empty()) {
          ImGui::SameLine();
          ImGui::TextDisabled("- %s", opt.detail.c_str());
        }
        if (opt.media.has_value()) {
          ImGui::Indent();
          DrawOptionThumbnail(board, *opt.media);
          ImGui::Unindent();
        }
        ImGui::PopID();
      }
      if (q.allow_other) {
        if (ImGui::RadioButton(tr("Other"), other_sel)) {
          other_sel = true;
          ans.value = std::string{};
        }
        ImGui::SameLine();
        char other_buf[512] = {};
        std::snprintf(other_buf, sizeof(other_buf), "%s", ans.other.c_str());
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 18.0f);
        if (ImGui::InputTextWithHint("##single_other", tr("Specify other..."),
                                     other_buf, sizeof(other_buf))) {
          ans.other = other_buf;
          if (!absl::StripAsciiWhitespace(ans.other).empty()) {
            other_sel = true;
            ans.value = std::string{};
          }
        }
      }
    } else if (q.kind == "multi") {
      std::vector<std::string> selected_ids;
      if (std::holds_alternative<std::vector<std::string>>(ans.value)) {
        selected_ids = std::get<std::vector<std::string>>(ans.value);
      }
      for (const auto& opt : q.options) {
        ImGui::PushID(opt.id.c_str());
        bool checked = std::find(selected_ids.begin(), selected_ids.end(),
                                 opt.id) != selected_ids.end();
        if (ImGui::Checkbox(opt.label.c_str(), &checked)) {
          if (checked) {
            selected_ids.push_back(opt.id);
          } else {
            selected_ids.erase(
                std::remove(selected_ids.begin(), selected_ids.end(), opt.id),
                selected_ids.end());
          }
          ans.value = selected_ids;
        }
        if (opt.recommended) {
          ImGui::SameLine();
          gui::StyleColorGuard pick_col(ImGuiCol_Text, gui::GetSuccessColor());
          ImGui::Text("[%s]", tr("pick"));
        }
        if (!opt.detail.empty()) {
          ImGui::SameLine();
          ImGui::TextDisabled("- %s", opt.detail.c_str());
        }
        if (opt.media.has_value()) {
          ImGui::Indent();
          DrawOptionThumbnail(board, *opt.media);
          ImGui::Unindent();
        }
        ImGui::PopID();
      }
      if (q.allow_other) {
        char other_buf[512] = {};
        std::snprintf(other_buf, sizeof(other_buf), "%s", ans.other.c_str());
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 20.0f);
        if (ImGui::InputTextWithHint("##multi_other", tr("Other (optional)..."),
                                     other_buf, sizeof(other_buf))) {
          ans.other = other_buf;
          if (!std::holds_alternative<std::vector<std::string>>(ans.value)) {
            ans.value = selected_ids;
          }
        }
      }
    } else if (q.kind == "text") {
      char text_buf[1024] = {};
      if (std::holds_alternative<std::string>(ans.value)) {
        std::snprintf(text_buf, sizeof(text_buf), "%s",
                      std::get<std::string>(ans.value).c_str());
      }
      const char* hint = q.placeholder.empty() ? tr("Enter response...")
                                               : q.placeholder.c_str();
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::InputTextWithHint("##text_answer", hint, text_buf,
                                   sizeof(text_buf))) {
        ans.value = std::string(text_buf);
      }
    } else if (q.kind == "number") {
      std::string& num_str = draft.number_inputs[q.id];
      char num_buf[128] = {};
      std::snprintf(num_buf, sizeof(num_buf), "%s", num_str.c_str());
      std::string range_hint = q.placeholder;
      if (range_hint.empty()) {
        if (q.min.has_value() && q.max.has_value()) {
          range_hint = absl::StrFormat("%s .. %s", FormatDoubleInput(*q.min),
                                       FormatDoubleInput(*q.max));
        } else {
          range_hint = tr("Enter number...");
        }
      }
      ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
      if (ImGui::InputTextWithHint("##number_answer", range_hint.c_str(),
                                   num_buf, sizeof(num_buf))) {
        num_str = num_buf;
        double parsed = 0.0;
        if (ParseDoubleStrict(num_str, &parsed)) {
          ans.value = parsed;
        } else {
          ans.value = std::monostate{};
        }
      }
      if (q.min.has_value() || q.max.has_value()) {
        ImGui::SameLine();
        ImGui::TextDisabled(
            "[%s..%s]",
            q.min.has_value() ? FormatDoubleInput(*q.min).c_str() : "",
            q.max.has_value() ? FormatDoubleInput(*q.max).c_str() : "");
      }
      if (!num_str.empty() &&
          !IsReviewBoardQuestionAnswered(q, draft.answers)) {
        ImGui::SameLine();
        gui::StyleColorGuard err(ImGuiCol_Text, gui::GetErrorColor());
        ImGui::TextUnformatted(tr("Out of range or invalid number"));
      }
    } else if (q.kind == "yes_no") {
      const bool has_bool = std::holds_alternative<bool>(ans.value);
      const bool val = has_bool ? std::get<bool>(ans.value) : false;
      if (ImGui::RadioButton(tr("Yes"), has_bool && val)) {
        ans.value = true;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton(tr("No"), has_bool && !val)) {
        ans.value = false;
      }
    }

    ImGui::Spacing();
    ImGui::PopID();
  }
}

void ReviewBoardsView::DrawMedia(const ReviewBoard& board) {
  EnsureImagesLoaded(board);
  if (board.media.empty())
    return;

  ImGui::SeparatorText(tr("Media"));
  ImGui::Checkbox(tr("Fit width"), &fit_width_);
  const ImVec2 fb = SanitizeReviewBoardFramebufferScale(
      ImGui::GetIO().DisplayFramebufferScale);
  for (size_t i = 0; i < board.media.size(); ++i) {
    const auto& media = board.media[i];
    ImGui::PushID(static_cast<int>(i));
    if (media.kind != "image") {
      ImGui::TextDisabled("%s %s", ICON_MD_VIDEOCAM, media.path.c_str());
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("Videos are listed, not played, in yaze."));
    } else if (!image_errors_[i].empty()) {
      gui::StyleColorGuard error(ImGuiCol_Text, gui::GetErrorColor());
      ImGui::PushTextWrapPos(0.0f);
      ImGui::Text("%s %s: %s", ICON_MD_BROKEN_IMAGE, media.path.c_str(),
                  image_errors_[i].c_str());
      ImGui::PopTextWrapPos();
    } else if (images_[i] && images_[i]->is_active()) {
      const float w = static_cast<float>(images_[i]->width());
      const float h = static_cast<float>(images_[i]->height());
      float physical_zoom = 1.0f;
      if (fit_width_ && w > 0.0f) {
        const float avail_phys_w = std::max(
            1.0f, std::floor(std::max(1.0f, ImGui::GetContentRegionAvail().x) *
                             fb.x));
        physical_zoom =
            std::clamp(avail_phys_w / w, kReviewBoardViewerMinZoom, 4.0f);
      }
      const int phys_w =
          std::max(1, static_cast<int>(std::lround(w * physical_zoom)));
      const int phys_h =
          std::max(1, static_cast<int>(std::lround(h * physical_zoom)));
      bool use_nearest = false;
      gfx::TextureHandle tex =
          GetTextureForPhysicalSize(i, phys_w, phys_h, &use_nearest);
      if (tex != nullptr) {
        const ImVec2 size(static_cast<float>(phys_w) / fb.x,
                          static_cast<float>(phys_h) / fb.y);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        if (use_nearest) {
          gui::ScopedNearestSampling nearest(ImGui::GetWindowDrawList());
          ImGui::Image((ImTextureID)(intptr_t)tex, size);
        } else {
          ImGui::Image((ImTextureID)(intptr_t)tex, size);
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", tr("Click to inspect in full image viewer"));
          if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            OpenImageViewer(static_cast<int>(i));
          }
        }
        if (i == 0) {
          ImDrawList* draw = ImGui::GetWindowDrawList();
          const ImU32 fill = ImGui::GetColorU32(gui::GetAccentColor());
          const ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
          const ImU32 back = ImGui::GetColorU32(ImGuiCol_WindowBg);
          const float radius = ImGui::GetFontSize() * 0.6f;
          int number = 0;
          for (const auto& comment : board.comments) {
            ++number;
            if (!comment.has_pin)
              continue;
            const ImVec2 at(origin.x + comment.pin_x * size.x,
                            origin.y + comment.pin_y * size.y);
            draw->AddCircleFilled(at, radius + 1.5f, back);
            draw->AddCircleFilled(at, radius, fill);
            const std::string n = std::to_string(number);
            const ImVec2 ts = ImGui::CalcTextSize(n.c_str());
            draw->AddText(ImVec2(at.x - ts.x * 0.5f, at.y - ts.y * 0.5f), text,
                          n.c_str());
          }
        }
      } else {
        ImGui::TextDisabled("%s %s", ICON_MD_IMAGE, tr("Loading image..."));
      }
    } else {
      ImGui::TextDisabled("%s %s", ICON_MD_IMAGE, tr("Loading image..."));
    }
    if (!media.caption.empty()) {
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextDisabled("%s", media.caption.c_str());
      ImGui::PopTextWrapPos();
    }
    ImGui::Spacing();
    ImGui::PopID();
  }
}

void ReviewBoardsView::DrawConversation(const ReviewBoard& board) {
  if (board.comments.empty() && board.feedback.empty())
    return;
  ImGui::SeparatorText(tr("Comments & History"));
  int number = 0;
  for (const auto& comment : board.comments) {
    ++number;
    if (comment.has_pin) {
      ImGui::TextDisabled("%s%d", ICON_MD_PLACE, number);
    } else {
      ImGui::TextDisabled("%s", ICON_MD_SPEAKER_NOTES);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s %s", comment.author.c_str(), comment.at.c_str());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(comment.text.c_str());
    ImGui::PopTextWrapPos();
  }
  for (const auto& entry : board.feedback) {
    const StateLook look = DecisionLook(entry.decision);
    {
      gui::StyleColorGuard color(ImGuiCol_Text, look.color);
      ImGui::Text("%s %s", look.icon, tr(look.label));
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s  %s", entry.category.c_str(), entry.at_utc.c_str());
    for (const auto& [qid, ans] : entry.answers) {
      const std::string summary = FormatReviewBoardAnswerSummary(ans);
      if (!summary.empty()) {
        ImGui::TextDisabled("  %s: %s", qid.c_str(), summary.c_str());
      }
    }
    if (!entry.comment.empty()) {
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextUnformatted(entry.comment.c_str());
      ImGui::PopTextWrapPos();
    }
  }
}

void ReviewBoardsView::DrawReply(ReviewBoard& board) {
  BoardDraft& draft = EnsureDraft(board);

  ImGui::Separator();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(tr("Decision:"));
  ImGui::SameLine();
  if (ImGui::RadioButton(tr("Looks good##dec"),
                         draft.decision == "looks_good")) {
    draft.decision = "looks_good";
  }
  ImGui::SameLine();
  if (ImGui::RadioButton(tr("Needs changes##dec"),
                         draft.decision == "needs_changes")) {
    draft.decision = "needs_changes";
  }
  ImGui::SameLine();
  if (ImGui::RadioButton(tr("Comment##dec"), draft.decision == "comment")) {
    draft.decision = "comment";
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.0f);
  ImGui::Combo("##board_kind", &draft.category, kCategories, 2);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tr("Kind of feedback: general or layout."));
  }

  const std::string last = ReviewBoardDecision(board);
  if (!last.empty()) {
    const StateLook look = DecisionLook(last);
    ImGui::SameLine();
    gui::StyleColorGuard color(ImGuiCol_Text, look.color);
    ImGui::Text("%s %s: %s", look.icon, tr("Saved"), tr(look.label));
  }

  ImGui::InputTextMultiline(
      "##board_comment", draft.comment, sizeof(draft.comment),
      ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 2.4f));

  const ReviewBoardQuestion* missing_q =
      FirstUnansweredRequiredQuestion(board, draft.answers);
  const bool has_comment = !absl::StripAsciiWhitespace(draft.comment).empty();
  const bool needs_comment =
      draft.decision != "looks_good" && !has_comment && board.questions.empty();
  const bool can_submit = (missing_q == nullptr) && !needs_comment;

  ImGui::BeginDisabled(!can_submit);
  if (gui::SuccessButton(
          absl::StrFormat("%s %s###submit_feedback", ICON_MD_SEND, tr("Submit"))
              .c_str())) {
    SubmitFeedback(board, draft);
  }
  ImGui::EndDisabled();
  if (!can_submit &&
      ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (missing_q != nullptr) {
      ImGui::SetTooltip("%s: %s", tr("Answer required question first"),
                        missing_q->prompt.c_str());
    } else {
      ImGui::SetTooltip("%s", tr("Write a comment first."));
    }
  }

  if (HasUnsavedDraft(board)) {
    ImGui::SameLine();
    gui::StyleColorGuard unsaved_col(ImGuiCol_Text, gui::GetWarningColor());
    ImGui::Text("%s %s", ICON_MD_FIBER_MANUAL_RECORD, tr("Unsaved edits"));
  }

  if (!status_.empty() && status_for_ == board.id) {
    gui::StyleColorGuard color(ImGuiCol_Text, status_is_error_
                                                  ? gui::GetErrorColor()
                                                  : gui::GetSuccessColor());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::Text("%s %s", status_is_error_ ? ICON_MD_ERROR : ICON_MD_CHECK,
                status_.c_str());
    ImGui::PopTextWrapPos();
  }
}

void ReviewBoardsView::SubmitFeedback(ReviewBoard& board, BoardDraft& draft) {
  ReviewBoardFeedbackEntry entry;
  entry.decision = draft.decision;
  entry.category = kCategories[draft.category];
  entry.comment = draft.comment;
  entry.at_utc =
      absl::FormatTime("%Y-%m-%dT%H:%M:%SZ", absl::Now(), absl::UTCTimeZone());
  entry.answers = board.questions.empty() ? ReviewBoardAnswerMap{}
                                          : FilterReviewBoardAnswersToQuestions(
                                                board, draft.answers);

  const std::string id = board.id;
  const absl::Status status = WriteReviewBoardFeedback(board, entry);
  if (!status.ok()) {
    status_for_ = id;
    status_ = std::string(status.message());
    status_is_error_ = true;
    NotifyToast(status_, /*is_error=*/true);
    return;
  }

  // Update watcher stamp for feedback-<id>.json so the 3 s poller sees our own
  // write as already accounted for.
  std::error_code ec;
  const auto fb_path = ReviewBoardFeedbackPath(board);
  if (auto it = watcher_state_.stamps.find(id);
      it != watcher_state_.stamps.end() &&
      std::filesystem::exists(fb_path, ec)) {
    it->second.has_feedback = true;
    it->second.feedback_mtime = std::filesystem::last_write_time(fb_path, ec);
  }

  draft.comment[0] = '\0';
  draft.answers = entry.answers;
  for (const auto& q : board.questions) {
    (void)draft.answers[q.id];
  }
  draft.baseline_answers = draft.answers;
  draft.baseline_decision = draft.decision;
  draft.draft_board_sha256 = board.board_sha256;
  draft.stale_after_republish = false;

  const std::string saved = absl::StrFormat(
      "%s \"%s\" -> %s", tr("Submitted"),
      tr(DecisionLook(entry.decision).label), fb_path.filename().string());
  NotifyToast(saved, /*is_error=*/false);

  RefreshVisible(id);
  status_for_ = id;
  status_ = saved;
  status_is_error_ = false;
}

REGISTER_PANEL(ReviewBoardsView);

}  // namespace yaze::editor
