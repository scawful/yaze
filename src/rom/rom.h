#ifndef YAZE_ROM_ROM_H
#define YAZE_ROM_ROM_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_tile.h"
#include "core/project.h"
#include "rom/lyra_layout_descriptor.h"
#include "rom/rom_diagnostics.h"

namespace yaze {

namespace rom {
class WriteFence;
}  // namespace rom
class ScopedRomReadOnly;

/// Size of the SMC/SWC copier header that some ROM dumps start with.
inline constexpr size_t kCopierHeaderSize = 0x200;

/// Why a copier header was recognized at the start of a ROM file.
enum class CopierHeaderReason {
  kNone,
  // File size % 0x8000 == 0x200: the standard copier-header layout.
  kSizeAlignment,
  // Nonstandard file size (neither a 32 KB multiple nor one plus 0x200), but
  // the SNES internal header is valid only 0x200 bytes into the file.
  // LYRA-v038b.smc (0x10083B bytes) is this case.
  kInternalHeaderOffset,
};

/// Returns how a copier header was recognized in `data`, or kNone. A file
/// whose SNES internal header is valid only without skipping 0x200 bytes is
/// never treated as headered.
CopierHeaderReason DetectCopierHeader(const uint8_t* data, size_t size);
const char* CopierHeaderReasonName(CopierHeaderReason reason);

/**
 * @brief The Rom class is used to load, save, and modify Rom data.
 * This is a generic SNES ROM container and does not contain game-specific logic.
 */
class Rom {
 public:
  struct SaveSettings {
    bool backup = false;
    bool save_new = false;
    std::string filename;
    // Require a successful backup of an existing save target before replacing
    // it. Unlike the legacy best-effort `backup` flag, backup failure aborts
    // the save. A new target has no previous bytes to protect. This implies
    // backup creation when `backup` is false.
    bool require_backup = false;
  };

  struct LoadOptions {
    // Detect a copier header (DetectCopierHeader) and keep it out of the ROM
    // data. The stripped bytes are kept and SaveToFile writes them back.
    bool strip_header = true;
    bool load_resource_labels = true;

    static LoadOptions Defaults() { return LoadOptions{}; }
  };

  Rom() = default;
  Rom(const Rom& other);
  Rom& operator=(const Rom& other);
  Rom& operator=(Rom&& other);
  ~Rom() = default;

  absl::Status LoadFromFile(
      const std::string& filename,
      const LoadOptions& options = LoadOptions::Defaults());
  absl::Status LoadFromData(
      const std::vector<uint8_t>& data,
      const LoadOptions& options = LoadOptions::Defaults());

  // Writes the ROM data. A ROM loaded with a copier header is written with
  // that header in front (in place or to a new file), so a save never
  // silently drops the header and an unedited round trip is byte-identical.
  absl::Status SaveToFile(const SaveSettings& settings);
  // The only Lyra writer: dedicated A0 terrain, exact loaded-image guard,
  // atomic Save As to a different backing file. Generic writes stay refused.
  absl::Status SaveLyraA0TerrainAs(const std::array<uint16_t, 256>& tiles,
                                   const std::string& filename);
  // Test seam: while set, SaveToFile fails when creating its staging file,
  // after any backup is written and before the destination is replaced.
  // The optional callback runs immediately before the injected error. It lets
  // tests model an external change after source publication without a race.
  // Disabling the fault also clears the callback.
  static void SetStagingFailureForTesting(
      bool fail, std::function<void()> before_failure = {});

  void Expand(int size) {
    // This exact-save transfer cannot be resized by a native producer.
    if (private_lyra_attached_ &&
        (size != 0x400000 || rom_data_.size() != 0x400000))
      return;
    const bool size_changed = rom_data_.size() != static_cast<size_t>(size);
    rom_data_.resize(size);
    size_ = size;
    if (size_changed) {
      AdvanceObjectTileRevision();
    }
  }

  void Close() {
    const bool had_data = !rom_data_.empty();
    rom_data_.clear();
    copier_header_.clear();
    copier_header_reason_ = CopierHeaderReason::kNone;
    lyra_attributed_at_load_ = false;
    lyra_terrain_baseline_.clear();
    size_ = 0;
    if (had_data) {
      AdvanceObjectTileRevision();
    }
  }

  // Raw access
  absl::StatusOr<uint8_t> ReadByte(int offset) const;
  absl::StatusOr<uint16_t> ReadWord(int offset) const;
  absl::StatusOr<uint32_t> ReadLong(int offset) const;
  absl::StatusOr<std::vector<uint8_t>> ReadByteVector(uint32_t offset,
                                                      uint32_t length) const;
  absl::StatusOr<gfx::Tile16> ReadTile16(uint32_t tile16_id,
                                         uint32_t tile16_ptr);

  absl::Status WriteTile16(int tile16_id, uint32_t tile16_ptr,
                           const gfx::Tile16& tile);
  absl::Status WriteByte(int addr, uint8_t value);
  absl::Status WriteWord(int addr, uint16_t value);
  absl::Status WriteShort(int addr, uint16_t value);
  absl::Status WriteLong(uint32_t addr, uint32_t value);
  absl::Status WriteVector(int addr, std::vector<uint8_t> data);
  absl::Status WriteColor(uint32_t address, const gfx::SnesColor& color);

  template <typename... Args>
  absl::Status WriteTransaction(Args... args) {
    absl::Status status;
    ((status = WriteHelper(args)), ...);
    return status;
  }

  template <typename T, typename... Args>
  absl::Status ReadTransaction(T& var, int address, Args&&... args) {
    absl::Status status = ReadHelper<T>(var, address);
    if (!status.ok()) {
      return status;
    }
    if constexpr (sizeof...(args) > 0) {
      status = ReadTransaction(std::forward<Args>(args)...);
    }
    return status;
  }

  struct WriteAction {
    using ValueType =
        std::variant<int, uint8_t, uint16_t, short, std::vector<uint8_t>,
                     gfx::SnesColor, std::vector<gfx::SnesColor>>;
    int address;
    ValueType value;
  };

  virtual absl::Status WriteHelper(const WriteAction& action);

  template <typename T>
  absl::Status ReadHelper(T& var, int address) {
    if constexpr (std::is_same_v<T, uint8_t>) {
      auto result = ReadByte(address);
      if (!result.ok())
        return result.status();
      var = *result;
    } else if constexpr (std::is_same_v<T, uint16_t>) {
      auto result = ReadWord(address);
      if (!result.ok())
        return result.status();
      var = *result;
    } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
      auto result = ReadByteVector(address, var.size());
      if (!result.ok())
        return result.status();
      var = *result;
    }
    return absl::OkStatus();
  }

  uint8_t& operator[](unsigned long i) {
    if (i >= size_)
      throw std::out_of_range("Rom index out of range");
    return rom_data_[i];
  }

  bool is_loaded() const { return !rom_data_.empty(); }
  bool dirty() const { return dirty_; }
  void set_dirty(bool dirty) { dirty_ = dirty; }
  void ClearDirty() { dirty_ = false; }

  // Monotonic generation for RoomObject caches derived from ROM tile tables.
  // Successful loads, content replacement, a real Close, and real size changes
  // advance it automatically. Writers that mutate object tile sources must
  // advance it only after their transaction commits successfully.
  uint64_t object_tile_revision() const { return object_tile_revision_; }
  void AdvanceObjectTileRevision() { ++object_tile_revision_; }

  auto title() const { return title_; }
  auto size() const { return size_; }
  auto data() const { return rom_data_.data(); }
  auto mutable_data() { return rom_data_.data(); }
  auto begin() { return rom_data_.begin(); }
  auto end() { return rom_data_.end(); }
  const auto& vector() const { return rom_data_; }
  auto& mutable_vector() { return rom_data_; }
  auto filename() const { return filename_; }
  auto set_filename(std::string_view name) { filename_ = name; }
  auto short_name() const { return short_name_; }

  // Copier header stripped at load (empty when the file had none). size()
  // and data() never include it.
  bool has_copier_header() const { return !copier_header_.empty(); }
  const std::vector<uint8_t>& copier_header() const { return copier_header_; }
  CopierHeaderReason copier_header_reason() const {
    return copier_header_reason_;
  }

  // The parsed result is derived from this ROM's current bytes. Attribution
  // captured at load prevents a damaged/erased signature from enabling saves.
  rom::LyraLayoutInspection InspectLyraLayout() const;
  absl::Status CheckLayoutWritable() const;

  // Sticky, value-owned exact-save reservation. No public detach capability.
  absl::Status AttachLyraPrivateReservation(const core::HackManifest& manifest);
  bool has_lyra_private_reservation() const { return private_lyra_attached_; }
  absl::Status ValidateLyraPrivateReservation() const;

  // Resource labels are generic enough to keep here
  project::ResourceLabelManager* resource_label() {
    return &resource_label_manager_;
  }

  // ROM write fence stack.
  //
  // When one or more fences are active, Rom::Write* calls must be allowed by
  // every fence (logical AND). This is used for strict save-time guardrails so
  // feature writers can't clobber reserved regions.
  void PushWriteFence(rom::WriteFence* fence);
  void PopWriteFence(rom::WriteFence* fence);
  size_t write_fence_depth() const { return write_fence_stack_.size(); }

 private:
  absl::Status SaveToFileUnchecked(const SaveSettings& settings);
  void CaptureLyraTerrainBaseline();
  absl::Status CapturePrivateLyraReservation();
  absl::Status ValidatePrivateLyraImage(
      const std::vector<uint8_t>& image) const;
  absl::Status CheckPrivateLyraWrite(uint32_t pc, const uint8_t* bytes,
                                     size_t size) const;
  bool private_lyra_attached_ = false;
  std::vector<std::pair<uint32_t, std::vector<uint8_t>>>
      private_lyra_preimages_;
  std::shared_ptr<rom::WriteFence> private_lyra_fence_;

  friend class ScopedRomReadOnly;

  // Instance-local command scopes, like write fences. Copies do not inherit
  // this state; assignment preserves the destination's active scopes.
  size_t read_only_scope_depth_ = 0;

  // Size of the ROM data.
  unsigned long size_ = 0;

  // Title of the ROM loaded from the header
  std::string title_ = "ROM not loaded";

  // Filename of the ROM
  std::string filename_;

  // Short name of the ROM
  std::string short_name_;

  // Full contiguous rom space
  std::vector<uint8_t> rom_data_;
  // Held only for the explicit revision-2 terrain capability. Direct mutable
  // access must not smuggle unrelated bytes through the dedicated Save As.
  std::vector<uint8_t> lyra_terrain_baseline_;

  // Copier header bytes removed from the front of the file at load.
  std::vector<uint8_t> copier_header_;
  CopierHeaderReason copier_header_reason_ = CopierHeaderReason::kNone;
  bool lyra_attributed_at_load_ = false;

  // Label manager for unique resource names.
  project::ResourceLabelManager resource_label_manager_;

  // True if there are unsaved changes
  bool dirty_ = false;

  // Generation of object-tile data visible to RoomObject parser caches.
  uint64_t object_tile_revision_ = 0;

  // Active write fences (not owned).
  std::vector<rom::WriteFence*> write_fence_stack_;
};

// Refuse normal ROM writes and file saves for this instance while the scope is
// alive. Independent ROM copies remain usable for dry-run simulations.
class ScopedRomReadOnly {
 public:
  explicit ScopedRomReadOnly(Rom& rom) : rom_(rom) {
    ++rom_.read_only_scope_depth_;
  }
  ~ScopedRomReadOnly() { --rom_.read_only_scope_depth_; }

  ScopedRomReadOnly(const ScopedRomReadOnly&) = delete;
  ScopedRomReadOnly& operator=(const ScopedRomReadOnly&) = delete;
  ScopedRomReadOnly(ScopedRomReadOnly&&) = delete;
  ScopedRomReadOnly& operator=(ScopedRomReadOnly&&) = delete;

 private:
  Rom& rom_;
};

}  // namespace yaze

#endif  // YAZE_ROM_ROM_H
