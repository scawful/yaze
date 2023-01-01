#ifndef YAZE_GUI_WIDGETS_H
#define YAZE_GUI_WIDGETS_H

#include <ImGuiColorTextEdit/TextEditor.h>

#include "absl/status/status.h"
#include "app/core/constants.h"

namespace yaze {
namespace gui {
namespace widgets {

TextEditor::LanguageDefinition GetAssemblyLanguageDef();

}  // namespace widgets
}  // namespace gui
}  // namespace yaze

#endif