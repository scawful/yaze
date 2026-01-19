#ifndef YAZE_APP_EDITOR_CORE_PANEL_REGISTRATION_H_
#define YAZE_APP_EDITOR_CORE_PANEL_REGISTRATION_H_

#include "app/editor/core/content_registry.h"

namespace yaze::editor {

/**
 * @brief Auto-registration macro for panels with default constructors.
 *
 * Use this macro at file scope (after the class definition) to automatically
 * register a panel type with ContentRegistry. The panel will be instantiated
 * when ContentRegistry::Panels::CreateAll() is called.
 *
 * Requirements:
 *   - Panel class must have a default constructor
 *   - Panel class must inherit from EditorPanel
 *
 * Usage:
 * @code
 *   // In my_panel.h or my_panel.cc (after class definition)
 *   class MyPanel : public EditorPanel {
 *    public:
 *     MyPanel() = default;
 *     // ... implementation
 *   };
 *   REGISTER_PANEL(MyPanel);
 * @endcode
 *
 * The panel can then access shared resources via ContentRegistry::Context:
 * @code
 *   void MyPanel::Draw(bool* p_open) {
 *     auto* rom = ContentRegistry::Context::rom();
 *     auto* editor = ContentRegistry::Context::current_editor();
 *     // ...
 *   }
 * @endcode
 */
#define REGISTER_PANEL(PanelClass)                                    \
  namespace {                                                         \
  struct PanelRegistrar_##PanelClass {                                \
    PanelRegistrar_##PanelClass() {                                   \
      ::yaze::editor::ContentRegistry::Panels::add<PanelClass>();     \
    }                                                                 \
  };                                                                  \
  [[maybe_unused]] static PanelRegistrar_##PanelClass                 \
      s_panel_registrar_##PanelClass;                                 \
  }  // namespace

/**
 * @brief Registration macro for panels requiring custom factory logic.
 *
 * Use this when a panel needs constructor arguments or conditional creation.
 * The factory function receives no arguments and should return a unique_ptr
 * to the panel, or nullptr to skip creation.
 *
 * Usage:
 * @code
 *   REGISTER_PANEL_FACTORY(MyPanel, []() -> std::unique_ptr<EditorPanel> {
 *     auto* editor = ContentRegistry::Context::current_editor();
 *     if (!editor) return nullptr;
 *     return std::make_unique<MyPanel>(editor);
 *   });
 * @endcode
 */
#define REGISTER_PANEL_FACTORY(PanelClass, FactoryFunc)               \
  namespace {                                                         \
  struct PanelRegistrar_##PanelClass {                                \
    PanelRegistrar_##PanelClass() {                                   \
      ::yaze::editor::ContentRegistry::Panels::RegisterFactory(       \
          FactoryFunc);                                               \
    }                                                                 \
  };                                                                  \
  [[maybe_unused]] static PanelRegistrar_##PanelClass                 \
      s_panel_registrar_##PanelClass;                                 \
  }  // namespace

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_CORE_PANEL_REGISTRATION_H_
