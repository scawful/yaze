#include "app/editor/overworld/entity_operations.h"

#include <memory>

#include "gtest/gtest.h"
#include "rom/rom.h"

namespace yaze::editor {
namespace {

class OverworldItemOperationsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    overworld_ = std::make_unique<zelda3::Overworld>(&rom_);
  }

  Rom rom_;
  std::unique_ptr<zelda3::Overworld> overworld_;
};

TEST_F(OverworldItemOperationsTest, RemoveItemErasesMatchingEntry) {
  auto* items = overworld_->mutable_all_items();
  items->emplace_back(0x10, 0x00, 16, 32, false);
  items->emplace_back(0x20, 0x00, 48, 64, false);
  items->emplace_back(0x30, 0x00, 80, 96, false);
  ASSERT_EQ(items->size(), 3u);

  const auto* middle_item = &items->at(1);
  const auto status = RemoveItem(overworld_.get(), middle_item);
  ASSERT_TRUE(status.ok()) << status.message();

  ASSERT_EQ(items->size(), 2u);
  EXPECT_EQ(items->at(0).id_, 0x10);
  EXPECT_EQ(items->at(1).id_, 0x30);
}

TEST_F(OverworldItemOperationsTest, RemoveItemRejectsNullArguments) {
  auto status = RemoveItem(nullptr, nullptr);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);

  status = RemoveItem(overworld_.get(), nullptr);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(OverworldItemOperationsTest,
       RemoveItemReturnsNotFoundWhenPointerMissing) {
  auto* items = overworld_->mutable_all_items();
  items->emplace_back(0x44, 0x00, 16, 16, false);
  ASSERT_EQ(items->size(), 1u);

  zelda3::OverworldItem external_item(0x55, 0x00, 32, 32, false);
  const auto status = RemoveItem(overworld_.get(), &external_item);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
  EXPECT_EQ(items->size(), 1u);
}

}  // namespace
}  // namespace yaze::editor
