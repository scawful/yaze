// Transaction helper for atomic ROM operations with rollback
//
// Usage:
//   yaze::Transaction tx(rom);
//   auto status = tx.WriteByte(addr, val)
//                      .WriteWord(addr2, val2)
//                      .Commit();
//
// If any write fails before Commit, subsequent operations are skipped and
// Commit() will Rollback() previously applied writes in reverse order.

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "app/gfx/types/snes_color.h"
#include "rom/rom.h"

namespace yaze {

class Transaction {
 public:
  explicit Transaction(Rom& rom) : rom_(rom) {}

  Transaction& WriteByte(int address, uint8_t value) {
    if (!status_.ok())
      return *this;
    auto original = rom_.ReadByte(address);
    if (!original.ok()) {
      status_ = original.status();
      return *this;
    }
    status_ = rom_.WriteByte(address, value);
    if (status_.ok()) {
      operations_.push_back({address, static_cast<uint8_t>(*original),
                             OperationType::kWriteByte});
    }
    return *this;
  }

  Transaction& WriteWord(int address, uint16_t value) {
    if (!status_.ok())
      return *this;
    auto original = rom_.ReadWord(address);
    if (!original.ok()) {
      status_ = original.status();
      return *this;
    }
    status_ = rom_.WriteWord(address, value);
    if (status_.ok()) {
      operations_.push_back({address, static_cast<uint16_t>(*original),
                             OperationType::kWriteWord});
    }
    return *this;
  }

  Transaction& WriteLong(int address, uint32_t value) {
    if (!status_.ok())
      return *this;
    auto original = rom_.ReadLong(address);
    if (!original.ok()) {
      status_ = original.status();
      return *this;
    }
    status_ = rom_.WriteLong(address, value);
    if (status_.ok()) {
      operations_.push_back({address, static_cast<uint32_t>(*original),
                             OperationType::kWriteLong});
    }
    return *this;
  }

  Transaction& WriteVector(int address, const std::vector<uint8_t>& data) {
    if (!status_.ok())
      return *this;
    auto original =
        rom_.ReadByteVector(address, static_cast<uint32_t>(data.size()));
    if (!original.ok()) {
      status_ = original.status();
      return *this;
    }
    status_ = rom_.WriteVector(address, data);
    if (status_.ok()) {
      operations_.push_back({address, *original, OperationType::kWriteVector});
    }
    return *this;
  }

  Transaction& WriteColor(int address, const gfx::SnesColor& color) {
    if (!status_.ok())
      return *this;
    // Store original raw 16-bit value for rollback via WriteWord.
    auto original_word = rom_.ReadWord(address);
    if (!original_word.ok()) {
      status_ = original_word.status();
      return *this;
    }
    status_ = rom_.WriteColor(address, color);
    if (status_.ok()) {
      operations_.push_back({address, static_cast<uint16_t>(*original_word),
                             OperationType::kWriteColor});
    }
    return *this;
  }

  absl::Status Commit() {
    if (!status_.ok()) {
      Rollback();
    }
    return status_;
  }

  void Rollback() {
    for (auto it = operations_.rbegin(); it != operations_.rend(); ++it) {
      const auto& op = *it;
      switch (op.type) {
        case OperationType::kWriteByte:
          (void)rom_.WriteByte(op.address,
                               std::get<uint8_t>(op.original_value));
          break;
        case OperationType::kWriteWord:
          (void)rom_.WriteWord(op.address,
                               std::get<uint16_t>(op.original_value));
          break;
        case OperationType::kWriteLong:
          (void)rom_.WriteLong(op.address,
                               std::get<uint32_t>(op.original_value));
          break;
        case OperationType::kWriteVector:
          (void)rom_.WriteVector(
              op.address, std::get<std::vector<uint8_t>>(op.original_value));
          break;
        case OperationType::kWriteColor:
          (void)rom_.WriteWord(op.address,
                               std::get<uint16_t>(op.original_value));
          break;
      }
    }
    operations_.clear();
  }

 private:
  enum class OperationType {
    kWriteByte,
    kWriteWord,
    kWriteLong,
    kWriteVector,
    kWriteColor
  };

  struct Operation {
    int address;
    std::variant<uint8_t, uint16_t, uint32_t, std::vector<uint8_t>>
        original_value;
    OperationType type;
  };

  Rom& rom_;
  absl::Status status_;
  std::vector<Operation> operations_;
};

// Whole-buffer transaction used by coordinated editor saves. Individual
// editor serializers do not share a Transaction instance and may resize the
// ROM, so recording only writes made through Transaction is insufficient for
// the File > Save ROM pipeline. This guard snapshots the complete in-memory
// state and restores it unless the caller commits after the atomic disk write.
class ScopedRomTransaction {
 public:
  explicit ScopedRomTransaction(Rom& rom)
      : rom_(rom),
        data_(rom.vector()),
        filename_(rom.filename()),
        dirty_(rom.dirty()) {}

  ScopedRomTransaction(const ScopedRomTransaction&) = delete;
  ScopedRomTransaction& operator=(const ScopedRomTransaction&) = delete;

  ~ScopedRomTransaction() {
    if (!committed_) {
      Rollback();
    }
  }

  void Commit() { committed_ = true; }

  void Rollback() {
    if (committed_) {
      return;
    }
    rom_.mutable_vector() = data_;
    // Keep Rom::size() synchronized if a serializer resized the buffer.
    rom_.Expand(static_cast<int>(data_.size()));
    rom_.set_filename(filename_);
    rom_.set_dirty(dirty_);
    committed_ = true;
  }

 private:
  Rom& rom_;
  std::vector<uint8_t> data_;
  std::string filename_;
  bool dirty_ = false;
  bool committed_ = false;
};

}  // namespace yaze
