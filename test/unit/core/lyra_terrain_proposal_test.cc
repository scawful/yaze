#include "core/lyra_terrain_proposal.h"

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

namespace yaze::core {
namespace {
using Json = nlohmann::json;

constexpr int kMap = 0xA0;

LyraTerrainProposalBinding Binding() {
  return {"synthetic",          "test",
          std::string(64, 'a'), std::string(64, 'b'),
          std::string(64, 'c'), 8};
}

LyraTerrainProfile Profile(char definitions = 'b') {
  LyraTerrainProfile profile;
  profile.tile_count = 8;
  profile.tile16_definition_fingerprint = std::string(64, definitions);
  profile.graphics_fingerprint = "graphics:fixture-v1";
  profile.palette_fingerprint = "palette:fixture-v1";
  profile.animation_fingerprint = "animation:fixture-v1";
  return profile;
}

// Exact Python json.dumps(sort_keys=True, separators=(",", ":"),
// ensure_ascii=True) spellings, written out so digests are pinned to them.
std::string BindingCanonical() {
  return R"({"adapter_id":"test","definition_fingerprint":")" +
         std::string(64, 'b') +
         R"(","game_id":"synthetic","profile_fingerprint":")" +
         std::string(64, 'c') + R"(","rom_sha256":")" + std::string(64, 'a') +
         R"(","tile_count":8})";
}

struct Spec {
  std::string rows = "[[2,3],[4,0]]";
  std::string anchor = "[0,0]";
  std::string overlap = "reject";
  std::string clearance = "[]";
  std::string ports = "[]";
  std::string protected_cells = "[]";
  std::string inventory;  // Canonical object; empty means absent.
  std::string placements = "[[2,2]]";
};

std::string FootprintCanonical(const Spec& spec,
                               const std::string& content_id = "") {
  std::string text = R"({"anchor":)" + spec.anchor + R"(,"binding":)" +
                     BindingCanonical() + R"(,"clearance":)" + spec.clearance;
  if (!content_id.empty())
    text += R"(,"content_id":")" + content_id + "\"";
  text +=
      R"(,"overlap_policy":")" + spec.overlap + R"(","ports":)" + spec.ports +
      R"(,"provenance":{"note":"caf\u00e9 \ud83c\udf0a\u007f\u0001\t\"\\/"})" +
      R"(,"review_state":"reviewed","rows":)" + spec.rows +
      R"(,"schema_version":1,"semantic_id":"synthetic.inlay")" +
      R"(,"units":"tile16","unresolved_behavior":)" +
      R"(["synthetic example has no engine behavior"]})";
  return text;
}

std::string TargetCanonical(const LyraTerrainDocument& document, int map_id,
                            const Spec& spec) {
  std::string rows = "[";
  for (int y = 0; y < LyraTerrainDocument::kMapSize; ++y) {
    rows += y ? ",[" : "[";
    for (int x = 0; x < LyraTerrainDocument::kMapSize; ++x) {
      if (x)
        rows += ",";
      rows += std::to_string(document.MapTile(map_id, x, y).value());
    }
    rows += "]";
  }
  rows += "]";
  std::string text = R"({"binding":)" + BindingCanonical() +
                     R"(,"protected":)" + spec.protected_cells + R"(,"rows":)" +
                     rows + R"(,"schema_version":1)";
  if (!spec.inventory.empty())
    text += R"(,"tile32_inventory":)" + spec.inventory;
  return text + R"(,"units":"tile16"})";
}

// Each entry is {placement, x, y, tile16}.
Json Occupied(std::initializer_list<std::array<int, 4>> cells) {
  Json out = Json::array();
  for (const auto& c : cells) {
    out.push_back(
        Json{{"placement", c[0]}, {"x", c[1]}, {"y", c[2]}, {"tile16", c[3]}});
  }
  return out;
}

// Each entry is {x, y, before, after}.
Json Changes(std::initializer_list<std::array<int, 4>> cells) {
  Json out = Json::array();
  for (const auto& c : cells) {
    out.push_back(
        Json{{"x", c[0]}, {"y", c[1]}, {"before", c[2]}, {"after", c[3]}});
  }
  return out;
}

Json Block(int x, int y, std::array<int, 4> before, std::array<int, 4> after,
           std::vector<int> existing_ids = {}) {
  return Json{{"x", x},
              {"y", y},
              {"before", before},
              {"after", after},
              {"existing_ids", existing_ids}};
}

Json Bundle(const LyraTerrainDocument& document, const Spec& spec,
            const Json& occupied, const Json& changes, const Json& touched,
            int unseen_tuples) {
  const std::string content_id =
      ComputeSourceArtifactSha256(FootprintCanonical(spec));
  const std::string footprint = FootprintCanonical(spec, content_id);
  const std::string target = TargetCanonical(document, kMap, spec);
  const Json report = {
      {"schema_version", 1},
      {"kind", "read_only_tile16_proposal"},
      {"writes_enabled", false},
      {"units", "tile16"},
      {"binding", Json::parse(BindingCanonical())},
      {"semantic_id", "synthetic.inlay"},
      {"content_id", content_id},
      {"target_digest", ComputeSourceArtifactSha256(target)},
      {"request_digest",
       ComputeSourceArtifactSha256(R"({"placements":)" + spec.placements +
                                   R"(,"prefab":)" + footprint + "}")},
      {"placements", Json::parse(spec.placements)},
      {"occupied", occupied},
      {"changes", changes},
      {"touched_tile32", touched},
      {"capacity",
       {{"status", "unknown"},
        {"unseen_tuples", unseen_tuples},
        {"inventory_complete", false}}},
      {"unresolved_behavior",
       Json::array({"synthetic example has no engine behavior"})},
      {"acceptance",
       "artifact proposal only; editor transaction and runtime unqualified"}};
  return {{"schema_version", 1},
          {"kind", "lyra_tile16_proposal_bundle"},
          {"map_id", kMap},
          {"target", Json::parse(target)},
          {"footprint", Json::parse(footprint)},
          {"report", report}};
}

Json FourCellBundle(const LyraTerrainDocument& document, const Spec& spec) {
  return Bundle(
      document, spec,
      Occupied({{0, 2, 2, 2}, {0, 3, 2, 3}, {0, 2, 3, 4}, {0, 3, 3, 0}}),
      Changes({{2, 2, 1, 2}, {3, 2, 1, 3}, {2, 3, 1, 4}, {3, 3, 1, 0}}),
      Json::array({Block(1, 1, {1, 1, 1, 1}, {2, 3, 4, 0})}), 1);
}

Json SparseBundle(const LyraTerrainDocument& document) {
  Spec spec;
  spec.rows = "[[2,null],[0,3]]";
  return Bundle(document, spec,
                Occupied({{0, 2, 2, 2}, {0, 2, 3, 0}, {0, 3, 3, 3}}),
                Changes({{2, 2, 1, 2}, {2, 3, 1, 0}, {3, 3, 1, 3}}),
                Json::array({Block(1, 1, {1, 1, 1, 1}, {2, 1, 0, 3})}), 1);
}

class LyraTerrainProposalTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto created = LyraTerrainDocument::Create(Profile(), 1);
    ASSERT_TRUE(created.ok()) << created.status();
    document_.emplace(std::move(*created));
    // An unchanged guard cell far outside every proposed change.
    Paint(30, 30, 5);
  }

  void Paint(int x, int y, uint16_t tile) {
    ASSERT_TRUE(document_->BeginStroke().ok());
    ASSERT_TRUE(document_->Paint(x, y, tile).ok());
    ASSERT_TRUE(document_->EndStroke().ok());
    document_->MarkSaved();
  }

  void ExpectRefused(const std::string& text, absl::StatusCode code,
                     std::string_view needle = "",
                     const LyraTerrainProposalBinding& binding = Binding()) {
    const auto maps =
        std::make_unique<LyraTerrainDocument::Maps>(document_->maps());
    const auto result = AdmitLyraTerrainProposal(text, *document_, binding);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.status().code(), code) << result.status();
    if (!needle.empty()) {
      EXPECT_NE(std::string(result.status().message()).find(needle),
                std::string::npos)
          << result.status();
    }
    EXPECT_EQ(document_->maps(), *maps);
    EXPECT_FALSE(document_->dirty());
    EXPECT_FALSE(document_->stroke_active());
  }

  void ExpectMalformed(const Json& bundle, std::string_view needle = "") {
    ExpectRefused(bundle.dump(), absl::StatusCode::kInvalidArgument, needle);
  }

  // Inputs the planner itself refuses have no valid report to forge.
  void ExpectPlanRefused(const Spec& spec, std::string_view needle) {
    ExpectMalformed(Bundle(*document_, spec, Json::array(), Json::array(),
                           Json::array(), 0),
                    needle);
  }

  std::optional<LyraTerrainDocument> document_;
};

TEST_F(LyraTerrainProposalTest, AdmitsExactFourCellBatchWithoutMutation) {
  const auto maps =
      std::make_unique<LyraTerrainDocument::Maps>(document_->maps());
  const auto stroke = AdmitLyraTerrainProposal(
      FourCellBundle(*document_, Spec{}).dump(), *document_, Binding());
  ASSERT_TRUE(stroke.ok()) << stroke.status();
  ASSERT_EQ(stroke->changes.size(), 4u);
  const std::array<std::array<int, 4>, 4> expected = {
      {{2, 2, 1, 2}, {3, 2, 1, 3}, {2, 3, 1, 4}, {3, 3, 1, 0}}};
  for (size_t i = 0; i < expected.size(); ++i) {
    const auto& change = stroke->changes[i];
    EXPECT_EQ(change.map_id, kMap);
    EXPECT_EQ(change.x, expected[i][0]);
    EXPECT_EQ(change.y, expected[i][1]);
    EXPECT_EQ(change.before, expected[i][2]);
    EXPECT_EQ(change.after, expected[i][3]);
  }
  EXPECT_EQ(document_->maps(), *maps);
  EXPECT_FALSE(document_->dirty());
  EXPECT_FALSE(document_->stroke_active());

  ASSERT_TRUE(document_->ApplyStroke(*stroke, false).ok());
  EXPECT_EQ(document_->MapTile(kMap, 3, 3).value(), 0);
  EXPECT_EQ(document_->MapTile(kMap, 30, 30).value(), 5);
}

TEST_F(LyraTerrainProposalTest, SparseNullSkipsWhileExplicitZeroWrites) {
  const Json bundle = SparseBundle(*document_);
  const auto stroke =
      AdmitLyraTerrainProposal(bundle.dump(), *document_, Binding());
  ASSERT_TRUE(stroke.ok()) << stroke.status();
  ASSERT_EQ(stroke->changes.size(), 3u);
  for (const auto& change : stroke->changes)
    EXPECT_FALSE(change.x == 3 && change.y == 2);
  EXPECT_EQ(stroke->changes[1].x, 2);
  EXPECT_EQ(stroke->changes[1].y, 3);
  EXPECT_EQ(stroke->changes[1].after, 0);

  // A report that treats the skipped cell as Tile16 zero is forged.
  Json forged = bundle;
  forged["report"]["occupied"].insert(
      forged["report"]["occupied"].begin() + 1,
      Json{{"placement", 0}, {"x", 3}, {"y", 2}, {"tile16", 0}});
  forged["report"]["changes"].insert(
      forged["report"]["changes"].begin() + 1,
      Json{{"x", 3}, {"y", 2}, {"before", 1}, {"after", 0}});
  forged["report"]["touched_tile32"][0]["after"] = {2, 0, 0, 3};
  ExpectMalformed(forged, "recomputed plan");

  // Replacing null with zero changes footprint content.
  Json zeroed = bundle;
  zeroed["footprint"]["rows"][0][1] = 0;
  ExpectMalformed(zeroed, "content_id");
}

TEST_F(LyraTerrainProposalTest, SameOnlyOverlapAdmitsIdenticalWritesOnly) {
  Spec spec;
  spec.rows = "[[2,null],[0,3]]";
  spec.anchor = "[1,1]";
  spec.overlap = "same_only";
  spec.placements = "[[3,3],[3,3]]";
  const Json bundle =
      Bundle(*document_, spec,
             Occupied({{0, 2, 2, 2},
                       {0, 2, 3, 0},
                       {0, 3, 3, 3},
                       {1, 2, 2, 2},
                       {1, 2, 3, 0},
                       {1, 3, 3, 3}}),
             Changes({{2, 2, 1, 2}, {2, 3, 1, 0}, {3, 3, 1, 3}}),
             Json::array({Block(1, 1, {1, 1, 1, 1}, {2, 1, 0, 3})}), 1);
  const auto stroke =
      AdmitLyraTerrainProposal(bundle.dump(), *document_, Binding());
  ASSERT_TRUE(stroke.ok()) << stroke.status();
  EXPECT_EQ(stroke->changes.size(), 3u);

  spec.overlap = "reject";
  ExpectPlanRefused(spec, "overlap");
  spec.overlap = "last_writer_wins";
  ExpectPlanRefused(spec, "overlap_policy");
}

TEST_F(LyraTerrainProposalTest, RefusesBoundsAndProtectedCells) {
  Spec spec;
  spec.placements = "[[31,31]]";
  ExpectPlanRefused(spec, "outside destination");
  spec.placements = "[[2,2]]";
  spec.protected_cells = "[[2,3]]";
  ExpectPlanRefused(spec, "protected cell");
  spec.protected_cells = "[[32,0]]";
  ExpectPlanRefused(spec, "protected cell outside");
  spec.protected_cells = "[]";
  spec.anchor = "[2,0]";
  ExpectPlanRefused(spec, "anchor");
}

TEST_F(LyraTerrainProposalTest, PortsAndClearanceCheckFinalCombinedValues) {
  Spec spec;
  spec.rows = "[[2,null],[0,3]]";
  spec.ports = R"([{"allowed_tile16":[1],"at":[2,0],"name":"east"}])";
  spec.clearance = R"([{"allowed_tile16":[1],"at":[1,0]}])";
  spec.placements = "[[0,0]]";
  const Json bundle = Bundle(
      *document_, spec, Occupied({{0, 0, 0, 2}, {0, 0, 1, 0}, {0, 1, 1, 3}}),
      Changes({{0, 0, 1, 2}, {0, 1, 1, 0}, {1, 1, 1, 3}}),
      Json::array({Block(0, 0, {1, 1, 1, 1}, {2, 1, 0, 3})}), 1);
  const auto stroke =
      AdmitLyraTerrainProposal(bundle.dump(), *document_, Binding());
  ASSERT_TRUE(stroke.ok()) << stroke.status();

  spec.placements = "[[0,0],[2,0]]";
  ExpectPlanRefused(spec, "ports constraint fails");
  spec.placements = "[[30,0]]";
  ExpectPlanRefused(spec, "ports cell outside");
  spec.placements = "[[0,0]]";
  spec.clearance = R"([{"allowed_tile16":[2],"at":[1,0]}])";
  ExpectPlanRefused(spec, "clearance constraint fails");
  spec.clearance = "[]";
  spec.ports = R"([{"allowed_tile16":[],"at":[2,0]}])";
  ExpectPlanRefused(spec, "allowed_tile16");
  spec.ports = R"([{"allowed_tile16":[8],"at":[2,0]}])";
  ExpectPlanRefused(spec, "ports Tile16");
  spec.ports = R"([{"allowed_tile16":[1],"at":[2,0],"walkable":true}])";
  ExpectPlanRefused(spec, "constraint objects");
}

TEST_F(LyraTerrainProposalTest, WholeMapTargetGuardsCellsOutsideChanges) {
  const Json bundle = FourCellBundle(*document_, Spec{});
  Json stale_target = bundle;
  stale_target["target"]["rows"][31][0] = 7;
  ExpectRefused(stale_target.dump(), absl::StatusCode::kFailedPrecondition,
                "stale");

  Paint(30, 30, 6);
  ExpectRefused(bundle.dump(), absl::StatusCode::kFailedPrecondition, "stale");
}

TEST_F(LyraTerrainProposalTest, RefusesWrongBindingAndWrongMap) {
  const Json bundle = FourCellBundle(*document_, Spec{});
  using Mutation = std::function<void(LyraTerrainProposalBinding&)>;
  const std::vector<Mutation> mutations = {
      [](auto& b) { b.game_id = "other"; },
      [](auto& b) { b.adapter_id = "other"; },
      [](auto& b) { b.rom_sha256 = std::string(64, 'd'); },
      [](auto& b) { b.definition_fingerprint = std::string(64, 'd'); },
      [](auto& b) { b.profile_fingerprint = std::string(64, 'd'); },
      [](auto& b) { b.tile_count = 9; },
  };
  for (const auto& mutate : mutations) {
    auto binding = Binding();
    mutate(binding);
    ExpectRefused(bundle.dump(), absl::StatusCode::kFailedPrecondition,
                  "binding", binding);
  }

  Json wrong_map = bundle;
  wrong_map["map_id"] = 0xA1;
  ExpectRefused(wrong_map.dump(), absl::StatusCode::kFailedPrecondition,
                "stale");
  for (const Json& map_id :
       {Json(0x9F), Json(0xC0), Json(true), Json(160.0), Json("A0")}) {
    Json invalid = bundle;
    invalid["map_id"] = map_id;
    ExpectMalformed(invalid);
  }

  Json self_declared = bundle;
  self_declared["footprint"]["binding"]["tile_count"] = 9;
  ExpectMalformed(self_declared, "bindings differ");
  self_declared = bundle;
  self_declared["target"]["binding"]["tile_count"] = true;
  ExpectMalformed(self_declared, "tile_count");

  auto other = LyraTerrainDocument::Create(Profile('d'), 1);
  ASSERT_TRUE(other.ok());
  ASSERT_TRUE(other->BeginStroke().ok());
  ASSERT_TRUE(other->Paint(30, 30, 5).ok());
  ASSERT_TRUE(other->EndStroke().ok());
  const auto refused =
      AdmitLyraTerrainProposal(bundle.dump(), *other, Binding());
  EXPECT_EQ(refused.status().code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(LyraTerrainProposalTest, RefusesForgedReportsAndDigests) {
  const Json bundle = FourCellBundle(*document_, Spec{});
  using Mutation = std::function<void(Json&)>;
  const std::vector<std::pair<std::string, Mutation>> forgeries = {
      {"target_digest",
       [](Json& r) { r["target_digest"] = std::string(64, '0'); }},
      {"request_digest",
       [](Json& r) { r["request_digest"] = std::string(64, '0'); }},
      {"content_id", [](Json& r) { r["content_id"] = std::string(64, '0'); }},
      {"writes_enabled", [](Json& r) { r["writes_enabled"] = true; }},
      {"kind", [](Json& r) { r["kind"] = "approved_tile16_edit"; }},
      {"semantic_id", [](Json& r) { r["semantic_id"] = "other"; }},
      {"binding", [](Json& r) { r["binding"]["tile_count"] = 9; }},
      {"changes", [](Json& r) { std::swap(r["changes"][0], r["changes"][1]); }},
      {"changes", [](Json& r) { r["changes"].push_back(r["changes"][0]); }},
      {"changes", [](Json& r) { r["changes"][0]["before"] = "1"; }},
      {"occupied", [](Json& r) { r["occupied"].push_back(r["occupied"][3]); }},
      {"occupied", [](Json& r) { r["occupied"][0]["tile16"] = true; }},
      {"touched_tile32",
       [](Json& r) { r["touched_tile32"][0]["existing_ids"] = {3}; }},
      {"capacity", [](Json& r) { r["capacity"]["unseen_tuples"] = 0; }},
      {"unresolved_behavior",
       [](Json& r) { r["unresolved_behavior"] = Json::array(); }},
      {"acceptance", [](Json& r) { r["acceptance"] = "approved"; }},
      {"report fields", [](Json& r) { r["approved"] = true; }},
      {"report fields", [](Json& r) { r.erase("acceptance"); }},
  };
  for (const auto& [needle, forge] : forgeries) {
    Json forged = bundle;
    forge(forged["report"]);
    ExpectMalformed(forged, needle);
  }

  Json target_note = bundle;
  target_note["target"]["provenance"] = {{"note", "not in the digest"}};
  ExpectMalformed(target_note, "report.target_digest");
  Json footprint_edit = bundle;
  footprint_edit["footprint"]["provenance"]["note"] = "edited";
  ExpectMalformed(footprint_edit, "content_id");
  Json unreviewed = bundle;
  unreviewed["footprint"]["review_state"] = "suggested";
  ExpectMalformed(unreviewed, "reviewed");
}

TEST_F(LyraTerrainProposalTest, RefusesDuplicateKeysTypesAndUnboundedInput) {
  const std::string text = FourCellBundle(*document_, Spec{}).dump();
  ExpectRefused("{\"map_id\":160," + text.substr(1),
                absl::StatusCode::kInvalidArgument, "duplicate");
  std::string nested = text;
  const std::string anchor = "\"anchor\":[0,0]";
  nested.replace(nested.find(anchor), anchor.size(), anchor + "," + anchor);
  ExpectRefused(nested, absl::StatusCode::kInvalidArgument, "duplicate");

  Spec spec;
  spec.rows = "[[true,3],[4,0]]";
  ExpectPlanRefused(spec, "footprint Tile16");
  spec.rows = "[[2.0,3],[4,0]]";
  ExpectPlanRefused(spec, "floating-point");
  spec.rows = "[[2,3],[4,8]]";
  ExpectPlanRefused(spec, "footprint Tile16");
  spec.rows = "[[null,null],[null,null]]";
  ExpectPlanRefused(spec, "no occupied cells");
  spec.rows = "[[2,3],[4]]";
  ExpectPlanRefused(spec, "rectangular");
  spec = Spec{};
  spec.placements = "[[true,2]]";
  ExpectPlanRefused(spec, "placement");
  spec.placements = "[]";
  ExpectPlanRefused(spec, "placements");

  Json deep = 1;
  for (int i = 0; i < 20; ++i)
    deep = Json::array({deep});
  Json nested_provenance = FourCellBundle(*document_, Spec{});
  nested_provenance["footprint"]["provenance"]["note"] = deep;
  ExpectMalformed(nested_provenance, "nesting");

  Json unknown = FourCellBundle(*document_, Spec{});
  unknown["footprint"]["last_writer_wins"] = true;
  ExpectMalformed(unknown, "unknown fields");
  unknown = FourCellBundle(*document_, Spec{});
  unknown["approved"] = true;
  ExpectMalformed(unknown, "bundle schema");
  unknown = FourCellBundle(*document_, Spec{});
  unknown["schema_version"] = 2;
  ExpectMalformed(unknown, "bundle schema");
  unknown = FourCellBundle(*document_, Spec{});
  unknown["kind"] = "read_only_tile16_proposal";
  ExpectMalformed(unknown, "bundle schema");

  ExpectRefused(std::string(kMaxLyraTerrainProposalBytes + 1, ' '),
                absl::StatusCode::kInvalidArgument, "1 MiB");
  ExpectRefused("not json", absl::StatusCode::kInvalidArgument);
  ExpectRefused("[]", absl::StatusCode::kInvalidArgument);
  ExpectRefused(text + "{}", absl::StatusCode::kInvalidArgument);
}

TEST_F(LyraTerrainProposalTest, RefusesWhileStrokeIsActive) {
  const std::string text = FourCellBundle(*document_, Spec{}).dump();
  ASSERT_TRUE(document_->BeginStroke().ok());
  const auto result = AdmitLyraTerrainProposal(text, *document_, Binding());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_TRUE(document_->stroke_active());
  document_->CancelStroke();
  EXPECT_FALSE(document_->dirty());
}

TEST_F(LyraTerrainProposalTest, RefusesProposalWithoutChanges) {
  Spec spec;
  spec.rows = "[[1,1],[1,1]]";
  const Json bundle =
      Bundle(*document_, spec,
             Occupied({{0, 2, 2, 1}, {0, 3, 2, 1}, {0, 2, 3, 1}, {0, 3, 3, 1}}),
             Json::array(), Json::array(), 0);
  ExpectRefused(bundle.dump(), absl::StatusCode::kFailedPrecondition,
                "no cells");
}

TEST_F(LyraTerrainProposalTest,
       IncompleteInventoryReportsReuseCompleteIsUnsupported) {
  Spec spec;
  spec.inventory =
      R"({"complete":false,"entries":[{"id":7,"tiles16":[2,3,4,0]},)"
      R"({"id":3,"tiles16":[2,3,4,0]}]})";
  const Json bundle =
      Bundle(*document_, spec,
             Occupied({{0, 2, 2, 2}, {0, 3, 2, 3}, {0, 2, 3, 4}, {0, 3, 3, 0}}),
             Changes({{2, 2, 1, 2}, {3, 2, 1, 3}, {2, 3, 1, 4}, {3, 3, 1, 0}}),
             Json::array({Block(1, 1, {1, 1, 1, 1}, {2, 3, 4, 0}, {3, 7})}), 0);
  const auto stroke =
      AdmitLyraTerrainProposal(bundle.dump(), *document_, Binding());
  ASSERT_TRUE(stroke.ok()) << stroke.status();
  EXPECT_EQ(stroke->changes.size(), 4u);

  spec.inventory =
      R"({"complete":false,"entries":[{"id":7,"tiles16":[2,3,4,0]},)"
      R"({"id":7,"tiles16":[1,1,1,1]}]})";
  ExpectPlanRefused(spec, "unique");
  spec.inventory = R"({"complete":"false","entries":[]})";
  ExpectPlanRefused(spec, "boolean");
  spec.inventory =
      R"({"capacity":4,"complete":true,"entries":[{"id":0,"tiles16":[1,1,1,1]}]})";
  ExpectRefused(
      Bundle(*document_, spec, Json::array(), Json::array(), Json::array(), 0)
          .dump(),
      absl::StatusCode::kUnimplemented, "unsupported");
}

}  // namespace
}  // namespace yaze::core
