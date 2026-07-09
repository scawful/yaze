#ifndef YAZE_UTIL_I18N_LANGUAGE_MANAGER_H
#define YAZE_UTIL_I18N_LANGUAGE_MANAGER_H

#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace yaze {
namespace i18n {

// Holds the loaded translation catalogs and the active locale. Mirrors the
// structure of yaze::gui::ThemeManager (Meyers singleton + single std::function
// changed-callback + graceful fallback to the default locale).
//
// The English ("en") locale is the identity: it needs no catalog because
// untranslated lookups fall back to the source string. Any locale is loaded
// lazily from assets/i18n/<locale>.json via util::PlatformPaths::FindAsset.
class LanguageManager {
 public:
  static LanguageManager& Get();

  // Locale codes that have a catalog on disk, always including "en". Sorted.
  std::vector<std::string> GetAvailableLocales() const;

  const std::string& GetCurrentLocale() const { return current_locale_; }

  // Switches the active language, loading its catalog on first use. Unknown or
  // unreadable locales fall back to "en". Clears the translator caches and
  // fires the changed-callback so the UI re-resolves strings next frame.
  void SetLanguage(const std::string& locale);

  using LanguageChangedCallback = std::function<void(const std::string&)>;
  void SetOnLanguageChangedCallback(LanguageChangedCallback cb) {
    on_changed_ = std::move(cb);
  }

  // Returns the translation of a visible key in the current locale, or nullptr
  // if there is none (caller then keeps the source string). The pointer stays
  // valid for the lifetime of the loaded catalog (catalogs are never evicted).
  const std::string* Find(const std::string& visible_key) const;

  // Test-only: injects a catalog for `locale` parsed from an in-memory flat
  // JSON object, bypassing the filesystem. Returns false if the text is not a
  // valid flat string->string object. Enables deterministic i18n tests.
  bool LoadCatalogFromStringForTesting(const std::string& locale,
                                       const std::string& json_text);

 private:
  LanguageManager();

  // Loads assets/i18n/<locale>.json into catalogs_. Returns false on failure.
  // "en" always succeeds (empty identity catalog).
  bool LoadCatalog(const std::string& locale);

  std::map<std::string, std::unordered_map<std::string, std::string>> catalogs_;
  std::string current_locale_ = "en";
  LanguageChangedCallback on_changed_;
};

}  // namespace i18n
}  // namespace yaze

#endif  // YAZE_UTIL_I18N_LANGUAGE_MANAGER_H
