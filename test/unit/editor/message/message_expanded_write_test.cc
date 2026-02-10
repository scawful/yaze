#include "app/editor/message/message_data.h"

#include <gtest/gtest.h>

#include <vector>

#include "rom/rom.h"
#include "rom/write_fence.h"

namespace yaze::editor {

namespace {

Rom MakeRom(size_t size) {
  Rom rom;
  std::vector<uint8_t> data(size, 0);
  auto status = rom.LoadFromData(data);
  EXPECT_TRUE(status.ok()) << status.message();
  return rom;
}

}  // namespace

TEST(ExpandedMessageWriteTest, WritesExpectedBytesToRegion) {
  Rom rom = MakeRom(256);

  const int start = 100;
  const int end = 120;  // inclusive

  ASSERT_TRUE(WriteExpandedTextData(&rom, start, end, {"A", "B"}).ok());

  std::vector<uint8_t> expected;
  auto a = ParseMessageToData("A");
  auto b = ParseMessageToData("B");
  expected.insert(expected.end(), a.begin(), a.end());
  expected.push_back(kMessageTerminator);
  expected.insert(expected.end(), b.begin(), b.end());
  expected.push_back(kMessageTerminator);
  expected.push_back(0xFF);

  auto bytes_or = rom.ReadByteVector(start, expected.size());
  ASSERT_TRUE(bytes_or.ok()) << bytes_or.status().message();
  EXPECT_EQ(bytes_or.value(), expected);
}

TEST(ExpandedMessageWriteTest, IsWriteFenceAware) {
  Rom rom = MakeRom(256);

  // Outer fence disallows the expanded region, so the writer must be blocked
  // if it routes through Rom::Write* (and doesn't bypass via raw pointers).
  yaze::rom::WriteFence outer;
  ASSERT_TRUE(outer.Allow(/*start=*/0, /*end=*/10, "deny").ok());
  yaze::rom::ScopedWriteFence scope(&rom, &outer);

  auto status = WriteExpandedTextData(&rom, /*start=*/100, /*end=*/120, {"A"});
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied);
}

}  // namespace yaze::editor

