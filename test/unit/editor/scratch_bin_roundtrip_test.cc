#include "app/editor/graphics/ui/scratch/graphics_scratch_view.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "gtest/gtest.h"
#include "imgui/imgui.h"

namespace yaze::editor {

struct GraphicsScratchBinTestPeer {
  static absl::Status Paint(GraphicsScratchView& view, uint8_t value) {
    auto before = view.library_;
    view.Selected()->pixels[0] = value;
    return view.Record(std::move(before));
  }
  static void BeginStroke(GraphicsScratchView& view) {
    view.stroke_ = view.library_;
  }
};

namespace {

class ScratchBinRoundtripTest : public ::testing::Test {
 protected:
  void SetUp() override {
    folder_ =
        std::filesystem::temp_directory_path() /
        ("yaze-bin-roundtrip-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(folder_));
    library_ = folder_ / "library.json";
    bin_ = folder_ / "sprite-page.bin";
    container_.assign(8192, '\x5a');
    // The imported slot is all zero; other slots are distinct recovery data.
    container_.replace(6144, 2048, std::string(2048, '\0'));
    Write(bin_, container_);
  }
  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(folder_, ec);
  }
  static void Write(const std::filesystem::path& path,
                    const std::string& value) {
    std::ofstream output(path, std::ios::binary);
    output.write(value.data(), value.size());
  }
  static std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
  }
  std::filesystem::path folder_, library_, bin_;
  std::string container_;
  GraphicsEditorState state_;
  UndoManager history_;
};

TEST_F(ScratchBinRoundtripTest,
       PatchesImportedRangeWithoutTruncatingOtherSlots) {
  GraphicsScratchView view(&state_, &history_, library_.string());
  ASSERT_TRUE(view.ImportBin(bin_.string(), 4, 6144).ok());
  ASSERT_TRUE(GraphicsScratchBinTestPeer::Paint(view, 9).ok());
  ASSERT_TRUE(view.ExportBin(bin_.string()).ok());
  const auto exported = Read(bin_);
  ASSERT_EQ(exported.size(), container_.size());
  EXPECT_EQ(exported.substr(0, 6144), container_.substr(0, 6144));
  EXPECT_EQ(static_cast<uint8_t>(exported[6144]), 0x80);
  EXPECT_EQ(static_cast<uint8_t>(exported[6144 + 17]), 0x80);
  EXPECT_EQ(exported.substr(6144 + 32), container_.substr(6144 + 32));
  EXPECT_FALSE(view.HasPendingChanges());
  GraphicsScratchView reopened(&state_, &history_, library_.string());
  ASSERT_EQ(reopened.library().assets.size(), 1u);
  const auto& asset = reopened.library().assets[0];
  EXPECT_EQ(asset.source_path,
            std::filesystem::weakly_canonical(bin_).string());
  EXPECT_EQ(asset.source_offset, 6144u);
  EXPECT_EQ(asset.original_chunk.size(), 2048u);
  ASSERT_TRUE(reopened.ExportBin(bin_.string()).ok());
}

TEST_F(ScratchBinRoundtripTest,
       UndoAfterExportKeepsCurrentPublicationBaseline) {
  GraphicsScratchView view(&state_, &history_, library_.string());
  ASSERT_TRUE(view.ImportBin(bin_.string(), 4, 6144).ok());
  ASSERT_TRUE(GraphicsScratchBinTestPeer::Paint(view, 9).ok());
  ASSERT_TRUE(view.ExportBin(bin_.string()).ok());
  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_EQ(view.library().assets[0].pixels[0], 0);
  ASSERT_TRUE(view.ExportBin(bin_.string()).ok());
  EXPECT_EQ(Read(bin_), container_);
  ASSERT_TRUE(history_.Redo().ok());
  ASSERT_TRUE(view.ExportBin(bin_.string()).ok());
  EXPECT_NE(Read(bin_), container_);
}

TEST_F(ScratchBinRoundtripTest, DifferentDepthsKeepIndependentSourceBaselines) {
  GraphicsScratchView view(&state_, &history_, library_.string());
  ASSERT_TRUE(view.ImportBin(bin_.string(), 4, 6144).ok());
  ASSERT_TRUE(view.ImportBin(bin_.string(), 3, 6144).ok());
  ASSERT_TRUE(GraphicsScratchBinTestPeer::Paint(view, 3).ok());
  ASSERT_TRUE(view.ExportBin(bin_.string()).ok());
  EXPECT_EQ(view.library().assets[0].original_chunk.size(), 2048u);
  EXPECT_EQ(view.library().assets[1].original_chunk.size(), 1536u);
  EXPECT_TRUE(GraphicsScratchLibrary::Parse(view.library().Serialize()).ok());
  GraphicsScratchView reopened(&state_, &history_, library_.string());
  ASSERT_EQ(reopened.library().assets.size(), 2u);
}

TEST_F(ScratchBinRoundtripTest, SourceDriftPreservesContainerAndEditedAsset) {
  GraphicsScratchView view(&state_, &history_, library_.string());
  ASSERT_TRUE(view.ImportBin(bin_.string(), 4, 6144).ok());
  ASSERT_TRUE(GraphicsScratchBinTestPeer::Paint(view, 9).ok());
  auto changed = container_;
  changed[6150] = 0x31;
  Write(bin_, changed);
  EXPECT_EQ(view.ExportBin(bin_.string()).code(), absl::StatusCode::kAborted);
  EXPECT_EQ(Read(bin_), changed);
  EXPECT_EQ(view.library().assets[0].pixels[0], 9);
  EXPECT_EQ(view.library().assets[0].original_chunk[6], 0);
}

TEST_F(ScratchBinRoundtripTest, UnrelatedContainerChangesSurviveRangeExport) {
  GraphicsScratchView view(&state_, &history_, library_.string());
  ASSERT_TRUE(view.ImportBin(bin_.string(), 4, 6144).ok());
  ASSERT_TRUE(GraphicsScratchBinTestPeer::Paint(view, 9).ok());
  auto changed = container_;
  changed[3] = 0x12;
  Write(bin_, changed);
  ASSERT_TRUE(view.ExportBin(bin_.string()).ok());
  EXPECT_EQ(Read(bin_)[3], 0x12);
  EXPECT_EQ(Read(bin_).size(), container_.size());
}

TEST_F(ScratchBinRoundtripTest,
       StandaloneExportCannotTruncateAnotherContainer) {
  GraphicsScratchView view(&state_, &history_, library_.string());
  ASSERT_TRUE(view.ImportBin(bin_.string(), 4, 6144).ok());
  const auto other = folder_ / "other.bin";
  Write(other, container_);
  EXPECT_FALSE(view.ExportBin(other.string()).ok());
  EXPECT_EQ(Read(other), container_);
  const auto standalone = folder_ / "standalone.bin";
  ASSERT_TRUE(view.ExportBin(standalone.string()).ok());
  EXPECT_EQ(Read(standalone).size(), 2048u);
  EXPECT_EQ(Read(bin_), container_);
}

TEST_F(ScratchBinRoundtripTest, RejectsLibraryAliasAndCallerProtectedOutput) {
  GraphicsScratchView view(&state_, &history_, library_.string());
  ASSERT_TRUE(view.ImportBin(bin_.string(), 4, 6144).ok());
  const auto library_before = Read(library_);
  EXPECT_FALSE(view.ExportBin(library_.string()).ok());
  EXPECT_EQ(Read(library_), library_before);
  view.SetExportValidator([](const std::string&) {
    return absl::FailedPreconditionError("Protected ROM output");
  });
  EXPECT_FALSE(view.ExportBin(bin_.string()).ok());
  EXPECT_EQ(Read(bin_), container_);
}

TEST_F(ScratchBinRoundtripTest, CommandsRejectUnfinishedStrokesBeforeMutation) {
  GraphicsScratchView view(&state_, &history_, library_.string());
  ASSERT_TRUE(view.ImportBin(bin_.string(), 4, 6144).ok());
  ImGui::CreateContext();
  ImGui::GetIO().MouseDown[0] = true;
  GraphicsScratchBinTestPeer::BeginStroke(view);
  EXPECT_FALSE(view.ImportBin(bin_.string(), 4, 6144).ok());
  EXPECT_FALSE(view.CaptureSheet().ok());
  EXPECT_FALSE(view.ApplyToSheet().ok());
  EXPECT_FALSE(view.ExportBin(bin_.string()).ok());
  EXPECT_EQ(view.library().assets.size(), 1u);
  EXPECT_EQ(Read(bin_), container_);
  ImGui::GetIO().MouseDown[0] = false;
  view.FinishGesture();
  EXPECT_FALSE(view.HasActiveStroke());
  ImGui::DestroyContext();
}

TEST_F(ScratchBinRoundtripTest, RejectsMalformedStructuredBinProvenance) {
  GraphicsScratchView view(&state_, &history_, library_.string());
  ASSERT_TRUE(view.ImportBin(bin_.string(), 4, 6144).ok());
  const auto valid = nlohmann::json::parse(view.library().Serialize());
  auto bad = valid;
  bad["assets"][0]["bin_source"]["original_chunk"][0] = 256;
  EXPECT_FALSE(GraphicsScratchLibrary::Parse(bad.dump()).ok());
  bad = valid;
  bad["assets"][0]["bin_source"]["offset"] = -1;
  EXPECT_FALSE(GraphicsScratchLibrary::Parse(bad.dump()).ok());
  bad = valid;
  bad["assets"][0]["bin_source"]["original_chunk"].erase(0);
  EXPECT_FALSE(GraphicsScratchLibrary::Parse(bad.dump()).ok());
}

}  // namespace
}  // namespace yaze::editor
