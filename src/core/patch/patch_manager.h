#ifndef YAZE_CORE_PATCH_PATCH_MANAGER_H
#define YAZE_CORE_PATCH_PATCH_MANAGER_H

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "core/patch/asm_patch.h"

namespace yaze {
class Rom;
}

namespace yaze::core {

/**
 * @brief Manages a collection of ZScream-compatible ASM patches
 *
 * PatchManager handles patch discovery, loading, organization, and application.
 * It expects patches to be organized in a directory structure like:
 *
 * ```
 * patches_dir/
 * ├── Misc/
 * │   ├── IntroSkip.asm
 * │   └── ...
 * ├── Sprites/
 * │   ├── Spike Damage.asm
 * │   └── ...
 * ├── Hex Edits/
 * │   └── ...
 * └── UNPATCHED/    (ignored)
 * ```
 */
class PatchManager {
 public:
  PatchManager() = default;

  /**
   * @brief Load all patches from a directory structure
   * @param patches_dir Path to the patches root directory
   * @return Status indicating success or failure
   */
  absl::Status LoadPatches(const std::string& patches_dir);

  /**
   * @brief Reload patches from the current directory
   */
  absl::Status ReloadPatches();

  /**
   * @brief Get list of patch folder names
   */
  const std::vector<std::string>& folders() const { return folders_; }

  /**
   * @brief Get all patches in a specific folder
   * @param folder The folder name (e.g., "Sprites")
   * @return Vector of pointers to patches in that folder
   */
  std::vector<AsmPatch*> GetPatchesInFolder(const std::string& folder);

  /**
   * @brief Get a specific patch by folder and filename
   * @param folder The folder name
   * @param filename The filename (e.g., "Spike Damage.asm")
   * @return Pointer to the patch, or nullptr if not found
   */
  AsmPatch* GetPatch(const std::string& folder, const std::string& filename);

  /**
   * @brief Get all loaded patches
   */
  const std::vector<std::unique_ptr<AsmPatch>>& patches() const {
    return patches_;
  }

  /**
   * @brief Get count of enabled patches
   */
  int GetEnabledPatchCount() const;

  /**
   * @brief Apply all enabled patches to a ROM
   * @param rom The ROM to patch
   * @return Status indicating success or failure
   */
  absl::Status ApplyEnabledPatches(Rom* rom);

  /**
   * @brief Generate a combined .asm file that includes all enabled patches
   * @param output_path Path to write the combined patch file
   * @return Status indicating success or failure
   *
   * The generated file can be applied using Asar directly. It uses `incsrc`
   * directives to include each enabled patch file.
   */
  absl::Status GenerateCombinedPatch(const std::string& output_path);

  /**
   * @brief Save all patches to their files
   * @return Status indicating success or failure
   */
  absl::Status SaveAllPatches();

  /**
   * @brief Create a new patch folder
   * @param folder_name The name of the new folder
   * @return Status indicating success or failure
   */
  absl::Status CreatePatchFolder(const std::string& folder_name);

  /**
   * @brief Remove a patch folder and all its contents
   * @param folder_name The folder to remove
   * @return Status indicating success or failure
   */
  absl::Status RemovePatchFolder(const std::string& folder_name);

  /**
   * @brief Add a patch file from an external source
   * @param source_path Path to the source .asm file
   * @param target_folder Folder to add the patch to
   * @return Status indicating success or failure
   */
  absl::Status AddPatchFile(const std::string& source_path,
                            const std::string& target_folder);

  /**
   * @brief Remove a patch file
   * @param folder The folder containing the patch
   * @param filename The patch filename
   * @return Status indicating success or failure
   */
  absl::Status RemovePatchFile(const std::string& folder,
                               const std::string& filename);

  /**
   * @brief Get the patches directory path
   */
  const std::string& patches_directory() const { return patches_directory_; }

  /**
   * @brief Check if patches have been loaded
   */
  bool is_loaded() const { return is_loaded_; }

 private:
  /**
   * @brief Scan a directory for .asm files
   * @param dir_path Path to the directory
   * @param folder_name Name to assign to patches found
   */
  void ScanDirectory(const std::string& dir_path, const std::string& folder_name);

  std::vector<std::string> folders_;
  std::vector<std::unique_ptr<AsmPatch>> patches_;
  std::string patches_directory_;
  bool is_loaded_ = false;
};

}  // namespace yaze::core

#endif  // YAZE_CORE_PATCH_PATCH_MANAGER_H
