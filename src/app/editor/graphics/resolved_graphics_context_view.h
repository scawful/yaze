#ifndef YAZE_APP_EDITOR_GRAPHICS_RESOLVED_GRAPHICS_CONTEXT_VIEW_H_
#define YAZE_APP_EDITOR_GRAPHICS_RESOLVED_GRAPHICS_CONTEXT_VIEW_H_

#include "absl/strings/str_format.h"
#include "app/editor/editor_context_snapshot.h"
#include "zelda3/graphics/resolved_graphics_context.h"

namespace yaze::editor {

// Copies one resolved graphics context into the inspector snapshot.
inline void AppendResolvedGraphicsContext(
    EditorContextSnapshot& snapshot,
    const zelda3::ResolvedGraphicsContext& context) {
  if (!context.graphics_loaded) {
    snapshot.diagnostics.push_back({
        .id = "gfx.not_loaded",
        .severity = EditorContextDiagnosticSeverity::kInfo,
        .message = "Graphics slots are not loaded for this selection yet.",
    });
    return;
  }
  snapshot.metadata.push_back(
      {.id = "gfx.project", .label = "Project", .value = context.project_name});
  snapshot.metadata.push_back({.id = "gfx.rom_sha256",
                               .label = "ROM SHA-256",
                               .value = context.rom_sha256});
  snapshot.metadata.push_back(
      {.id = "gfx.revision",
       .label = "Graphics revision",
       .value = std::to_string(context.graphics_revision)});
  snapshot.metadata.push_back({.id = "gfx.fingerprint",
                               .label = "Graphics fingerprint",
                               .value = context.graphics_fingerprint});
  snapshot.metadata.push_back({.id = "gfx.terrain_catalog",
                               .label = "Terrain catalog",
                               .value = context.terrain_catalog});
  for (const auto& slot : context.slots) {
    snapshot.metadata.push_back({
        .id = absl::StrFormat("gfx.slot.%02d", slot.slot),
        .label = slot.role,
        .value = absl::StrFormat("0x%02X %s", slot.sheet_id, slot.source),
    });
  }
  for (const auto& palette : context.palettes) {
    snapshot.metadata.push_back({
        .id = absl::StrFormat("gfx.palette.%s", palette.name),
        .label = palette.name,
        .value = absl::StrFormat("0x%X %s%s", palette.selector, palette.source,
                                 palette.inherited ? " inherited" : ""),
    });
  }
  for (const auto& alternate : context.alternates) {
    snapshot.metadata.push_back({
        .id = absl::StrFormat("gfx.alternate.%s", alternate.name),
        .label = alternate.name,
        .value = alternate.state,
    });
  }
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_RESOLVED_GRAPHICS_CONTEXT_VIEW_H_
