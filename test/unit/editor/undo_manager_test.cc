#include "app/editor/core/undo_manager.h"

#include <memory>
#include <string>

#include <gtest/gtest.h>

namespace yaze::editor {
namespace {

class IntAction : public UndoAction {
 public:
  IntAction(int* target, int before, int after, std::string desc)
      : target_(target),
        before_(before),
        after_(after),
        desc_(std::move(desc)) {}

  absl::Status Undo() override {
    if (target_ == nullptr) {
      return absl::InvalidArgumentError("null target");
    }
    *target_ = before_;
    return absl::OkStatus();
  }

  absl::Status Redo() override {
    if (target_ == nullptr) {
      return absl::InvalidArgumentError("null target");
    }
    *target_ = after_;
    return absl::OkStatus();
  }

  std::string Description() const override { return desc_; }
  size_t MemoryUsage() const override { return sizeof(*this); }

 protected:
  int* target_ = nullptr;
  int before_ = 0;
  int after_ = 0;
  std::string desc_;
};

class MergeableIntAction final : public IntAction {
 public:
  using IntAction::IntAction;

  bool CanMergeWith(const UndoAction& prev) const override {
    const auto* other = dynamic_cast<const MergeableIntAction*>(&prev);
    return other != nullptr && other->target_ == target_;
  }

  void MergeWith(UndoAction& prev) override {
    const auto* other = dynamic_cast<const MergeableIntAction*>(&prev);
    if (other == nullptr) {
      return;
    }
    // Preserve the earliest "before" and the latest "after".
    before_ = other->before_;
  }
};

}  // namespace

TEST(UndoManagerTest, UndoRedoRoundTrip) {
  int value = 0;
  UndoManager mgr;

  value = 1;  // simulate a mutation that already happened
  mgr.Push(std::make_unique<IntAction>(&value, /*before=*/0, /*after=*/1, "A"));

  EXPECT_TRUE(mgr.CanUndo());
  EXPECT_FALSE(mgr.CanRedo());
  EXPECT_EQ(mgr.GetUndoDescription(), "A");

  ASSERT_TRUE(mgr.Undo().ok());
  EXPECT_EQ(value, 0);

  EXPECT_FALSE(mgr.CanUndo());
  EXPECT_TRUE(mgr.CanRedo());
  EXPECT_EQ(mgr.GetRedoDescription(), "A");

  ASSERT_TRUE(mgr.Redo().ok());
  EXPECT_EQ(value, 1);
}

TEST(UndoManagerTest, PushClearsRedoStack) {
  int value = 0;
  UndoManager mgr;

  value = 1;
  mgr.Push(std::make_unique<IntAction>(&value, /*before=*/0, /*after=*/1, "A"));
  ASSERT_TRUE(mgr.Undo().ok());
  ASSERT_TRUE(mgr.CanRedo());

  value = 2;
  mgr.Push(std::make_unique<IntAction>(&value, /*before=*/0, /*after=*/2, "B"));
  EXPECT_FALSE(mgr.CanRedo());
  EXPECT_TRUE(mgr.CanUndo());
}

TEST(UndoManagerTest, EnforcesStackLimitByDroppingOldest) {
  int value = 0;
  UndoManager mgr;
  mgr.SetMaxStackSize(2);

  value = 1;
  mgr.Push(std::make_unique<IntAction>(&value, /*before=*/0, /*after=*/1, "A"));
  value = 2;
  mgr.Push(std::make_unique<IntAction>(&value, /*before=*/1, /*after=*/2, "B"));
  value = 3;
  mgr.Push(std::make_unique<IntAction>(&value, /*before=*/2, /*after=*/3, "C"));

  EXPECT_EQ(mgr.UndoStackSize(), 2u);
  ASSERT_TRUE(mgr.Undo().ok());
  EXPECT_EQ(value, 2);
  ASSERT_TRUE(mgr.Undo().ok());
  EXPECT_EQ(value, 1);

  // "A" was dropped.
  EXPECT_FALSE(mgr.CanUndo());
}

TEST(UndoManagerTest, MergeReplacesPreviousUndoAction) {
  int value = 0;
  UndoManager mgr;

  value = 1;
  mgr.Push(std::make_unique<MergeableIntAction>(&value, /*before=*/0, /*after=*/1,
                                                "A"));
  value = 2;
  mgr.Push(std::make_unique<MergeableIntAction>(&value, /*before=*/1, /*after=*/2,
                                                "B"));

  EXPECT_EQ(mgr.UndoStackSize(), 1u);

  ASSERT_TRUE(mgr.Undo().ok());
  EXPECT_EQ(value, 0);
  ASSERT_TRUE(mgr.Redo().ok());
  EXPECT_EQ(value, 2);
}

}  // namespace yaze::editor
