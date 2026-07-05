#include "util/i18n/translator.h"

#include <string>
#include <unordered_map>

#include "util/i18n/language_manager.h"

namespace yaze {
namespace i18n {
namespace {

// Resolved-value store keyed by source CONTENT. unordered_map is node-based, so
// the std::string values never move on rehash and their c_str() stays valid
// until the map is cleared (only on a language switch). Handles distinct
// addresses that hold equal content (e.g. std::string::c_str() sources).
std::unordered_map<std::string, std::string>& Resolved() {
  static std::unordered_map<std::string, std::string> store;
  return store;
}

// Translates only the visible prefix of an ImGui label, preserving any
// "##id"/"###id" suffix byte-for-byte.
std::string Compose(const std::string& source) {
  const std::string::size_type pos = source.find("##");
  const std::string visible =
      (pos == std::string::npos) ? source : source.substr(0, pos);
  if (visible.empty()) {
    return source;  // pure-id label like "##foo": nothing visible to translate
  }
  const std::string* translated = LanguageManager::Get().Find(visible);
  const std::string& out_visible =
      (translated != nullptr && !translated->empty()) ? *translated : visible;
  if (pos == std::string::npos) {
    return out_visible;
  }
  return out_visible + source.substr(pos);  // re-attach the id suffix unchanged
}

const std::string& ResolveByContent(const std::string& key) {
  auto& store = Resolved();
  auto it = store.find(key);
  if (it == store.end()) {
    it = store.emplace(key, Compose(key)).first;
  }
  return it->second;
}

}  // namespace

const char* tr(const char* source) {
  if (source == nullptr)
    return "";
  // Resolve by CONTENT, not by pointer: menus are rebuilt every frame, so a
  // label's std::string reuses addresses across frames. A pointer-keyed cache
  // returned the previous frame's translation for a reused address -> the
  // menu-label flicker. ResolveByContent already caches per unique string.
  return ResolveByContent(std::string(source)).c_str();
}

const char* tr(const std::string& source) {
  return ResolveByContent(source).c_str();
}

void ClearTranslationCache() {
  Resolved().clear();
}

}  // namespace i18n
}  // namespace yaze
