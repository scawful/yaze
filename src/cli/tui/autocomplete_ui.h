#ifndef YAZE_CLI_TUI_AUTOCOMPLETE_UI_H_
#define YAZE_CLI_TUI_AUTOCOMPLETE_UI_H_

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "cli/util/autocomplete.h"

namespace yaze {
namespace cli {

/**
 * @brief Create an input component with autocomplete suggestions
 *
 * @param input_str Pointer to the input string
 * @param engine Pointer to the autocomplete engine
 * @return ftxui::Component Input component with autocomplete dropdown
 */
ftxui::Component CreateAutocompleteInput(std::string* input_str,
                                         AutocompleteEngine* engine);

/**
 * @brief Create a quick action menu for common ROM operations
 *
 * @param screen The screen interactive reference
 * @return ftxui::Component Menu component with quick actions
 */
ftxui::Component CreateQuickActionMenu(ftxui::ScreenInteractive& screen);

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_TUI_AUTOCOMPLETE_UI_H_
