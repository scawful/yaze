#include <gtest/gtest.h>

#include "absl/strings/str_format.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "app/editor/overworld/overworld_editor.h"
#include "cli/handlers/game/overworld_commands.h"
#include "core/hack_manifest.h"
#include "core/source_artifact_publisher.h"
#include "rom/rom.h"
#include "rom/transaction.h"
#include "util/json.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace {

Json ContractJson() {
  const auto c = core::PrivateLyraExactSaveReservation();
  Json v = {{"kind", "lyra.private_exact_save_producer_reservation.v1"},
            {"scope", "private_candidate_only"},
            {"production_admission", false},
            {"source_rom_sha256", c.source_rom_sha256},
            {"tile32_id_range", c.tile32_id_range},
            {"stream_pc_range", c.stream_pc_range},
            {"dictionary_bases_pc", c.dictionary_bases_pc},
            {"producer_transfer",
             {{"from", "yaze.native_overworld_terrain"},
              {"to", "oracle.lyra_private_candidate"},
              {"native_terrain_policy", "refuse_changes"}}}};
  v["packed_preimages"] = Json::array();
  for (const auto& p : c.packed_preimages)
    v["packed_preimages"].push_back({{"pc", p.pc}, {"sha256", p.sha256}});
  v["stream_preimage"] = {{"pc", c.stream_preimage.pc},
                          {"sha256", c.stream_preimage.sha256}};
  return v;
}

std::string ManifestText(const Json& contract) {
  return Json({{"manifest_version", 3},
               {"hack_name", "Oracle private producer fixture"},
               {"lyra_private_reservation", contract}})
      .dump();
}

TEST(PrivateLyraManifestTest, AcceptsOnlyExactPrivateContract) {
  core::HackManifest manifest;
  EXPECT_TRUE(manifest.LoadFromString(ManifestText(ContractJson())).ok());
  ASSERT_TRUE(manifest.lyra_private_reservation());
  EXPECT_EQ(*manifest.lyra_private_reservation(),
            core::PrivateLyraExactSaveReservation());
  for (const char* field :
       {"kind", "scope", "producer_transfer", "packed_preimages"}) {
    auto bad = ContractJson();
    bad.erase(field);
    EXPECT_FALSE(manifest.LoadFromString(ManifestText(bad)).ok()) << field;
    EXPECT_FALSE(manifest.loaded());
  }
}

TEST(PrivateLyraManifestTest,
     RefusesGeometrySourcePreimageAndAdmissionChanges) {
  for (const char* field :
       {"source_rom_sha256", "tile32_id_range", "stream_pc_range",
        "dictionary_bases_pc", "production_admission", "stream_preimage"}) {
    auto bad = ContractJson();
    if (std::string(field) == "source_rom_sha256")
      bad[field] = std::string(64, '0');
    else if (std::string(field) == "production_admission")
      bad[field] = true;
    else if (std::string(field) == "stream_preimage")
      bad[field]["sha256"] = std::string(64, '0');
    else
      bad[field][0] = bad[field][0].get<uint32_t>() + 1;
    core::HackManifest manifest;
    EXPECT_FALSE(manifest.LoadFromString(ManifestText(bad)).ok()) << field;
  }
}

TEST(PrivateLyraManifestTest, OtherSourceCannotClaimTransfer) {
  core::HackManifest manifest;
  ASSERT_TRUE(manifest.LoadFromString(ManifestText(ContractJson())).ok());
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x400000, 0)).ok());
  EXPECT_FALSE(rom.has_lyra_private_reservation());
  EXPECT_TRUE(
      absl::IsFailedPrecondition(rom.AttachLyraPrivateReservation(manifest)));
  EXPECT_EQ(rom.vector(), std::vector<uint8_t>(0x400000, 0));
}

class PrivateLyraReservationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* path = std::getenv("YAZE_PRIVATE_LYRA_SOURCE_ROM");
    if (!path || !*path)
      GTEST_SKIP() << "Read-only exact source fixture requires "
                      "YAZE_PRIVATE_LYRA_SOURCE_ROM";
    std::ifstream file(path, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    bytes_ = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
    ASSERT_EQ(core::ComputeSourceArtifactSha256(std::string_view(
                  reinterpret_cast<const char*>(bytes_.data()), bytes_.size())),
              core::PrivateLyraExactSaveReservation().source_rom_sha256);
    Rom::LoadOptions options;
    options.load_resource_labels = false;
    ASSERT_TRUE(rom_.LoadFromData(bytes_, options).ok());
    ASSERT_TRUE(rom_.has_lyra_private_reservation());
  }
  Rom rom_;
  std::vector<uint8_t> bytes_;
};

TEST_F(PrivateLyraReservationTest,
       FreshLoadWithoutManifestAttachesAndNoOpSurvives) {
  EXPECT_TRUE(rom_.ValidateLyraPrivateReservation().ok());
  for (auto pc : {0x1D1FC, 0x133D3A, 0x137FFF, 0x17B2D, 0x1FE7D4})
    EXPECT_TRUE(rom_.WriteByte(pc, bytes_[pc]).ok());
  EXPECT_EQ(rom_.vector(), bytes_);
  core::HackManifest omitted;
  EXPECT_TRUE(rom_.AttachLyraPrivateReservation(omitted).ok());
  EXPECT_TRUE(rom_.has_lyra_private_reservation());
}

TEST_F(PrivateLyraReservationTest, SuppliedManifestAttachesAndCannotClear) {
  const char* path = std::getenv("YAZE_PRIVATE_LYRA_MANIFEST");
  if (!path || !*path)
    GTEST_SKIP() << "Set YAZE_PRIVATE_LYRA_MANIFEST to the review-bound "
                    "attachment wrapper";
  core::HackManifest manifest;
  ASSERT_TRUE(manifest.LoadFromFile(path).ok());
  ASSERT_TRUE(manifest.lyra_private_reservation());
  EXPECT_TRUE(rom_.AttachLyraPrivateReservation(manifest).ok());
  manifest.Clear();
  EXPECT_TRUE(rom_.AttachLyraPrivateReservation(manifest).ok());
  EXPECT_FALSE(rom_.WriteByte(0x133D3A, 1).ok());
  EXPECT_EQ(rom_.vector(), bytes_);
}

TEST_F(PrivateLyraReservationTest, RejectsAllWriteWidthsBeforeMutation) {
  for (uint32_t pc : {0x1D1FCu, 0x253FCu, 0x1F51FCu, 0x1FD1FCu, 0x133D3Au,
                      0x137FFFu, 0x1794Du, 0x17B2Du}) {
    EXPECT_FALSE(rom_.WriteByte(pc, bytes_[pc] ^ 1).ok());
    if (pc != 0x137FFF) {
      EXPECT_FALSE(rom_.WriteWord(pc, 0x1234).ok());
      EXPECT_FALSE(rom_.WriteLong(pc, 0x123456).ok());
    }
  }
  // A vector that starts in an allowed neighbor must be checked in full first.
  EXPECT_FALSE(rom_.WriteVector(0x133D39, {0xCC, 0xCC}).ok());
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_FALSE(rom_.dirty());
}

TEST_F(PrivateLyraReservationTest, AliasAndExternalNativeRowsStayProtected) {
  for (uint32_t table : {0x1794Du, 0x17B2Du}) {
    for (int map = 0; map < 160; ++map) {
      const auto pointer = rom_.ReadLong(table + map * 3);
      ASSERT_TRUE(pointer.ok());
      const uint32_t pc = ((*pointer & 0x7F0000) >> 1) | (*pointer & 0x7FFF);
      EXPECT_FALSE(rom_.WriteByte(pc, bytes_[pc] ^ 1).ok()) << map;
    }
  }
  EXPECT_EQ(rom_.vector(), bytes_);
}

TEST_F(PrivateLyraReservationTest, CopyAssignmentReloadAndCloseCannotDetach) {
  Rom copy(rom_);
  EXPECT_FALSE(copy.WriteByte(0x133D3A, 1).ok());
  Rom empty;
  copy = empty;
  EXPECT_TRUE(copy.has_lyra_private_reservation());
  EXPECT_FALSE(copy.ValidateLyraPrivateReservation().ok());
  EXPECT_TRUE(copy.LoadFromData(bytes_).ok());
  auto changed = bytes_;
  changed[0x133D3A] = 1;
  EXPECT_FALSE(copy.LoadFromData(changed).ok());
  EXPECT_EQ(copy.vector(), bytes_);
  EXPECT_TRUE(copy.has_lyra_private_reservation());
  copy.Close();
  EXPECT_TRUE(copy.has_lyra_private_reservation());
  EXPECT_FALSE(copy.ValidateLyraPrivateReservation().ok());
}

TEST_F(PrivateLyraReservationTest, RawMutationDetectedAndTransactionRestores) {
  {
    ScopedRomTransaction transaction(rom_);
    rom_.mutable_vector()[0x133D3A] = 1;
    EXPECT_FALSE(rom_.ValidateLyraPrivateReservation().ok());
    // No SaveToFile call: this fixture never writes a ROM to disk.
  }
  EXPECT_TRUE(rom_.ValidateLyraPrivateReservation().ok());
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_FALSE(rom_.dirty());
  std::array<uint16_t, 256> tiles{};
  EXPECT_FALSE(
      rom_.SaveLyraA0TerrainAs(tiles, "/unused-private-probe.sfc").ok());
}

TEST_F(PrivateLyraReservationTest, TransactionRestoresClosedAttachedRom) {
  rom_.set_filename("private-in-memory-fixture");
  const auto filename = rom_.filename();
  const auto dirty = rom_.dirty();
  {
    ScopedRomTransaction transaction(rom_);
    rom_.Close();
    EXPECT_EQ(rom_.size(), 0u);
  }
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_EQ(rom_.size(), bytes_.size());
  EXPECT_EQ(rom_.filename(), filename);
  EXPECT_EQ(rom_.dirty(), dirty);
  EXPECT_TRUE(rom_.has_lyra_private_reservation());
  EXPECT_TRUE(rom_.ValidateLyraPrivateReservation().ok());
}

TEST_F(PrivateLyraReservationTest, DirectProducersNoOpAndChangedGridRefusal) {
  zelda3::Overworld ow(&rom_);
  ASSERT_TRUE(ow.Load(&rom_).ok());
  using Producer = absl::Status (zelda3::Overworld::*)();
  const Producer methods[] = {&zelda3::Overworld::CreateTile32Tilemap,
                              &zelda3::Overworld::SaveMap32Tiles,
                              &zelda3::Overworld::SaveMap32Expanded,
                              &zelda3::Overworld::SaveMap16Tiles,
                              &zelda3::Overworld::SaveMap16Expanded,
                              &zelda3::Overworld::SaveOverworldMaps};
  for (auto method : methods)
    EXPECT_TRUE((ow.*method)().ok());
  EXPECT_EQ(rom_.vector(), bytes_);
  ow.mutable_map_tiles()->light_world[0][0] ^= 1;
  for (auto method : methods)
    EXPECT_TRUE(absl::IsFailedPrecondition((ow.*method)()));
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_FALSE(rom_.dirty());
}

TEST_F(PrivateLyraReservationTest, WholeOverworldNoOpSavePreservesBytes) {
  zelda3::Overworld ow(&rom_);
  ASSERT_TRUE(ow.Load(&rom_).ok());
  // Re-reading a formerly lazy overlay is a cache operation, not an edit.
  ASSERT_TRUE(ow.mutable_overworld_map(0x37)->LoadOverlay().ok());
  const auto status = ow.Save(&rom_);
  EXPECT_TRUE(status.ok()) << status;
  for (size_t pc = 0; pc < bytes_.size();) {
    if (rom_.vector()[pc] == bytes_[pc]) {
      ++pc;
      continue;
    }
    const auto begin = pc++;
    while (pc < bytes_.size() && rom_.vector()[pc] != bytes_[pc])
      ++pc;
    std::fprintf(stderr, "WholeSaveDelta [0x%06zx,0x%06zx)\n", begin, pc);
  }
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_TRUE(rom_.ValidateLyraPrivateReservation().ok());
}

TEST_F(PrivateLyraReservationTest,
       EditedOverlayRefusedBeforeWholeSaveMutation) {
  zelda3::Overworld ow(&rom_);
  ASSERT_TRUE(ow.Load(&rom_).ok());
  auto* map = ow.mutable_overworld_map(0);
  ASSERT_NE(map, nullptr);
  map->set_subscreen_overlay(map->subscreen_overlay() ^ 1);
  EXPECT_TRUE(absl::IsFailedPrecondition(ow.SaveMapOverlays()));
  EXPECT_TRUE(absl::IsFailedPrecondition(ow.Save(&rom_)));
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_FALSE(rom_.dirty());
}

TEST_F(PrivateLyraReservationTest, Tile16EditRefusedBeforeSerialization) {
  zelda3::Overworld ow(&rom_);
  ASSERT_TRUE(ow.Load(&rom_).ok());
  (*ow.mutable_tiles16())[0].tile0_.id_ ^= 1;
  EXPECT_FALSE(ow.SaveMap16Tiles().ok());
  EXPECT_FALSE(ow.SaveMap16Expanded().ok());
  EXPECT_EQ(rom_.vector(), bytes_);
}

TEST_F(PrivateLyraReservationTest, EditorNoOpTerrainSaveRemainsUsable) {
  editor::OverworldEditor editor(&rom_);
  ASSERT_TRUE(editor.overworld().Load(&rom_).ok());
  const auto status = editor.Save();
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_TRUE(editor.Save().ok());
  EXPECT_TRUE(rom_.ValidateLyraPrivateReservation().ok());
  EXPECT_EQ(rom_.vector(), bytes_);
}

TEST_F(PrivateLyraReservationTest,
       EditorRefusesChangedNativeGridBeforeMutation) {
  editor::OverworldEditor editor(&rom_);
  ASSERT_TRUE(editor.overworld().Load(&rom_).ok());
  editor.overworld().mutable_map_tiles()->light_world[0][0] ^= 1;
  EXPECT_TRUE(absl::IsFailedPrecondition(editor.Save()));
  EXPECT_EQ(rom_.vector(), bytes_);
}

TEST_F(PrivateLyraReservationTest, CliNoManifestDryRunCannotBypassAttachment) {
  zelda3::Overworld ow(&rom_);
  ASSERT_TRUE(ow.Load(&rom_).ok());
  const auto before = ow.map_tiles().light_world[0][0];
  cli::handlers::OverworldSetTileCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--map=0", "--x=0", "--y=0",
                   absl::StrFormat("--tile=0x%X", before ^ 1), "--format=json"},
                  &rom_, &output);
  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_FALSE(rom_.dirty());
}

}  // namespace
}  // namespace yaze
