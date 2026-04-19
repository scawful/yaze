#ifndef YAZE_APP_EDITOR_AGENT_AGENT_EDITOR_INTERNAL_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_EDITOR_INTERNAL_H_

#include <cstring>
#include <optional>
#include <string>

#include "app/editor/agent/agent_editor.h"
#include "app/editor/system/user_settings.h"
#include "cli/service/ai/ai_config_utils.h"
#include "cli/service/ai/provider_ids.h"

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <TargetConditionals.h>
#endif

namespace yaze {
namespace editor {
namespace internal {

template <size_t N>
void CopyStringToBuffer(const std::string& src, char (&dest)[N]) {
  std::strncpy(dest, src.c_str(), N - 1);
  dest[N - 1] = '\0';
}

inline std::optional<std::string> LoadKeychainValue(const std::string& key) {
#if defined(__APPLE__)
  if (key.empty()) {
    return std::nullopt;
  }
  CFStringRef key_ref = CFStringCreateWithCString(
      kCFAllocatorDefault, key.c_str(), kCFStringEncodingUTF8);
  const void* keys[] = {kSecClass, kSecAttrAccount, kSecReturnData,
                        kSecMatchLimit};
  const void* values[] = {kSecClassGenericPassword, key_ref, kCFBooleanTrue,
                          kSecMatchLimitOne};
  CFDictionaryRef query = CFDictionaryCreate(
      kCFAllocatorDefault, keys, values,
      static_cast<CFIndex>(sizeof(keys) / sizeof(keys[0])),
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFTypeRef item = nullptr;
  OSStatus status = SecItemCopyMatching(query, &item);
  if (query) {
    CFRelease(query);
  }
  if (key_ref) {
    CFRelease(key_ref);
  }
  if (status == errSecItemNotFound) {
    return std::nullopt;
  }
  if (status != errSecSuccess || !item) {
    if (item) {
      CFRelease(item);
    }
    return std::nullopt;
  }
  CFDataRef data_ref = static_cast<CFDataRef>(item);
  const UInt8* data_ptr = CFDataGetBytePtr(data_ref);
  CFIndex data_len = CFDataGetLength(data_ref);
  std::string value(reinterpret_cast<const char*>(data_ptr),
                    static_cast<size_t>(data_len));
  CFRelease(item);
  return value;
#else
  (void)key;
  return std::nullopt;
#endif
}

inline std::string ResolveHostApiKey(
    const UserSettings::Preferences* prefs,
    const UserSettings::Preferences::AiHost& host) {
  if (!host.api_key.empty()) {
    return host.api_key;
  }
  if (!host.credential_id.empty()) {
    if (auto key = LoadKeychainValue(host.credential_id)) {
      return *key;
    }
  }
  if (!prefs) {
    return {};
  }
  std::string api_type =
      host.api_type.empty() ? cli::kProviderOpenAi : host.api_type;
  if (api_type == cli::kProviderLmStudio) {
    api_type = cli::kProviderOpenAi;
  }
  if (api_type == cli::kProviderOpenAi) {
    return prefs->openai_api_key;
  }
  if (api_type == cli::kProviderGemini) {
    return prefs->gemini_api_key;
  }
  if (api_type == cli::kProviderAnthropic) {
    return prefs->anthropic_api_key;
  }
  return {};
}

inline void ApplyHostPresetToProfile(
    AgentEditor::BotProfile* profile,
    const UserSettings::Preferences::AiHost& host,
    const UserSettings::Preferences* prefs) {
  if (!profile) {
    return;
  }
  std::string api_key = ResolveHostApiKey(prefs, host);
  profile->host_id = host.id;
  std::string api_type = host.api_type;
  if (api_type == cli::kProviderLmStudio) {
    api_type = cli::kProviderOpenAi;
  }
  if (api_type == cli::kProviderOpenAi || api_type == cli::kProviderOllama ||
      api_type == cli::kProviderGemini || api_type == cli::kProviderAnthropic) {
    profile->provider = api_type;
  }
  if (profile->provider == cli::kProviderOpenAi) {
    if (!host.base_url.empty()) {
      profile->openai_base_url = cli::NormalizeOpenAiBaseUrl(host.base_url);
    }
    if (!api_key.empty()) {
      profile->openai_api_key = api_key;
    }
  } else if (profile->provider == cli::kProviderOllama) {
    if (!host.base_url.empty()) {
      profile->ollama_host = host.base_url;
    }
  } else if (profile->provider == cli::kProviderGemini) {
    if (!api_key.empty()) {
      profile->gemini_api_key = api_key;
    }
  } else if (profile->provider == cli::kProviderAnthropic) {
    if (!api_key.empty()) {
      profile->anthropic_api_key = api_key;
    }
  }
}

}  // namespace internal
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_EDITOR_INTERNAL_H_
