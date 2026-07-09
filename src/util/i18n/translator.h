#ifndef YAZE_UTIL_I18N_TRANSLATOR_H
#define YAZE_UTIL_I18N_TRANSLATOR_H

#include <string>

namespace yaze {
namespace i18n {

// Translates a user-visible source string (the English msgid) into the active
// language. Returns a pointer that stays valid until the next language switch;
// never returns null.
//
// ImGui label convention is handled transparently: for a label like
// "Mode##selector" or "Layout###main", only the VISIBLE prefix ("Mode",
// "Layout") is translated while the "##id"/"###id" suffix is preserved
// byte-for-byte so ImGui widget state/IDs remain stable across languages.
// A pure-id label like "##foo" is returned unchanged.
const char* tr(const char* source);
const char* tr(const std::string& source);

// Drops the internal resolution caches. Called by LanguageManager whenever the
// active language changes so the next frame re-resolves every string.
void ClearTranslationCache();

}  // namespace i18n
}  // namespace yaze

#endif  // YAZE_UTIL_I18N_TRANSLATOR_H
