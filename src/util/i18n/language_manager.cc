#include "util/i18n/language_manager.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "util/i18n/translator.h"
#include "util/log.h"
#include "util/platform_paths.h"

namespace yaze {
namespace i18n {
namespace {

constexpr char kDefaultLocale[] = "en";

// Appends the UTF-8 encoding of a Unicode code point to out.
void AppendUtf8(uint32_t cp, std::string* out) {
  if (cp <= 0x7F) {
    out->push_back(static_cast<char>(cp));
  } else if (cp <= 0x7FF) {
    out->push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    out->push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out->push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out->push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

int HexVal(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

// Minimal, self-contained parser for a FLAT JSON object of the form
// {"source":"translation", ...}. Values and keys are UTF-8; standard escapes
// (\" \\ \/ \b \f \n \r \t \uXXXX, incl. surrogate pairs) are decoded. Anything
// that is not a flat string->string object is rejected (returns false). This
// avoids coupling the foundation util library to a JSON dependency.
class FlatJsonParser {
 public:
  explicit FlatJsonParser(const std::string& text) : s_(text) {}

  bool Parse(std::unordered_map<std::string, std::string>* out) {
    SkipWs();
    if (!Consume('{'))
      return false;
    SkipWs();
    if (Peek() == '}') {
      ++i_;
      return true;
    }
    while (true) {
      SkipWs();
      std::string key;
      if (!ParseString(&key))
        return false;
      SkipWs();
      if (!Consume(':'))
        return false;
      SkipWs();
      std::string value;
      if (!ParseString(&value))
        return false;
      (*out)[key] = value;
      SkipWs();
      const char c = Peek();
      if (c == ',') {
        ++i_;
        continue;
      }
      if (c == '}') {
        ++i_;
        return true;
      }
      return false;
    }
  }

 private:
  char Peek() const { return i_ < s_.size() ? s_[i_] : '\0'; }
  bool Consume(char c) {
    if (Peek() != c)
      return false;
    ++i_;
    return true;
  }
  void SkipWs() {
    while (i_ < s_.size()) {
      const char c = s_[i_];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        ++i_;
      } else {
        break;
      }
    }
  }

  bool ParseString(std::string* out) {
    if (!Consume('"'))
      return false;
    while (i_ < s_.size()) {
      const char c = s_[i_++];
      if (c == '"')
        return true;
      if (c != '\\') {
        out->push_back(c);
        continue;
      }
      if (i_ >= s_.size())
        return false;
      const char e = s_[i_++];
      switch (e) {
        case '"':
          out->push_back('"');
          break;
        case '\\':
          out->push_back('\\');
          break;
        case '/':
          out->push_back('/');
          break;
        case 'b':
          out->push_back('\b');
          break;
        case 'f':
          out->push_back('\f');
          break;
        case 'n':
          out->push_back('\n');
          break;
        case 'r':
          out->push_back('\r');
          break;
        case 't':
          out->push_back('\t');
          break;
        case 'u': {
          uint32_t cp = 0;
          if (!ParseHex4(&cp))
            return false;
          if (cp >= 0xD800 && cp <= 0xDBFF) {  // high surrogate
            if (i_ + 1 < s_.size() && s_[i_] == '\\' && s_[i_ + 1] == 'u') {
              i_ += 2;
              uint32_t lo = 0;
              if (!ParseHex4(&lo))
                return false;
              cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            }
          }
          AppendUtf8(cp, out);
          break;
        }
        default:
          return false;
      }
    }
    return false;  // unterminated string
  }

  bool ParseHex4(uint32_t* out) {
    if (i_ + 4 > s_.size())
      return false;
    uint32_t v = 0;
    for (int k = 0; k < 4; ++k) {
      const int h = HexVal(s_[i_++]);
      if (h < 0)
        return false;
      v = (v << 4) | static_cast<uint32_t>(h);
    }
    *out = v;
    return true;
  }

  const std::string& s_;
  std::string::size_type i_ = 0;
};

// Resolves the assets/i18n directory (via a known sentinel file) so we can
// enumerate available locale catalogs. Returns empty on failure.
std::filesystem::path ResolveI18nDir() {
  auto path = util::PlatformPaths::FindAsset("i18n/en.json");
  if (path.ok()) {
    return path->parent_path();
  }
  // en.json is optional; try fr.json to still locate the directory.
  path = util::PlatformPaths::FindAsset("i18n/fr.json");
  if (path.ok()) {
    return path->parent_path();
  }
  return {};
}

}  // namespace

LanguageManager& LanguageManager::Get() {
  static LanguageManager instance;
  return instance;
}

LanguageManager::LanguageManager() {
  // "en" is the identity locale: an empty catalog means every lookup misses and
  // tr() falls back to the (English) source string.
  catalogs_[kDefaultLocale] = {};
}

std::vector<std::string> LanguageManager::GetAvailableLocales() const {
  std::vector<std::string> locales;
  locales.push_back(kDefaultLocale);
  const std::filesystem::path dir = ResolveI18nDir();
  if (!dir.empty()) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
      if (ec)
        break;
      if (!entry.is_regular_file())
        continue;
      const std::filesystem::path& p = entry.path();
      if (p.extension() != ".json")
        continue;
      std::string stem = p.stem().string();
      if (stem == kDefaultLocale)
        continue;
      if (std::find(locales.begin(), locales.end(), stem) == locales.end()) {
        locales.push_back(stem);
      }
    }
  }
  std::sort(locales.begin(), locales.end());
  return locales;
}

bool LanguageManager::LoadCatalog(const std::string& locale) {
  if (catalogs_.count(locale))
    return true;
  if (locale == kDefaultLocale) {
    catalogs_[locale] = {};
    return true;
  }
  auto asset = util::PlatformPaths::FindAsset("i18n/" + locale + ".json");
  if (!asset.ok()) {
    util::logf("i18n: catalog for '%s' not found", locale.c_str());
    return false;
  }
  std::ifstream file(asset->string(), std::ios::binary);
  if (!file) {
    util::logf("i18n: cannot open catalog %s", asset->string().c_str());
    return false;
  }
  std::ostringstream ss;
  ss << file.rdbuf();
  const std::string text = ss.str();
  std::unordered_map<std::string, std::string> catalog;
  FlatJsonParser parser(text);
  if (!parser.Parse(&catalog)) {
    util::logf("i18n: failed to parse catalog %s", asset->string().c_str());
    return false;
  }
  util::logf("i18n: loaded '%s' catalog (%d entries)", locale.c_str(),
             static_cast<int>(catalog.size()));
  catalogs_[locale] = std::move(catalog);
  return true;
}

void LanguageManager::SetLanguage(const std::string& locale) {
  std::string target = locale.empty() ? std::string(kDefaultLocale) : locale;
  if (!LoadCatalog(target)) {
    target = kDefaultLocale;
    LoadCatalog(target);
  }
  current_locale_ = target;
  ClearTranslationCache();
  if (on_changed_) {
    on_changed_(current_locale_);
  }
}

bool LanguageManager::LoadCatalogFromStringForTesting(
    const std::string& locale, const std::string& json_text) {
  std::unordered_map<std::string, std::string> catalog;
  FlatJsonParser parser(json_text);
  if (!parser.Parse(&catalog)) {
    return false;
  }
  catalogs_[locale] = std::move(catalog);
  return true;
}

const std::string* LanguageManager::Find(const std::string& visible_key) const {
  auto cat_it = catalogs_.find(current_locale_);
  if (cat_it == catalogs_.end())
    return nullptr;
  auto it = cat_it->second.find(visible_key);
  if (it == cat_it->second.end())
    return nullptr;
  return &it->second;
}

}  // namespace i18n
}  // namespace yaze
