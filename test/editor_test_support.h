#ifndef YAZE_TEST_EDITOR_TEST_SUPPORT_H
#define YAZE_TEST_EDITOR_TEST_SUPPORT_H

#include <filesystem>
#include <system_error>
#include <vector>

#include "app/editor/editor_manager.h"
#include "app/gfx/backend/irenderer.h"
#include "unique_temp_path.h"

namespace yaze::test {

namespace internal {

// Removes every redirected settings file when the test process exits.
struct IsolatedSettingsFiles {
  std::vector<std::filesystem::path> paths;
  ~IsolatedSettingsFiles() {
    for (const auto& path : paths) {
      std::error_code ec;
      std::filesystem::remove(path, ec);
      std::filesystem::remove(path.string() + ".legacy.ini", ec);
      std::filesystem::remove(path.string() + ".bak", ec);
    }
  }
};

inline IsolatedSettingsFiles& GetIsolatedSettingsFiles() {
  static IsolatedSettingsFiles files;
  return files;
}

}  // namespace internal

// Initializes `manager` with a settings file no other test (or test process)
// uses. EditorManager::Initialize loads UserSettings, and saved preferences
// such as the sidebar active_category change what a ROM open materializes, so
// every case starts from default preferences. The runner already keeps all
// tests away from the real ~/Documents/Yaze/settings.json; this adds per-case
// freshness when several cases share one process.
inline void InitializeWithIsolatedSettings(editor::EditorManager& manager,
                                           gfx::IRenderer* renderer) {
  const auto settings_path = UniqueTempPath("yaze_test_settings", ".json");
  internal::GetIsolatedSettingsFiles().paths.push_back(settings_path);
  manager.user_settings().SetSettingsFilePathForTesting(settings_path.string());
  manager.Initialize(renderer, "");
}

}  // namespace yaze::test

#endif  // YAZE_TEST_EDITOR_TEST_SUPPORT_H
