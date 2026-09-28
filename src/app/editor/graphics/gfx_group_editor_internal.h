#ifndef YAZE_APP_EDITOR_GRAPHICS_GFX_GROUP_EDITOR_INTERNAL_H_
#define YAZE_APP_EDITOR_GRAPHICS_GFX_GROUP_EDITOR_INTERNAL_H_

#include <string>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/sheet_role.h"
#include "app/gfx/types/sheet_role_palette_table.h"
#include "app/gfx/types/snes_palette.h"

namespace yaze {
namespace editor {
namespace internal {

// Ensure a graphics sheet bitmap is ready for canvas rendering.
//
// Sheets are seeded by GameData::LoadGraphics into Arena::gfx_sheets_, but
// nothing in the GfxGroupEditor draw path queues a CREATE for the underlying
// SDL texture. RenderBitmapOnCanvas (canvas_rendering.cc) silently returns
// when !is_active() || !texture(), so sheets render blank until something
// else (e.g. opening Graphics Editor) seeds the textures.
//
// Behavior:
//   * Bitmap with no surface         -> no-op.
//   * Surface present, !is_active()  -> mark active.
//   * Surface present, no texture    -> queue CREATE on Arena.
//   * Surface and texture both set   -> no-op.
//
// Safe to call every frame; the Arena's CREATE branch checks for an existing
// texture before allocating, so re-queuing is harmless.
inline void EnsureSheetTextureQueued(gfx::Bitmap& sheet) {
  if (sheet.surface() == nullptr) {
    return;
  }
  if (!sheet.is_active()) {
    sheet.set_active(true);
  }
  if (sheet.texture() == nullptr) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &sheet);
  }
  // Sheets shown in the gfx-group viewers are read-only previews. Marking
  // the purpose lets later passes (and ad-hoc inspectors) tell preview
  // bitmaps apart from editable scratchpads without consulting the caller.
  sheet.metadata().purpose = gfx::Bitmap::BitmapPurpose::kPreview;
}

// Apply the role-default palette to a sheet bitmap. Looks up the binding via
// sheet_role_palette_table and resolves it against the provided palette
// groups. Returns true if a palette was applied, false otherwise.
//
// No-op (returns false) when:
//   * role is kUnclassified
//   * the role's palette group cannot be resolved (palette data not loaded
//     yet, or unknown group name)
//   * the binding's sub_index is out of range for the resolved group
inline const gfx::SnesPalette* ResolveRoleDefaultPalette(
    gfx::SheetRole role, gfx::PaletteGroupMap& palette_groups) {
  if (role == gfx::SheetRole::kUnclassified)
    return nullptr;
  const auto binding = gfx::DefaultBindingFor(role);
  if (binding.palette_group_name.empty())
    return nullptr;
  auto* group =
      palette_groups.get_group(std::string(binding.palette_group_name));
  if (group == nullptr || group->size() == 0)
    return nullptr;
  const size_t index =
      binding.default_sub_index < group->size() ? binding.default_sub_index : 0;
  const auto* palette = group->mutable_palette(index);
  return palette && !palette->empty() ? palette : nullptr;
}

inline bool ApplyRoleDefaultPalette(gfx::Bitmap& sheet, gfx::SheetRole role,
                                    gfx::PaletteGroupMap& palette_groups) {
  const auto* palette = ResolveRoleDefaultPalette(role, palette_groups);
  if (!palette)
    return false;
  sheet.SetPalette(*palette);
  return true;
}

// A view owns its palette and texture; Arena sheets remain source data.
inline void SyncSheetPreview(const gfx::Bitmap& source,
                             const gfx::SnesPalette& palette,
                             gfx::Bitmap& preview) {
  if (!source.surface())
    return;
  const bool pixels_changed =
      !preview.surface() || preview.width() != source.width() ||
      preview.height() != source.height() ||
      preview.depth() != source.depth() || preview.vector() != source.vector();
  const bool palette_changed = !(preview.palette() == palette);
  if (pixels_changed) {
    preview.Create(source.width(), source.height(), source.depth(),
                   source.vector());
  }
  if (pixels_changed || palette_changed)
    preview.SetPalette(palette);
  EnsureSheetTextureQueued(preview);
  if (preview.texture() && (pixels_changed || palette_changed)) {
    preview.UpdateTexture();
  }
}

}  // namespace internal
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_GFX_GROUP_EDITOR_INTERNAL_H_
