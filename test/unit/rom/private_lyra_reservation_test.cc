#include <gtest/gtest.h>

#include "absl/strings/str_format.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "app/editor/editor_manager.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/system/session/collision_source_pairing.h"
#include "app/editor/system/session/rom_lifecycle_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "cli/handlers/game/overworld_commands.h"
#include "core/hack_manifest.h"
#include "core/source_artifact_publisher.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "rom/transaction.h"
#include "rom/write_fence.h"
#include "util/json.h"
#include "util/rom_hash.h"
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

Json CandidateContractJson() {
  const auto c = core::PrivateLyraExactCandidatePersistence();
  return {{"kind", "lyra.private_exact_candidate_persistence.v1"},
          {"scope", "private_candidate_only"},
          {"production_admission", false},
          {"source_rom_sha256", c.source_rom_sha256},
          {"candidate_rom_sha256", c.candidate_rom_sha256},
          {"write_plan_sha256", c.write_plan_sha256},
          {"source_attachment_sha256", c.source_attachment_sha256},
          {"image_size", c.image_size},
          {"save_policy", "byte_identical_no_op_only"}};
}

std::string CandidateManifestText(const Json& contract) {
  return Json({{"manifest_version", 3},
               {"hack_name", "Oracle private candidate persistence fixture"},
               {"lyra_private_candidate_persistence", contract}})
      .dump();
}

TEST(PrivateLyraCandidateManifestTest, AcceptsOnlyFixedPersistenceDescriptor) {
  core::HackManifest manifest;
  ASSERT_TRUE(
      manifest.LoadFromString(CandidateManifestText(CandidateContractJson()))
          .ok());
  ASSERT_TRUE(manifest.lyra_private_candidate_persistence());
  EXPECT_EQ(*manifest.lyra_private_candidate_persistence(),
            core::PrivateLyraExactCandidatePersistence());
  EXPECT_FALSE(manifest.lyra_private_reservation());
  const auto contract = CandidateContractJson();
  for (auto it = contract.begin(); it != contract.end(); ++it) {
    auto bad = contract;
    bad.erase(it.key());
    EXPECT_FALSE(manifest.LoadFromString(CandidateManifestText(bad)).ok())
        << it.key();
    EXPECT_FALSE(manifest.loaded());
  }
}

TEST(PrivateLyraCandidateManifestTest, RejectsRetargetedOrExpandedCapability) {
  const auto contract = CandidateContractJson();
  for (auto it = contract.begin(); it != contract.end(); ++it) {
    auto bad = contract;
    if (it.key() == "production_admission")
      bad[it.key()] = true;
    else if (it.key() == "image_size")
      bad[it.key()] = 0x400001;
    else
      bad[it.key()] = "wrong";
    core::HackManifest manifest;
    EXPECT_FALSE(manifest.LoadFromString(CandidateManifestText(bad)).ok())
        << it.key();
  }
  for (const Json& bad : {Json(4194304.0), Json(true), Json("4194304")}) {
    auto v = contract;
    v["image_size"] = bad;
    core::HackManifest manifest;
    EXPECT_FALSE(manifest.LoadFromString(CandidateManifestText(v)).ok());
  }
  for (const char* field : {"producer_transfer", "allocation"}) {
    auto v = contract;
    v[field] = Json::object();
    core::HackManifest manifest;
    EXPECT_FALSE(manifest.LoadFromString(CandidateManifestText(v)).ok());
  }
  auto dual = Json::parse(CandidateManifestText(contract));
  dual["lyra_private_reservation"] = ContractJson();
  core::HackManifest manifest;
  EXPECT_FALSE(manifest.LoadFromString(dual.dump()).ok());
}

TEST(PrivateLyraCandidateManifestTest, UnknownImageCannotClaimPersistence) {
  core::HackManifest manifest;
  ASSERT_TRUE(
      manifest.LoadFromString(CandidateManifestText(CandidateContractJson()))
          .ok());
  Rom rom;
  const std::vector<uint8_t> unknown(0x400000, 0);
  ASSERT_TRUE(rom.LoadFromData(unknown).ok());
  EXPECT_FALSE(rom.has_lyra_private_reservation());
  EXPECT_TRUE(
      absl::IsFailedPrecondition(rom.AttachLyraPrivateReservation(manifest)));
  EXPECT_EQ(rom.vector(), unknown);
}

class PrivateLyraCandidateTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* path = std::getenv("YAZE_PRIVATE_LYRA_CANDIDATE_ROM");
    if (!path || !*path)
      GTEST_SKIP() << "Read-only exact candidate fixture requires "
                      "YAZE_PRIVATE_LYRA_CANDIDATE_ROM";
    path_ = path;
    std::ifstream file(path_, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    bytes_ = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
    ASSERT_EQ(bytes_.size(), 0x400000u);
    ASSERT_EQ(
        core::ComputeSourceArtifactSha256(std::string_view(
            reinterpret_cast<const char*>(bytes_.data()), bytes_.size())),
        core::PrivateLyraExactCandidatePersistence().candidate_rom_sha256);
    options_.load_resource_labels = false;
    ASSERT_TRUE(rom_.LoadFromData(bytes_, options_).ok());
    ASSERT_TRUE(rom_.is_lyra_private_candidate());
  }
  void TearDown() override {
    // Project parsing registers a borrowed pointer; keep it out of later cases.
    zelda3::GetResourceLabels().SetHackManifest(nullptr);
  }
  Rom rom_;
  Rom::LoadOptions options_;
  std::vector<uint8_t> bytes_;
  std::string path_;
};

TEST_F(PrivateLyraCandidateTest, FreshFileLoadAndManifestOmissionStayAttached) {
  Rom cold;
  ASSERT_TRUE(cold.LoadFromFile(path_, options_).ok());
  EXPECT_TRUE(cold.is_lyra_private_candidate());
  EXPECT_EQ(cold.vector(), bytes_);
  const auto title = cold.title();
  const auto filename = cold.filename();
  core::HackManifest manifest;
  const char* path = std::getenv("YAZE_PRIVATE_LYRA_CANDIDATE_MANIFEST");
  ASSERT_NE(path, nullptr);
  ASSERT_TRUE(manifest.LoadFromFile(path).ok());
  ASSERT_TRUE(cold.AttachLyraPrivateReservation(manifest).ok());
  manifest.Clear();
  EXPECT_TRUE(cold.AttachLyraPrivateReservation(manifest).ok());
  EXPECT_EQ(cold.title(), title);
  EXPECT_EQ(cold.filename(), filename);
  EXPECT_TRUE(cold.ValidateLyraPrivateReservation().ok());
}

TEST_F(PrivateLyraCandidateTest, FailedFileLoadsPreserveMetadataAndImage) {
  ASSERT_TRUE(rom_.LoadFromFile(path_, options_).ok());
  const auto filename = rom_.filename();
  const auto title = rom_.title();
  const auto revision = rom_.object_tile_revision();
  const char* manifest_path =
      std::getenv("YAZE_PRIVATE_LYRA_CANDIDATE_MANIFEST");
  ASSERT_NE(manifest_path, nullptr);
  EXPECT_FALSE(rom_.LoadFromFile("", options_).ok());
  EXPECT_FALSE(
      rom_.LoadFromFile(path_ + ".missing-candidate-fixture", options_).ok());
  // The existing JSON is read-only and smaller than the ROM file size cap.
  EXPECT_FALSE(rom_.LoadFromFile(manifest_path, options_).ok());
  EXPECT_EQ(rom_.filename(), filename);
  EXPECT_EQ(rom_.title(), title);
  EXPECT_EQ(rom_.object_tile_revision(), revision);
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_TRUE(rom_.ValidateLyraPrivateReservation().ok());
}

TEST_F(PrivateLyraCandidateTest, NoOpWritesAndDeniedTransactionKeepCleanImage) {
  for (uint32_t pc : {0u, 0x10000u, 0x1794Du, 0x133D3Au, 0x3FFFFCu}) {
    ASSERT_TRUE(rom_.WriteByte(pc, bytes_[pc]).ok());
    ASSERT_TRUE(rom_.WriteWord(pc, *rom_.ReadWord(pc)).ok());
    ASSERT_TRUE(rom_.WriteLong(pc, *rom_.ReadLong(pc)).ok());
    ASSERT_TRUE(rom_.WriteVector(pc, {bytes_[pc], bytes_[pc + 1]}).ok());
    EXPECT_FALSE(rom_.WriteByte(pc, bytes_[pc] ^ 1).ok());
    EXPECT_FALSE(rom_.WriteWord(pc, *rom_.ReadWord(pc) ^ 1).ok());
    EXPECT_FALSE(rom_.WriteLong(pc, *rom_.ReadLong(pc) ^ 1).ok());
    EXPECT_FALSE(rom_.WriteVector(pc, {uint8_t(bytes_[pc] ^ 1)}).ok());
  }
  EXPECT_FALSE(rom_.WriteTransaction(
                       Rom::WriteAction{0x10000, uint8_t(bytes_[0x10000] ^ 1)},
                       Rom::WriteAction{0x10001, bytes_[0x10001]})
                   .ok());
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_TRUE(rom_.ValidateLyraPrivateReservation().ok());
}

TEST_F(PrivateLyraCandidateTest,
       BothRolesRejectCrossRoleManifestAndFileReload) {
  const char* source_path = std::getenv("YAZE_PRIVATE_LYRA_SOURCE_ROM");
  ASSERT_NE(source_path, nullptr);
  Rom source;
  ASSERT_TRUE(source.LoadFromFile(source_path, options_).ok());
  ASSERT_TRUE(source.has_lyra_private_reservation());
  ASSERT_FALSE(source.is_lyra_private_candidate());
  const auto source_bytes = source.vector();
  const auto source_filename = source.filename();
  core::HackManifest source_manifest;
  ASSERT_TRUE(
      source_manifest.LoadFromString(ManifestText(ContractJson())).ok());
  core::HackManifest candidate_manifest;
  ASSERT_TRUE(
      candidate_manifest
          .LoadFromString(CandidateManifestText(CandidateContractJson()))
          .ok());
  EXPECT_FALSE(rom_.AttachLyraPrivateReservation(source_manifest).ok());
  EXPECT_FALSE(source.AttachLyraPrivateReservation(candidate_manifest).ok());
  EXPECT_FALSE(rom_.LoadFromFile(source_path, options_).ok());
  EXPECT_FALSE(source.LoadFromFile(path_, options_).ok());
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_EQ(source.vector(), source_bytes);
  EXPECT_EQ(source.filename(), source_filename);
  EXPECT_TRUE(rom_.is_lyra_private_candidate());
  EXPECT_FALSE(source.is_lyra_private_candidate());
  // The original source's accepted copier-header normalization is unchanged.
  std::vector<uint8_t> headered(kCopierHeaderSize, 0);
  headered.insert(headered.end(), source_bytes.begin(), source_bytes.end());
  Rom headered_source;
  ASSERT_TRUE(headered_source.LoadFromData(headered, options_).ok());
  EXPECT_EQ(headered_source.vector(), source_bytes);
  EXPECT_TRUE(headered_source.has_lyra_private_reservation());
  EXPECT_FALSE(headered_source.is_lyra_private_candidate());
}

TEST_F(PrivateLyraCandidateTest,
       HeaderedCandidateIsRejectedBeforeLoadMutation) {
  std::vector<uint8_t> headered(kCopierHeaderSize, 0);
  headered.insert(headered.end(), bytes_.begin(), bytes_.end());
  for (bool strip : {true, false}) {
    auto options = options_;
    options.strip_header = strip;
    Rom cold;
    EXPECT_TRUE(
        absl::IsFailedPrecondition(cold.LoadFromData(headered, options)));
    EXPECT_FALSE(cold.is_loaded());
    EXPECT_FALSE(cold.has_lyra_private_reservation());
    EXPECT_TRUE(
        absl::IsFailedPrecondition(rom_.LoadFromData(headered, options)));
    EXPECT_EQ(rom_.vector(), bytes_);
    EXPECT_TRUE(rom_.ValidateLyraPrivateReservation().ok());
  }
}

TEST_F(PrivateLyraCandidateTest, CopyAssignmentCloseAndReloadCannotRetarget) {
  Rom copy(rom_);
  EXPECT_TRUE(copy.is_lyra_private_candidate());
  Rom assigned;
  assigned = rom_;
  EXPECT_TRUE(assigned.is_lyra_private_candidate());
  copy = Rom();
  EXPECT_TRUE(copy.is_lyra_private_candidate());
  EXPECT_FALSE(copy.ValidateLyraPrivateReservation().ok());
  ASSERT_TRUE(copy.LoadFromData(bytes_, options_).ok());
  auto changed = bytes_;
  changed[0x10000] ^= 1;
  EXPECT_FALSE(copy.LoadFromData(changed, options_).ok());
  EXPECT_EQ(copy.vector(), bytes_);
  copy.Close();
  EXPECT_TRUE(copy.is_lyra_private_candidate());
  ASSERT_TRUE(copy.LoadFromData(bytes_, options_).ok());
  EXPECT_TRUE(copy.ValidateLyraPrivateReservation().ok());
  Rom cold_mutant;
  ASSERT_TRUE(cold_mutant.LoadFromData(changed, options_).ok());
  EXPECT_FALSE(cold_mutant.has_lyra_private_reservation());
  core::HackManifest manifest;
  ASSERT_TRUE(
      manifest.LoadFromString(CandidateManifestText(CandidateContractJson()))
          .ok());
  EXPECT_FALSE(cold_mutant.AttachLyraPrivateReservation(manifest).ok());
}

TEST_F(PrivateLyraCandidateTest,
       RawMutationFailsBeforeIoAndTransactionRestores) {
  const auto forbidden_parent = std::filesystem::path(
      "/Users/scawful/.context/projects/oracle-of-secrets/scratchpad/handoffs/"
      "lyra_current_project_2026-10-22/full-sea/candidate-persistence/"
      "forbidden-not-created");
  const auto forbidden_path = forbidden_parent / "candidate-negative-no-io.sfc";
  ASSERT_FALSE(std::filesystem::exists(forbidden_parent));
  {
    ScopedRomTransaction transaction(rom_);
    rom_.mutable_vector()[0x10000] ^= 1;
    EXPECT_FALSE(rom_.ValidateLyraPrivateReservation().ok());
    Rom::SaveSettings settings;
    settings.filename = forbidden_path.string();
    const auto status = rom_.SaveToFile(settings);
    EXPECT_TRUE(absl::IsFailedPrecondition(status));
    EXPECT_NE(status.message().find("Private Lyra"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(forbidden_parent));
  }
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_TRUE(rom_.ValidateLyraPrivateReservation().ok());
  EXPECT_FALSE(rom_.dirty());
}

TEST_F(PrivateLyraCandidateTest,
       FileManagerRefusesInvalidCandidateBeforeBackup) {
  const auto parent = std::filesystem::path(
      "/Users/scawful/.context/projects/oracle-of-secrets/scratchpad/handoffs/"
      "lyra_current_project_2026-10-22/full-sea/candidate-persistence/"
      "forbidden-not-created");
  ASSERT_FALSE(std::filesystem::exists(parent));
  rom_.set_filename((parent / "candidate-file-manager-no-io.sfc").string());
  rom_.mutable_vector()[0x10000] ^= 1;
  editor::RomFileManager manager(nullptr);
  manager.SetBackupBeforeSave(true);
  manager.SetBackupFolder((parent / "Backups").string());
  const auto status = manager.SaveRom(&rom_);
  EXPECT_TRUE(absl::IsFailedPrecondition(status));
  EXPECT_NE(status.message().find("Private Lyra"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(parent));
  const auto save_as =
      manager.SaveRomAs(&rom_, (parent / "save-as.sfc").string());
  EXPECT_TRUE(absl::IsFailedPrecondition(save_as));
  EXPECT_NE(save_as.message().find("Private Lyra"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(parent));
}

TEST_F(PrivateLyraCandidateTest,
       CollisionPairPublisherRefusesRawMutationBeforeIo) {
  const auto parent = std::filesystem::path(
      "/Users/scawful/.context/projects/oracle-of-secrets/scratchpad/handoffs/"
      "lyra_current_project_2026-10-22/full-sea/candidate-persistence/"
      "forbidden-not-created");
  ASSERT_FALSE(std::filesystem::exists(parent));
  rom_.set_filename((parent / "candidate-pair-no-io.sfc").string());
  rom_.mutable_vector()[0x10000] ^= 1;
  const auto status = editor::SaveRomWithCollisionSource(
      &rom_, parent / "custom_collision.json");
  EXPECT_TRUE(absl::IsFailedPrecondition(status));
  EXPECT_NE(status.message().find("Private Lyra"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(parent));
}

TEST_F(PrivateLyraCandidateTest, RawRestoreMetadataMismatchAndCloseRollback) {
  Rom mismatched(rom_);
  mismatched.Close();
  mismatched.mutable_vector() = bytes_;
  ASSERT_EQ(mismatched.size(), 0u);
  EXPECT_FALSE(mismatched.ValidateLyraPrivateReservation().ok());
  rom_.set_filename("candidate-in-memory-fixture");
  const auto filename = rom_.filename();
  {
    ScopedRomTransaction transaction(rom_);
    rom_.Close();
    EXPECT_FALSE(rom_.ValidateLyraPrivateReservation().ok());
  }
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_EQ(rom_.size(), bytes_.size());
  EXPECT_EQ(rom_.filename(), filename);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_TRUE(rom_.is_lyra_private_candidate());
  EXPECT_TRUE(rom_.ValidateLyraPrivateReservation().ok());
}

TEST_F(PrivateLyraCandidateTest,
       AssignmentRollbackRestoresPhysicalIdentityAndScopes) {
  ASSERT_TRUE(rom_.LoadFromFile(path_, options_).ok());
  const auto filename = rom_.filename();
  const auto short_name = rom_.short_name();
  const auto title = rom_.title();
  std::vector<uint8_t> unrelated(0x8000 + kCopierHeaderSize, 0);
  Rom headered;
  ASSERT_TRUE(headered.LoadFromData(unrelated, options_).ok());
  ASSERT_TRUE(headered.has_copier_header());
  const char* source_path = std::getenv("YAZE_PRIVATE_LYRA_SOURCE_ROM");
  ASSERT_NE(source_path, nullptr);
  Rom source;
  ASSERT_TRUE(source.LoadFromFile(source_path, options_).ok());
  for (const Rom* replacement : {&headered, &source}) {
    {
      ScopedRomTransaction transaction(rom_);
      rom_ = *replacement;
      EXPECT_TRUE(rom_.is_lyra_private_candidate());
      EXPECT_FALSE(rom_.ValidateLyraPrivateReservation().ok());
    }
    EXPECT_EQ(rom_.vector(), bytes_);
    EXPECT_EQ(rom_.size(), bytes_.size());
    EXPECT_EQ(rom_.filename(), filename);
    EXPECT_EQ(rom_.short_name(), short_name);
    EXPECT_EQ(rom_.title(), title);
    EXPECT_FALSE(rom_.has_copier_header());
    EXPECT_FALSE(rom_.dirty());
    EXPECT_TRUE(rom_.ValidateLyraPrivateReservation().ok());
  }
  {
    ScopedRomReadOnly read_only(rom_);
    ScopedRomTransaction transaction(rom_);
    rom_ = Rom();
    transaction.Rollback();
    EXPECT_TRUE(rom_.ValidateLyraPrivateReservation().ok());
    EXPECT_TRUE(absl::IsPermissionDenied(rom_.WriteByte(0, bytes_[0])));
  }
  {
    rom::WriteFence fence;
    rom::ScopedWriteFence active_fence(&rom_, &fence);
    ScopedRomTransaction transaction(rom_);
    rom_ = headered;
    transaction.Rollback();
    EXPECT_TRUE(absl::IsPermissionDenied(rom_.WriteByte(0, bytes_[0])));
  }
  EXPECT_TRUE(rom_.WriteByte(0, bytes_[0]).ok());
  EXPECT_TRUE(rom_.ValidateLyraPrivateReservation().ok());
}

TEST_F(PrivateLyraCandidateTest, WholeAndEditorSaveNoOpsPreserveWholeImage) {
  zelda3::Overworld ow(&rom_);
  ASSERT_TRUE(ow.Load(&rom_).ok());
  auto status = ow.Save(&rom_);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_EQ(rom_.vector(), bytes_);
  editor::OverworldEditor editor(&rom_);
  ASSERT_TRUE(editor.overworld().Load(&rom_).ok());
  status = editor.Save();
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_TRUE(rom_.ValidateLyraPrivateReservation().ok());
}

TEST_F(PrivateLyraCandidateTest, NativeAndOverlayEditsRefusedBeforeMutation) {
  editor::OverworldEditor editor(&rom_);
  ASSERT_TRUE(editor.overworld().Load(&rom_).ok());
  editor.overworld().mutable_map_tiles()->light_world[0][0] ^= 1;
  EXPECT_TRUE(absl::IsFailedPrecondition(editor.Save()));
  EXPECT_TRUE(
      absl::IsFailedPrecondition(editor.overworld().SaveOverworldMaps()));
  EXPECT_EQ(rom_.vector(), bytes_);
  zelda3::Overworld ow(&rom_);
  ASSERT_TRUE(ow.Load(&rom_).ok());
  auto* map = ow.mutable_overworld_map(0);
  map->set_subscreen_overlay(map->subscreen_overlay() ^ 1);
  EXPECT_TRUE(absl::IsFailedPrecondition(ow.SaveMapOverlays()));
  EXPECT_TRUE(absl::IsFailedPrecondition(ow.Save(&rom_)));
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_FALSE(rom_.dirty());
}

TEST_F(PrivateLyraCandidateTest, NormalProjectManifestAndLifecyclePreflight) {
  const char* manifest_path =
      std::getenv("YAZE_PRIVATE_LYRA_CANDIDATE_MANIFEST");
  ASSERT_NE(manifest_path, nullptr);
  project::YazeProject project;
  ASSERT_TRUE(
      project
          .LoadFromString(
              absl::StrFormat("[project]\nname=Private candidate\n[files]\n"
                              "rom_filename=%s\nhack_manifest_file=%s\n",
                              path_, manifest_path),
              (std::filesystem::path(manifest_path).parent_path() /
               "in-memory-candidate.yaze")
                  .string())
          .ok());
  ASSERT_TRUE(project.hack_manifest.loaded());
  ASSERT_TRUE(project.hack_manifest.lyra_private_candidate_persistence());
  Rom cold;
  ASSERT_TRUE(cold.LoadFromFile(project.rom_filename, options_).ok());
  editor::RomLifecycleManager lifecycle;
  lifecycle.SetProjectContext(&project);
  EXPECT_TRUE(lifecycle.CheckRomOpenPolicy(&cold).ok());
  EXPECT_TRUE(lifecycle.CheckRomWritePolicy(&cold).ok());
  EXPECT_TRUE(lifecycle.CheckOracleRomSafetyPreSave(&cold).ok());
  editor::EditorDependencies deps;
  deps.project = &project;
  editor::OverworldEditor editor(&cold, deps);
  ASSERT_TRUE(editor.overworld().Load(&cold).ok());
  EXPECT_TRUE(editor.Save().ok());
  EXPECT_EQ(cold.vector(), bytes_);
  project.hack_manifest.Clear();
  EXPECT_FALSE(lifecycle.CheckOracleRomSafetyPreSave(&cold).ok());
  project.hack_manifest_file.clear();
  EXPECT_TRUE(lifecycle.CheckOracleRomSafetyPreSave(&cold).ok());
  EXPECT_TRUE(cold.is_lyra_private_candidate());
}

// These are future native disk proofs, not part of the in-memory host run.
// Setting the exact case gate is required in addition to selecting its filter.
class PrivateLyraCandidateDiskRoundtripTest : public ::testing::Test {
 protected:
  static std::filesystem::path Phase() {
    return "/Users/scawful/.context/projects/oracle-of-secrets/scratchpad/"
           "handoffs/lyra_current_project_2026-10-22/full-sea/"
           "candidate-persistence";
  }
  static std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
  }
  static void AssertRegularUnaliased(const std::filesystem::path& path) {
    ASSERT_TRUE(std::filesystem::is_regular_file(path)) << path;
    ASSERT_EQ(std::filesystem::canonical(path), path.lexically_normal());
    ASSERT_EQ(std::filesystem::hard_link_count(path), 1u);
  }
  void SetUp() override {
    const std::string name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();
    case_ = name == "SaveAsAndColdReopen" ? "save_as" : "ordinary_save";
    const char* gate = std::getenv("YAZE_PRIVATE_LYRA_T3_DISK_CASE");
    if (!gate || case_ != gate)
      GTEST_SKIP() << "Disk proof requires explicit exact-case gate "
                      "YAZE_PRIVATE_LYRA_T3_DISK_CASE="
                   << case_;
    const auto app_data = Phase() / "isolated-settings/app-data";
    const auto documents = Phase() / "isolated-settings/documents";
    ASSERT_NE(std::getenv("YAZE_APP_DATA_DIR"), nullptr);
    ASSERT_NE(std::getenv("YAZE_USER_DOCUMENTS_DIR"), nullptr);
    ASSERT_EQ(std::string(std::getenv("YAZE_APP_DATA_DIR")), app_data.string());
    ASSERT_EQ(std::string(std::getenv("YAZE_USER_DOCUMENTS_DIR")),
              documents.string());
    ASSERT_EQ(std::filesystem::canonical(Phase()), Phase());
    ASSERT_EQ(std::filesystem::canonical(Phase() / "isolated-settings"),
              Phase() / "isolated-settings");
    ASSERT_EQ(std::filesystem::canonical(app_data), app_data);
    ASSERT_EQ(std::filesystem::canonical(documents), documents);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             Phase() / "isolated-settings")) {
      ASSERT_FALSE(entry.is_symlink()) << entry.path();
      if (entry.is_regular_file())
        ASSERT_EQ(std::filesystem::hard_link_count(entry.path()), 1u);
    }
    ASSERT_NO_FATAL_FAILURE(AssertRegularUnaliased(ProjectPath()));
    ASSERT_NO_FATAL_FAILURE(AssertRegularUnaliased(InputPath()));
    ASSERT_NO_FATAL_FAILURE(AssertRegularUnaliased(ManifestPath()));
    ASSERT_NO_FATAL_FAILURE(AssertRegularUnaliased(CollisionPath()));
    ASSERT_EQ(
        core::ComputeSourceArtifactSha256(ReadFile(ProjectPath())),
        "4eb30bfe8c75425a25aef3cbdb27de9eb9e242df4c6311cf71bc251a8b3d8dba");
    ASSERT_EQ(
        core::ComputeSourceArtifactSha256(ReadFile(ManifestPath())),
        "f74aa255a0cd3ecba3e624b5fbe43434f6dda53ff0da20a93621393bc7b026ce");
    ASSERT_EQ(
        core::ComputeSourceArtifactSha256(ReadFile(InputPath())),
        core::PrivateLyraExactCandidatePersistence().candidate_rom_sha256);
    ASSERT_EQ(std::filesystem::file_size(InputPath()), 0x400000u);
    project_before_ = ReadFile(ProjectPath());
    manifest_before_ = ReadFile(ManifestPath());
    collision_before_ = ReadFile(CollisionPath());
    imgui_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_);
    ImGui::GetIO().IniFilename = nullptr;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }
  void TearDown() override {
    zelda3::GetResourceLabels().SetHackManifest(nullptr);
    if (imgui_)
      ImGui::DestroyContext(imgui_);
  }
  static std::filesystem::path ProjectPath() {
    return Phase() / "project/Oracle-of-Secrets-Lyra-Candidate-T3.yaze";
  }
  static std::filesystem::path InputPath() {
    return Phase() / "project/oracle-private-full32.sfc";
  }
  static std::filesystem::path ManifestPath() {
    return Phase() / "project/resources/hack_manifest.json";
  }
  static std::filesystem::path CollisionPath() {
    return Phase() / "project/resources/custom_collision.json";
  }
  void OpenProject(editor::EditorManager& manager,
                   gfx::NullRenderer& renderer) {
    manager.user_settings().SetSettingsFilePathForTesting(
        (Phase() / "isolated-settings" / (case_ + ".settings.json")).string());
    manager.Initialize(&renderer, "");
    ASSERT_EQ(ImGui::GetIO().IniFilename, nullptr);
    manager.SetAssetLoadMode(AssetLoadMode::kLazy);
    ASSERT_TRUE(manager.OpenRomOrProject(ProjectPath().string()).ok());
    auto* project = manager.GetCurrentProject();
    ASSERT_NE(project, nullptr);
    ASSERT_EQ(project->filepath, ProjectPath().string());
    ASSERT_EQ(project->rom_filename, InputPath().string());
    ASSERT_EQ(project->GetAbsolutePath(project->hack_manifest_file),
              ManifestPath().string());
    ASSERT_TRUE(project->hack_manifest.loaded());
    ASSERT_TRUE(project->hack_manifest.lyra_private_candidate_persistence());
    ASSERT_FALSE(project->hack_manifest.lyra_private_reservation());
    ASSERT_EQ(*project->hack_manifest.lyra_private_candidate_persistence(),
              core::PrivateLyraExactCandidatePersistence());
    ASSERT_EQ(project->rom_metadata.write_policy,
              project::RomWritePolicy::kBlock);
    ASSERT_EQ(project->rom_metadata.expected_hash,
              "849eddf7d1e335e6e1529bdf450053748224b83c");
    ASSERT_EQ(project->GetAbsolutePath(
                  project->hack_manifest.build_pipeline().dev_rom),
              InputPath().string());
    ASSERT_TRUE(project->workspace_settings.backup_on_save);
    ASSERT_EQ(project->GetAbsolutePath(project->rom_backup_folder),
              (Phase() / "project/Backups").string());
    auto* rom = manager.GetCurrentRom();
    ASSERT_NE(rom, nullptr);
    ASSERT_EQ(rom->filename(), InputPath().string());
    ASSERT_TRUE(rom->is_lyra_private_candidate());
    ASSERT_EQ(util::ComputeSha1Hex(rom->data(), rom->size()),
              project->rom_metadata.expected_hash);
    ASSERT_TRUE(
        manager.EnsureEditorAssetsLoaded(editor::EditorType::kOverworld).ok());
    auto* overworld = static_cast<editor::OverworldEditor*>(
        manager.GetCurrentEditorSet()->GetExistingEditor(
            editor::EditorType::kOverworld));
    ASSERT_NE(overworld, nullptr);
    ASSERT_TRUE(overworld->overworld().is_loaded());
  }
  void AssertPreservedProjectAndInput() {
    EXPECT_EQ(ReadFile(ProjectPath()), project_before_);
    EXPECT_EQ(ReadFile(ManifestPath()), manifest_before_);
    EXPECT_EQ(ReadFile(CollisionPath()), collision_before_);
    EXPECT_EQ(
        core::ComputeSourceArtifactSha256(ReadFile(InputPath())),
        core::PrivateLyraExactCandidatePersistence().candidate_rom_sha256);
  }
  ImGuiContext* imgui_ = nullptr;
  std::string case_;
  std::string project_before_;
  std::string manifest_before_;
  std::string collision_before_;
};

TEST_F(PrivateLyraCandidateDiskRoundtripTest, SaveAsAndColdReopen) {
  const auto output = Phase() / "roundtrip/save-as-full32.sfc";
  ASSERT_FALSE(std::filesystem::exists(output));
  ASSERT_EQ(std::filesystem::canonical(output.parent_path()),
            output.parent_path());
  gfx::NullRenderer renderer;
  auto manager = std::make_unique<editor::EditorManager>();
  ASSERT_NO_FATAL_FAILURE(OpenProject(*manager, renderer));
  const auto status = manager->SaveRomAs(output.string());
  ASSERT_TRUE(status.ok()) << status;
  ASSERT_NO_FATAL_FAILURE(AssertRegularUnaliased(output));
  ASSERT_EQ(ReadFile(output), ReadFile(InputPath()));
  ASSERT_NO_FATAL_FAILURE(AssertPreservedProjectAndInput());
  // Save As does not retarget the manifest's development-ROM binding.
  manager.reset();
  auto fresh = std::make_unique<editor::EditorManager>();
  fresh->user_settings().SetSettingsFilePathForTesting(
      (Phase() / "isolated-settings/save_as.cold.settings.json").string());
  fresh->Initialize(&renderer, "");
  ASSERT_EQ(ImGui::GetIO().IniFilename, nullptr);
  fresh->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_TRUE(fresh->OpenRomOrProject(output.string()).ok());
  ASSERT_TRUE(fresh->GetCurrentRom()->is_lyra_private_candidate());
  ASSERT_EQ(core::ComputeSourceArtifactSha256(ReadFile(output)),
            core::PrivateLyraExactCandidatePersistence().candidate_rom_sha256);
  project::YazeProject project;
  ASSERT_TRUE(project.Open(ProjectPath().string()).ok());
  editor::RomLifecycleManager lifecycle;
  lifecycle.SetProjectContext(&project);
  EXPECT_TRUE(absl::IsFailedPrecondition(
      lifecycle.CheckRomRebindPolicy(fresh->GetCurrentRom())));
  EXPECT_TRUE(fresh->GetCurrentRom()->ValidateLyraPrivateReservation().ok());
  ASSERT_NO_FATAL_FAILURE(AssertPreservedProjectAndInput());
}

TEST_F(PrivateLyraCandidateDiskRoundtripTest, OrdinarySaveAndColdReopen) {
  const auto backups = Phase() / "project/Backups";
  ASSERT_EQ(std::filesystem::canonical(backups), backups);
  ASSERT_TRUE(std::filesystem::is_empty(backups));
  std::vector<std::filesystem::path> before;
  for (const auto& entry : std::filesystem::directory_iterator(backups))
    before.push_back(entry.path());
  gfx::NullRenderer renderer;
  auto manager = std::make_unique<editor::EditorManager>();
  ASSERT_NO_FATAL_FAILURE(OpenProject(*manager, renderer));
  const auto status = manager->SaveRom();
  ASSERT_TRUE(status.ok()) << status;
  size_t created = 0;
  for (const auto& entry : std::filesystem::directory_iterator(backups)) {
    if (std::find(before.begin(), before.end(), entry.path()) != before.end())
      continue;
    ASSERT_NO_FATAL_FAILURE(AssertRegularUnaliased(entry.path()));
    ASSERT_EQ(ReadFile(entry.path()), ReadFile(InputPath()));
    std::fprintf(
        stderr, "PrivateLyraRequiredBackup %s sha256=%s\n",
        entry.path().c_str(),
        core::ComputeSourceArtifactSha256(ReadFile(entry.path())).c_str());
    ++created;
  }
  ASSERT_EQ(created, 1u);
  ASSERT_NO_FATAL_FAILURE(AssertPreservedProjectAndInput());
  manager.reset();
  auto fresh = std::make_unique<editor::EditorManager>();
  ASSERT_NO_FATAL_FAILURE(OpenProject(*fresh, renderer));
  EXPECT_TRUE(fresh->GetCurrentRom()->ValidateLyraPrivateReservation().ok());
  ASSERT_NO_FATAL_FAILURE(AssertPreservedProjectAndInput());
}

}  // namespace
}  // namespace yaze
