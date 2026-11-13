#include "app/editor/overworld/project_graphics_source.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "core/overworld_graphics_preview.h"
#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

namespace yaze::editor {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

class ProjectGraphicsSourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = fs::temp_directory_path() /
            ("yaze graphics source ' ; $(literal)-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
    source_root_ = root_ / "Oracle source";
    canonical_ = source_root_ / "Overworld/graphics_catalog.json";
    portable_ = root_ / "project pack/catalog.json";
    fs::create_directories(canonical_.parent_path() / "gfx");
    fs::create_directories(portable_.parent_path() / "assets");
    Json catalog = {{"schema_version", 2},
                    {"resources", Json::object()},
                    {"profiles", Json::object()},
                    {"bindings", Json::array()}};
    const std::vector<std::pair<std::string, std::string>> assets = {
        {"alpha", Tile(1) + Tile(2)},
        {"beta", Tile(8)},
        {"unrelated", Tile(4)}};
    for (const auto& [id, bytes] : assets) {
      Write(canonical_.parent_path() / "gfx" / (id + ".4bpp"), bytes);
      Write(portable_.parent_path() / "assets" / (id + ".4bpp"), bytes);
      catalog["resources"][id] = {
          {"format", "snes4bpp"},
          {"byte_count", bytes.size()},
          {"sha256", core::ComputeSourceArtifactSha256(bytes)},
          {"path", "gfx/" + id + ".4bpp"}};
    }
    catalog["profiles"]["coast"] = {
        {"bg_character_base_word", 0x2000},
        {"transfers", Json::array({Transfer("alpha", 32, 32, 0x2C00),
                                   Transfer("beta", 0, 32, 0x2C10)})}};
    catalog["profiles"]["island"] = {
        {"bg_character_base_word", 0x2000},
        {"transfers", Json::array({Transfer("alpha", 0, 64, 0x3000)})}};
    catalog["profiles"]["elsewhere"] = {
        {"bg_character_base_word", 0x2000},
        {"transfers", Json::array({Transfer("unrelated", 0, 32, 0x2C00)})}};
    catalog["bindings"] =
        Json::array({{{"profile_id", "coast"}, {"parent_maps", {0x30, 0x32}}},
                     {{"profile_id", "island"}, {"parent_maps", {0x38}}},
                     {{"profile_id", "elsewhere"}, {"parent_maps", {0x3A}}}});
    Write(canonical_, catalog.dump());
    for (auto& [id, resource] : catalog["resources"].items())
      resource["path"] = "assets/" + id + ".4bpp";
    Write(portable_, catalog.dump());
  }

  void TearDown() override { fs::remove_all(root_); }

  static void Write(const fs::path& path, const std::string& bytes) {
    std::ofstream(path, std::ios::binary) << bytes;
  }
  static std::string Read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
  }
  static std::string Tile(uint8_t color) {
    std::string result(32, '\0');
    for (int plane = 0; plane < 4; ++plane) {
      if (color & (1 << plane))
        result[(plane / 2) * 16 + plane % 2] = static_cast<char>(0x80);
    }
    return result;
  }
  static Json Transfer(const std::string& id, int offset, int count, int vram) {
    return {{"resource_id", id},
            {"source_offset", offset},
            {"byte_count", count},
            {"vram_word_address", vram}};
  }
  absl::Status Load() {
    return document_.Load(source_root_.string(), portable_.string(), 0x30);
  }

  // Small deterministic backend fixture. It verifies the argv and held plan
  // protocol; production compiler/transaction tests belong to the Python tool.
  void WriteBackend() {
    const auto script =
        source_root_ / "Scripts/Graphics/edit_graphics_source.py";
    fs::create_directories(script.parent_path());
    Write(script, R"PY(import hashlib, json, pathlib, sys
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
args = sys.argv[1:]
operation = args.pop(0)
options = dict(zip(args[::2], args[1::2]))
root = pathlib.Path(options['--source-root'])
portable = pathlib.Path(options['--portable-catalog'])
canonical = root / 'Overworld/graphics_catalog.json'
plan_path = pathlib.Path(options['--plan'])
if operation == 'plan':
    resource_id = options['--resource-id']
    replacement = pathlib.Path(options['--replacement'])
    catalog = json.loads(canonical.read_text())
    pack = json.loads(portable.read_text())
    resource = catalog['resources'][resource_id]
    inputs = []
    for path, doc in [(canonical, catalog), (portable, pack)]:
        inputs.append({'path': str(path), 'sha256': sha(path)})
        for asset in doc['resources'].values():
            asset_path = path.parent / asset['path']
            inputs.append({'path': str(asset_path), 'sha256': sha(asset_path)})
    after = resource_id + '-next'
    result = {'schema_version': 1, 'operation': 'plan', 'result': 'ok',
              'source_root': str(root), 'portable_catalog': str(portable),
              'before_resource_id': resource_id, 'after_resource_id': after,
              'resource_sha256': resource['sha256'],
              'resource_byte_count': resource['byte_count'],
              'replacement_path': str(replacement),
              'replacement_sha256': sha(replacement), 'inputs': inputs,
              'source_resource_path': str(canonical.parent / resource['path']),
              'portable_resource_path': str(portable.parent / pack['resources'][resource_id]['path']),
              'after_source_resource_path': str(canonical.parent / (after + '.4bpp')),
              'after_portable_resource_path': str(portable.parent / (after + '.4bpp')),
              'affected_profiles': ['coast', 'island'],
              'affected_parent_maps': [48, 50, 56], 'changed_files': []}
    plan_path.write_text(json.dumps(result))
else:
    plan_bytes = plan_path.read_bytes()
    if hashlib.sha256(plan_bytes).hexdigest() != options.get('--expected-plan-sha256'):
        print(json.dumps({'result': 'error', 'error': 'review plan hash mismatch'}))
        sys.exit(1)
    result = json.loads(plan_bytes)
    for entry in result['inputs']:
        if sha(pathlib.Path(entry['path'])) != entry['sha256']:
            print(json.dumps({'result': 'error', 'error': 'stale backend input'}))
            sys.exit(1)
    replacement = pathlib.Path(result['replacement_path']).read_bytes()
    before, after = result['before_resource_id'], result['after_resource_id']
    for path in [canonical, portable]:
        catalog = json.loads(path.read_text())
        resource = catalog['resources'].pop(before)
        resource['path'] = after + '.4bpp'
        resource['sha256'] = hashlib.sha256(replacement).hexdigest()
        (path.parent / resource['path']).write_bytes(replacement)
        catalog['resources'][after] = resource
        for profile in catalog['profiles'].values():
            for transfer in profile['transfers']:
                if transfer['resource_id'] == before:
                    transfer['resource_id'] = after
        path.write_text(json.dumps(catalog))
    result['operation'], result['result'] = 'apply', 'published'
print(json.dumps(result))
)PY");
  }

  fs::path root_;
  fs::path source_root_;
  fs::path canonical_;
  fs::path portable_;
  ProjectGraphicsSource document_;
};

TEST_F(ProjectGraphicsSourceTest,
       LoadsWholeResourceAndReportsEveryTransferConsumer) {
  ASSERT_TRUE(Load().ok());
  ASSERT_EQ(document_.resources().size(), 2);
  EXPECT_EQ(document_.profile_id(), "coast");
  EXPECT_EQ(document_.parent_map(), 0x30);
  const auto* resource = document_.selected_resource();
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(resource->id, "alpha");
  ASSERT_EQ(resource->tiles.size(), 2);
  EXPECT_EQ(*document_.GetPixel(0, 0, 0), 1);
  EXPECT_EQ(*document_.GetPixel(1, 0, 0), 2);
  EXPECT_EQ(resource->profile_ids,
            std::vector<std::string>({"coast", "island"}));
  EXPECT_EQ(resource->parent_maps, std::vector<int>({0x30, 0x32, 0x38}));
  ASSERT_EQ(resource->slices.size(), 2);
  EXPECT_EQ(resource->slices[0].first_resource_tile(), 1);
  EXPECT_EQ(resource->slices[0].first_character(), 0xC0);
  EXPECT_EQ(resource->slices[1].first_resource_tile(), 0);
  EXPECT_EQ(resource->slices[1].first_character(), 0x100);
  EXPECT_FALSE(document_.SelectResource("unrelated").ok());
  EXPECT_FALSE(document_.any_dirty());
}

TEST_F(ProjectGraphicsSourceTest,
       AcceptsPathDifferencesButRequiresPublisherCompatibleArrayOrder) {
  EXPECT_TRUE(Load().ok());
  auto pack = Json::parse(Read(portable_));
  std::reverse(pack["bindings"].begin(), pack["bindings"].end());
  std::reverse(pack["profiles"]["coast"]["transfers"].begin(),
               pack["profiles"]["coast"]["transfers"].end());
  std::reverse(pack["bindings"][2]["parent_maps"].begin(),
               pack["bindings"][2]["parent_maps"].end());
  Write(portable_, pack.dump());
  EXPECT_FALSE(Load().ok());
  EXPECT_FALSE(document_.loaded());
}

TEST_F(ProjectGraphicsSourceTest,
       RejectsValidButDifferentSourceAndPackBindings) {
  auto pack = Json::parse(Read(portable_));
  pack["bindings"][0]["parent_maps"] = {0x31, 0x32};
  Write(portable_, pack.dump());
  auto status = Load();
  EXPECT_FALSE(status.ok());
  EXPECT_NE(status.message().find("catalogs differ"), std::string::npos);
  EXPECT_FALSE(document_.loaded());
}

TEST_F(ProjectGraphicsSourceTest,
       InvalidPackHashOrDuplicateIdentityFailsClosed) {
  ASSERT_TRUE(Load().ok());
  const auto path = portable_.parent_path() / "assets/alpha.4bpp";
  Write(path, std::string(64, '\0'));
  EXPECT_FALSE(Load().ok());
  EXPECT_FALSE(document_.loaded());
  EXPECT_EQ(document_.selected_resource(), nullptr);

  Write(path, Tile(1) + Tile(2));
  const auto valid = Read(portable_);
  Write(portable_, "{\"schema_version\":2," + valid.substr(1));
  EXPECT_FALSE(Load().ok());
  EXPECT_FALSE(document_.loaded());
}

TEST_F(ProjectGraphicsSourceTest, PixelCodecChangesOnlyTheFourPlaneBits) {
  ASSERT_TRUE(Load().ok());
  const auto original = document_.selected_resource()->original_bytes;
  ASSERT_TRUE(document_.SetPixel(1, 7, 6, 15).ok());
  EXPECT_EQ(*document_.GetPixel(1, 7, 6), 15);
  auto expected = original;
  for (int plane = 0; plane < 4; ++plane)
    expected[32 + (plane / 2) * 16 + 6 * 2 + plane % 2] |= 1;
  EXPECT_EQ(document_.selected_resource()->pending_bytes, expected);
  EXPECT_EQ(document_.selected_resource()->original_bytes, original);
  EXPECT_TRUE(document_.dirty());
  EXPECT_FALSE(document_.SetPixel(0, 0, 0, 16).ok());
  EXPECT_FALSE(document_.SetPixel(2, 0, 0, 1).ok());
  EXPECT_FALSE(document_.SetPixel(0, -1, 0, 1).ok());
  EXPECT_FALSE(document_.GetPixel(0, 0, 8).ok());
}

TEST_F(ProjectGraphicsSourceTest,
       UndoRedoAreIndependentForEachExternalResource) {
  ASSERT_TRUE(Load().ok());
  ASSERT_TRUE(document_.SetPixel(0, 0, 0, 3).ok());
  ASSERT_TRUE(document_.SelectResource("beta").ok());
  EXPECT_FALSE(document_.dirty());
  EXPECT_TRUE(document_.any_dirty());
  EXPECT_FALSE(document_.can_undo());
  ASSERT_TRUE(document_.SetPixel(0, 0, 0, 15).ok());
  ASSERT_TRUE(document_.Undo().ok());
  EXPECT_EQ(*document_.GetPixel(0, 0, 0), 8);
  EXPECT_FALSE(document_.dirty());
  ASSERT_TRUE(document_.SelectResource("alpha").ok());
  ASSERT_TRUE(document_.Undo().ok());
  EXPECT_EQ(*document_.GetPixel(0, 0, 0), 1);
  EXPECT_FALSE(document_.any_dirty());
  ASSERT_TRUE(document_.Redo().ok());
  EXPECT_EQ(*document_.GetPixel(0, 0, 0), 3);
  ASSERT_TRUE(document_.SelectResource("beta").ok());
  ASSERT_TRUE(document_.Redo().ok());
  EXPECT_EQ(*document_.GetPixel(0, 0, 0), 15);
  ASSERT_TRUE(document_.Undo().ok());
  ASSERT_TRUE(document_.SetPixel(0, 0, 0, 4).ok());
  EXPECT_FALSE(document_.can_redo());
}

TEST_F(ProjectGraphicsSourceTest, UnchangedPixelDoesNotCreateHistory) {
  ASSERT_TRUE(Load().ok());
  ASSERT_TRUE(document_.SetPixel(0, 0, 0, 1).ok());
  EXPECT_FALSE(document_.can_undo());
  EXPECT_FALSE(document_.any_dirty());
  EXPECT_FALSE(document_.Undo().ok());
  EXPECT_FALSE(document_.Redo().ok());
  EXPECT_FALSE(document_.Publish().ok());
}

TEST_F(ProjectGraphicsSourceTest, BulkReplacementRequiresLoadedBinding) {
  const std::vector<uint8_t> bytes(64, 0);
  EXPECT_FALSE(document_.ReplaceResourceBytes(bytes).ok());
  EXPECT_FALSE(document_.ReplaceResourceBytes("unbound.4bpp", bytes).ok());
  EXPECT_FALSE(document_.PendingGraphicsPreview().ok());
  ASSERT_TRUE(Load().ok());
  const auto before = document_.selected_resource()->pending_bytes;
  EXPECT_FALSE(document_.ReplaceResourceBytes("unbound.4bpp", bytes).ok());
  EXPECT_EQ(document_.selected_resource()->pending_bytes, before);
  EXPECT_FALSE(document_.any_dirty());
}

TEST_F(ProjectGraphicsSourceTest,
       InvalidBulkSizeLeavesBytesPixelsSelectionAndHistoryUnchanged) {
  ASSERT_TRUE(Load().ok());
  ASSERT_TRUE(document_.SetPixel(1, 2, 3, 11).ok());
  const auto before = *document_.selected_resource();
  for (size_t size :
       {size_t{0}, size_t{32}, size_t{63}, size_t{65}, size_t{96}}) {
    EXPECT_FALSE(
        document_.ReplaceResourceBytes(std::vector<uint8_t>(size, 0)).ok());
    EXPECT_EQ(document_.selected_resource_id(), before.id);
    EXPECT_EQ(document_.selected_resource()->pending_bytes,
              before.pending_bytes);
    EXPECT_EQ(document_.selected_resource()->original_bytes,
              before.original_bytes);
    EXPECT_EQ(document_.selected_resource()->tiles, before.tiles);
    EXPECT_TRUE(document_.can_undo());
    EXPECT_FALSE(document_.can_redo());
  }
  ASSERT_TRUE(document_.Undo().ok());
  EXPECT_EQ(*document_.GetPixel(1, 2, 3), 0);
  EXPECT_FALSE(document_.any_dirty());
}

TEST_F(ProjectGraphicsSourceTest,
       StaleSourceBlocksBulkReplacementWithoutChangingDraftOrHistory) {
  ASSERT_TRUE(Load().ok());
  ASSERT_TRUE(document_.SetPixel(0, 0, 0, 3).ok());
  const auto before = *document_.selected_resource();
  Write(canonical_.parent_path() / "gfx/alpha.4bpp", Tile(4) + Tile(2));
  const auto status =
      document_.ReplaceResourceBytes(std::vector<uint8_t>(64, 0));
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(document_.selected_resource()->pending_bytes, before.pending_bytes);
  EXPECT_EQ(document_.selected_resource()->original_bytes,
            before.original_bytes);
  EXPECT_EQ(document_.selected_resource()->tiles, before.tiles);
  EXPECT_TRUE(document_.can_undo());
  EXPECT_FALSE(document_.PendingGraphicsPreview().ok());
  ASSERT_TRUE(document_.Undo().ok());
  EXPECT_EQ(*document_.GetPixel(0, 0, 0), 1);
}

TEST_F(ProjectGraphicsSourceTest,
       BulkReplacementDecodesExactlyAndUsesCallerOwnedSnapshots) {
  ASSERT_TRUE(Load().ok());
  const auto original = document_.selected_resource()->original_bytes;
  ASSERT_TRUE(document_.SetPixel(0, 1, 2, 7).ok());
  std::vector<uint8_t> replacement(64, 0);
  for (int y = 0; y < 8; ++y) {
    replacement[y * 2] = 0xAA;
    replacement[y * 2 + 1] = 0xCC;
    replacement[16 + y * 2] = 0xF0;
    replacement[16 + y * 2 + 1] = 0xFF;
    replacement[32 + y * 2] = 0xFF;
    replacement[32 + y * 2 + 1] = 0xFF;
    replacement[48 + y * 2] = 0xFF;
  }
  ASSERT_TRUE(document_.ReplaceResourceBytes(replacement).ok());
  EXPECT_EQ(document_.selected_resource()->pending_bytes, replacement);
  EXPECT_EQ(document_.selected_resource()->original_bytes, original);
  EXPECT_TRUE(document_.dirty());
  EXPECT_FALSE(document_.can_undo());
  EXPECT_FALSE(document_.can_redo());
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      EXPECT_EQ(*document_.GetPixel(0, x, y), 15 - x);
      EXPECT_EQ(*document_.GetPixel(1, x, y), 7);
    }
  }
  ASSERT_TRUE(document_.SetPixel(0, 0, 0, 2).ok());
  ASSERT_TRUE(document_.Undo().ok());
  EXPECT_EQ(*document_.GetPixel(0, 0, 0), 15);
  ASSERT_TRUE(document_.Redo().ok());
  EXPECT_EQ(*document_.GetPixel(0, 0, 0), 2);
  ASSERT_TRUE(document_.ReplaceResourceBytes(original).ok());
  EXPECT_FALSE(document_.any_dirty());
  EXPECT_FALSE(document_.can_undo());
  EXPECT_FALSE(document_.can_redo());
}

TEST_F(ProjectGraphicsSourceTest,
       BulkPathBindingKeepsSelectionAndOtherResourcePixelHistory) {
  ASSERT_TRUE(Load().ok());
  const auto alpha_path = document_.selected_resource()->source_path;
  const std::vector<uint8_t> alpha(64, 0);
  ASSERT_TRUE(document_.SelectResource("beta").ok());
  ASSERT_TRUE(document_.SetPixel(0, 0, 0, 5).ok());
  ASSERT_TRUE(document_.ReplaceResourceBytes(alpha_path, alpha).ok());
  EXPECT_EQ(document_.selected_resource_id(), "beta");
  EXPECT_TRUE(document_.can_undo());
  ASSERT_TRUE(document_.Undo().ok());
  EXPECT_EQ(*document_.GetPixel(0, 0, 0), 8);
  EXPECT_FALSE(document_.dirty());
  EXPECT_TRUE(document_.any_dirty());
  ASSERT_TRUE(document_.SelectResource("alpha").ok());
  EXPECT_EQ(document_.selected_resource()->pending_bytes, alpha);
  EXPECT_FALSE(document_.can_undo());
}

TEST_F(ProjectGraphicsSourceTest,
       PendingPreviewResolvesResourceSlicesAndConsumersWithoutWrites) {
  ASSERT_TRUE(Load().ok());
  const auto canonical_before = Read(canonical_);
  const auto portable_before = Read(portable_);
  const auto source_path = document_.selected_resource()->source_path;
  const auto source_before = Read(source_path);
  const auto raw = Tile(4) + Tile(15);
  ASSERT_TRUE(
      document_
          .ReplaceResourceBytes(std::vector<uint8_t>(raw.begin(), raw.end()))
          .ok());
  auto preview = document_.PendingGraphicsPreview();
  ASSERT_TRUE(preview.ok()) << preview.status();
  ASSERT_EQ(preview->size(), 3);
  for (int parent : {0x30, 0x32}) {
    ASSERT_EQ(preview->at(parent).size(), 2);
    EXPECT_EQ(preview->at(parent).at(0xC0)[0], 15);
    EXPECT_EQ(preview->at(parent).at(0xC1)[0], 8);
  }
  ASSERT_EQ(preview->at(0x38).size(), 2);
  EXPECT_EQ(preview->at(0x38).at(0x100)[0], 4);
  EXPECT_EQ(preview->at(0x38).at(0x101)[0], 15);
  EXPECT_EQ(preview->count(0x3A), 0);
  EXPECT_FALSE(document_.has_preview());
  EXPECT_EQ(Read(canonical_), canonical_before);
  EXPECT_EQ(Read(portable_), portable_before);
  EXPECT_EQ(Read(source_path), source_before);
}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
TEST_F(ProjectGraphicsSourceTest,
       BulkReplacementInvalidatesHeldPlanOnlyAfterSuccessfulChange) {
  WriteBackend();
  ASSERT_TRUE(Load().ok());
  ASSERT_TRUE(document_.SetPixel(0, 0, 0, 3).ok());
  ASSERT_TRUE(document_.Preview().ok());
  const auto bytes = document_.selected_resource()->pending_bytes;
  EXPECT_FALSE(document_.ReplaceResourceBytes(std::vector<uint8_t>(63)).ok());
  EXPECT_TRUE(document_.has_preview());
  ASSERT_TRUE(document_.ReplaceResourceBytes(bytes).ok());
  EXPECT_TRUE(document_.has_preview());
  EXPECT_TRUE(document_.can_undo());
  auto changed = bytes;
  changed[0] ^= 1;
  ASSERT_TRUE(document_.ReplaceResourceBytes(changed).ok());
  EXPECT_FALSE(document_.has_preview());
  EXPECT_FALSE(document_.can_undo());
  EXPECT_FALSE(document_.Publish().ok());
}

TEST_F(ProjectGraphicsSourceTest,
       PreviewDoesNotWriteSourceAndPixelEditInvalidatesIt) {
  WriteBackend();
  ASSERT_TRUE(Load().ok());
  ASSERT_TRUE(document_.SetPixel(1, 3, 5, 14).ok());
  const auto source_before = Read(canonical_);
  const auto pack_before = Read(portable_);
  auto preview = document_.Preview();
  ASSERT_TRUE(preview.ok()) << preview.status();
  EXPECT_TRUE(document_.has_preview());
  EXPECT_EQ(preview->at("before_resource_id"), "alpha");
  EXPECT_EQ(preview->at("after_resource_id"), "alpha-next");
  EXPECT_EQ(Read(canonical_), source_before);
  EXPECT_EQ(Read(portable_), pack_before);
  ASSERT_TRUE(document_.SetPixel(1, 3, 5, 13).ok());
  EXPECT_FALSE(document_.has_preview());
  EXPECT_FALSE(document_.Publish().ok());
  EXPECT_EQ(Read(canonical_), source_before);
}

TEST_F(ProjectGraphicsSourceTest, StaleLoadedSourceBlocksPreviewAndKeepsDraft) {
  WriteBackend();
  ASSERT_TRUE(Load().ok());
  ASSERT_TRUE(document_.SetPixel(0, 0, 0, 3).ok());
  Write(canonical_, Read(canonical_) + "\n");
  const auto status = document_.Preview();
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(document_.dirty());
  EXPECT_EQ(*document_.GetPixel(0, 0, 0), 3);
  EXPECT_FALSE(document_.has_preview());
}

TEST_F(ProjectGraphicsSourceTest, StalePortableResourceBlocksHeldPlan) {
  WriteBackend();
  ASSERT_TRUE(Load().ok());
  ASSERT_TRUE(document_.SetPixel(0, 0, 0, 3).ok());
  auto preview = document_.Preview();
  ASSERT_TRUE(preview.ok()) << preview.status();
  const auto source_before = Read(canonical_);
  Write(portable_.parent_path() / "assets/beta.4bpp", Tile(9));
  const auto published = document_.Publish();
  EXPECT_FALSE(published.ok());
  EXPECT_EQ(Read(canonical_), source_before);
  EXPECT_TRUE(document_.dirty());
}

TEST_F(ProjectGraphicsSourceTest, ChangedReplacementOrPlanBlocksPublication) {
  WriteBackend();
  ASSERT_TRUE(Load().ok());
  ASSERT_TRUE(document_.SetPixel(0, 0, 0, 3).ok());
  auto preview = document_.Preview();
  ASSERT_TRUE(preview.ok()) << preview.status();
  Write(preview->at("replacement_path").get<std::string>(),
        std::string(64, '\0'));
  EXPECT_FALSE(document_.Publish().ok());
  EXPECT_TRUE(document_.dirty());
  preview = document_.Preview();
  ASSERT_TRUE(preview.ok()) << preview.status();
  const auto plan_path =
      fs::path(preview->at("replacement_path").get<std::string>())
          .parent_path() /
      "review-plan.json";
  Write(plan_path, Read(plan_path) + "\n");
  EXPECT_FALSE(document_.Publish().ok());
  EXPECT_TRUE(document_.dirty());
}

TEST_F(ProjectGraphicsSourceTest,
       PublishReloadsNewIdentityAndRetainsOtherResourceDraft) {
  WriteBackend();
  ASSERT_TRUE(Load().ok());
  ASSERT_TRUE(document_.SelectResource("beta").ok());
  ASSERT_TRUE(document_.SetPixel(0, 1, 1, 7).ok());
  ASSERT_TRUE(document_.SelectResource("alpha").ok());
  ASSERT_TRUE(document_.SetPixel(1, 3, 5, 14).ok());
  const auto expected = document_.selected_resource()->pending_bytes;
  const auto old_source_path = document_.selected_resource()->source_path;
  auto preview = document_.Preview();
  ASSERT_TRUE(preview.ok()) << preview.status();
  auto published = document_.Publish();
  ASSERT_TRUE(published.ok()) << published.status();
  EXPECT_EQ(published->at("result"), "published");
  EXPECT_EQ(document_.selected_resource_id(), "alpha-next");
  EXPECT_EQ(document_.selected_resource()->original_bytes, expected);
  EXPECT_FALSE(document_.dirty());
  EXPECT_TRUE(document_.can_undo());
  ASSERT_TRUE(document_.Undo().ok());
  EXPECT_EQ(*document_.GetPixel(1, 3, 5), 0);
  EXPECT_TRUE(document_.dirty());
  ASSERT_TRUE(document_.Redo().ok());
  EXPECT_EQ(*document_.GetPixel(1, 3, 5), 14);
  EXPECT_FALSE(document_.dirty());
  EXPECT_FALSE(document_.has_preview());
  EXPECT_NE(document_.selected_resource()->source_path, old_source_path);
  EXPECT_TRUE(fs::exists(old_source_path));
  EXPECT_TRUE(document_.any_dirty());
  ASSERT_TRUE(document_.SelectResource("beta").ok());
  EXPECT_TRUE(document_.dirty());
  EXPECT_EQ(*document_.GetPixel(0, 1, 1), 7);
  ASSERT_TRUE(document_.Undo().ok());
  EXPECT_FALSE(document_.any_dirty());
  ProjectGraphicsSource independent;
  ASSERT_TRUE(
      independent.Load(source_root_.string(), portable_.string(), 0x30).ok());
  ASSERT_TRUE(independent.SelectResource("alpha-next").ok());
  EXPECT_EQ(independent.selected_resource()->original_bytes, expected);
}

// This test invokes the real Oracle compiler/publisher on a complete disposable
// source and portable pack. The synthetic suite above never requires Oracle.
class ProjectGraphicsSourceOracleTest : public ProjectGraphicsSourceTest {
 protected:
  static std::map<std::string, std::string> HashTree(const fs::path& root) {
    std::map<std::string, std::string> result;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
      if (entry.is_regular_file())
        result[entry.path().string()] =
            core::ComputeSourceArtifactSha256(Read(entry.path()));
    }
    return result;
  }
};

TEST_F(ProjectGraphicsSourceOracleTest,
       PublishReopensCompilerValidatedFortResource) {
  const auto* source_env = std::getenv("YAZE_TEST_GRAPHICS_SOURCE_ROOT");
  const auto* catalog_env = std::getenv("YAZE_TEST_PORTABLE_CATALOG");
  if (!source_env || !catalog_env)
    GTEST_SKIP()
        << "Set YAZE_TEST_GRAPHICS_SOURCE_ROOT and "
           "YAZE_TEST_PORTABLE_CATALOG for disposable Oracle publication";
  const auto original_root = fs::canonical(source_env);
  const auto original_catalog = fs::canonical(catalog_env);
  const auto source_hashes = HashTree(original_root / "Overworld");
  const auto pack_hashes = HashTree(original_catalog.parent_path());
  std::map<std::string, std::string> script_hashes;
  for (const auto* name :
       {"edit_graphics_source.py", "compile_graphics_pack.py",
        "compile_runtime_graphics.py"}) {
    const auto path = original_root / "Scripts/Graphics" / name;
    ASSERT_TRUE(fs::is_regular_file(path)) << path;
    script_hashes[path.string()] =
        core::ComputeSourceArtifactSha256(Read(path));
  }
  const auto copied_root = fs::canonical(root_) / "actual Oracle";
  const auto copied_pack = fs::canonical(root_) / "actual pack";
  fs::create_directories(copied_root);
  fs::copy(original_root / "Overworld", copied_root / "Overworld",
           fs::copy_options::recursive);
  fs::create_directories(copied_root / "Scripts/Graphics");
  for (const auto& [path, hash] : script_hashes)
    fs::copy_file(path,
                  copied_root / "Scripts/Graphics" / fs::path(path).filename());
  fs::copy(original_catalog.parent_path(), copied_pack,
           fs::copy_options::recursive);
  const auto copied_catalog = copied_pack / original_catalog.filename();

  ProjectGraphicsSource actual;
  ASSERT_TRUE(
      actual.Load(copied_root.string(), copied_catalog.string(), 0x83).ok());
  ASSERT_EQ(actual.profile_id(), "stepped-waterfront-fort");
  ASSERT_NE(actual.selected_resource(), nullptr);
  const auto old_id = actual.selected_resource_id();
  const auto original_bytes = actual.selected_resource()->original_bytes;
  const auto& slices = actual.selected_resource()->slices;
  const auto slice =
      std::find_if(slices.begin(), slices.end(), [](const auto& value) {
        return value.profile_id == "stepped-waterfront-fort";
      });
  ASSERT_NE(slice, slices.end());
  const auto resource_tile = slice->first_resource_tile();
  const auto character = slice->first_character();
  auto before = actual.GetPixel(resource_tile, 0, 0);
  ASSERT_TRUE(before.ok());
  // This profile forbids opaque index 8. Both choices are opaque valid colors.
  const uint8_t color = *before == 1 ? 2 : 1;
  ASSERT_TRUE(actual.SetPixel(resource_tile, 0, 0, color).ok());
  const auto pending = actual.selected_resource()->pending_bytes;
  auto preview = actual.Preview();
  ASSERT_TRUE(preview.ok()) << preview.status();
  EXPECT_EQ(preview->at("before_resource_id"), old_id);
  EXPECT_EQ(preview->at("after_resource_id"),
            "snes4bpp:" + core::ComputeSourceArtifactSha256(
                              {reinterpret_cast<const char*>(pending.data()),
                               pending.size()}));
  auto published = actual.Publish();
  ASSERT_TRUE(published.ok()) << published.status();
  EXPECT_NE(actual.selected_resource_id(), old_id);
  EXPECT_FALSE(actual.any_dirty());
  EXPECT_EQ(actual.selected_resource()->original_bytes, pending);
  EXPECT_NE(original_bytes, pending);
  ASSERT_TRUE(actual.Undo().ok());
  EXPECT_EQ(*actual.GetPixel(resource_tile, 0, 0), *before);
  EXPECT_TRUE(actual.dirty());
  ASSERT_TRUE(actual.Redo().ok());
  EXPECT_EQ(*actual.GetPixel(resource_tile, 0, 0), color);
  EXPECT_FALSE(actual.dirty());
  ProjectGraphicsSource reopened;
  ASSERT_TRUE(
      reopened.Load(copied_root.string(), copied_catalog.string(), 0x83).ok());
  EXPECT_EQ(*reopened.GetPixel(resource_tile, 0, 0), color);
  auto rendered = core::LoadOverworldGraphicsPreview(copied_catalog.string());
  ASSERT_TRUE(rendered.ok()) << rendered.status();
  ASSERT_TRUE(rendered->contains(0x83));
  ASSERT_TRUE(rendered->at(0x83).contains(character));
  EXPECT_EQ(rendered->at(0x83).at(character)[0], color);
  EXPECT_EQ(HashTree(original_root / "Overworld"), source_hashes);
  EXPECT_EQ(HashTree(original_catalog.parent_path()), pack_hashes);
  for (const auto& [path, hash] : script_hashes)
    EXPECT_EQ(core::ComputeSourceArtifactSha256(Read(path)), hash);
}
#endif

}  // namespace
}  // namespace yaze::editor
