#include "app/editor/dungeon/dungeon_proposal_overlay.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "yaze_test_paths.h"

namespace yaze::editor {
namespace {

TEST(DungeonProposalOverlayTest, LoadsBundledZoraRoomReview) {
  const auto path =
      std::filesystem::path(YAZE_TEST_FIXTURE_DIR).parent_path().parent_path() /
      "assets/proposals/dungeon/zora-reservoir-review.json";
  std::ifstream file(path, std::ios::binary);
  ASSERT_TRUE(file.is_open()) << path;
  const std::string document((std::istreambuf_iterator<char>(file)), {});
  auto overlay = ParseDungeonProposalOverlay(document);
  ASSERT_TRUE(overlay.ok()) << overlay.status();
  ASSERT_EQ(overlay->rooms.size(), 2u);
  EXPECT_EQ(overlay->rooms[0].room_id, 0x26);
  EXPECT_EQ(overlay->rooms[1].room_id, 0x27);
  ASSERT_FALSE(overlay->steps.empty());
  EXPECT_TRUE(ProposalLayerInStep(*overlay, "current_bank_and_actors", 0));
  EXPECT_FALSE(ProposalLayerInStep(*overlay, "proposed_lower_join", 0));
}

constexpr const char* kValidOverlay = R"json({
  "format": "yaze-dungeon-proposal-overlay",
  "version": 1,
  "title": "Goron $77/$87",
  "status": "Proposal, not applied",
  "notes": ["walk transfer", 7],
  "rooms": [{"room": "0x87", "label": "dock"}, {"room": 119}],
  "layers": [
    {"id": "floor", "label": "New floor", "color": "#4CAF50",
     "shapes": [{"room": "0x87", "type": "rect", "tiles": [40, 16, 48, 23],
                 "label": "Dock floor", "detail": "new"}]},
    {"id": "rail", "color": "#9B633D80", "visible": false,
     "shapes": [{"room": "$77", "type": "path",
                 "tiles": [[16, 44], [24, 44], [24, 37]]},
                {"room": 119, "type": "marker", "tile": [16, 44], "label": "A"},
                {"room": 119, "type": "remove", "tile": [2, 3]}]}
  ]
})json";

TEST(DungeonProposalOverlayTest, ParsesRoomsLayersAndShapes) {
  auto overlay = ParseDungeonProposalOverlay(kValidOverlay);
  ASSERT_TRUE(overlay.ok()) << overlay.status();
  EXPECT_EQ(overlay->title, "Goron $77/$87");
  EXPECT_EQ(overlay->status, "Proposal, not applied");
  ASSERT_EQ(overlay->notes.size(), 1u);  // non-string notes are skipped
  ASSERT_EQ(overlay->rooms.size(), 2u);
  EXPECT_EQ(overlay->rooms[0].room_id, 0x87);
  EXPECT_EQ(overlay->rooms[0].label, "dock");
  EXPECT_EQ(overlay->rooms[1].room_id, 0x77);

  ASSERT_EQ(overlay->layers.size(), 2u);
  const auto& floor = overlay->layers[0];
  EXPECT_EQ(floor.label, "New floor");
  EXPECT_EQ(floor.color_rgba, 0x4CAF50FFu);
  EXPECT_TRUE(floor.visible);
  ASSERT_EQ(floor.shapes.size(), 1u);
  EXPECT_EQ(floor.shapes[0].type, ProposalShapeType::kRect);
  EXPECT_EQ(floor.shapes[0].tiles[0], (ProposalTile{40, 16}));
  EXPECT_EQ(floor.shapes[0].tiles[1], (ProposalTile{48, 23}));

  const auto& rail = overlay->layers[1];
  EXPECT_EQ(rail.label, "rail");  // falls back to id
  EXPECT_EQ(rail.color_rgba, 0x9B633D80u);
  EXPECT_FALSE(rail.visible);
  ASSERT_EQ(rail.shapes.size(), 3u);
  EXPECT_EQ(rail.shapes[0].type, ProposalShapeType::kPath);
  EXPECT_EQ(rail.shapes[0].room_id, 0x77);
  EXPECT_EQ(rail.shapes[0].tiles.size(), 3u);
  EXPECT_EQ(rail.shapes[1].type, ProposalShapeType::kMarker);
  EXPECT_EQ(rail.shapes[2].type, ProposalShapeType::kRemove);
}

std::string WithShape(const std::string& shape) {
  return R"({"format":"yaze-dungeon-proposal-overlay","version":1,
             "rooms":[119],"layers":[{"id":"l","shapes":[)" +
         shape + "]}]}";
}

nlohmann::json TemplatePlanReport(bool valid = true) {
  return {
      {"schema", "yaze.dungeon_template_plan.v1"},
      {"status", valid ? "valid_geometry_plan" : "blocked_geometry_plan"},
      {"valid", valid},
      {"write_performed", false},
      {"runtime_qualified", false},
      {"room_id", 119},
      {"rom_content_sha256", std::string(64, 'a')},
      {"template_sha256", std::string(64, 'b')},
      {"proposal_overlay",
       nlohmann::json::parse(WithShape(
           R"({"room":119,"type":"marker","tile":[12,13],"label":"Missing corner"})"))}};
}

TEST(DungeonProposalOverlayTest, LoadsKnownTemplatePlanReportForms) {
  using json = nlohmann::json;
  for (bool valid : {false, true}) {
    const auto plan = TemplatePlanReport(valid);
    for (const auto& document :
         {plan, json{{"plan", plan}}, json{{"Result", {{"plan", plan}}}}}) {
      auto overlay = ParseDungeonProposalOverlay(document.dump());
      ASSERT_TRUE(overlay.ok()) << overlay.status();
      ASSERT_EQ(overlay->rooms.size(), 1u);
      EXPECT_EQ(overlay->rooms[0].room_id, 119);
      EXPECT_EQ(overlay->layers[0].shapes[0].label, "Missing corner");
      EXPECT_NE(overlay->status.find(valid ? "valid_geometry_plan"
                                           : "blocked_geometry_plan"),
                std::string::npos);
      EXPECT_NE(overlay->status.find("runtime unqualified"), std::string::npos);
      ASSERT_GE(overlay->notes.size(), 3u);
      EXPECT_NE(overlay->notes[0].find("not been matched"), std::string::npos);
      EXPECT_NE(overlay->notes[1].find(std::string(64, 'a')),
                std::string::npos);
      EXPECT_NE(overlay->notes[2].find(std::string(64, 'b')),
                std::string::npos);
    }
  }
}

TEST(DungeonProposalOverlayTest, RejectsAmbiguousOrUnrelatedPlanEnvelopes) {
  using json = nlohmann::json;
  const auto plan = TemplatePlanReport();
  const auto overlay = plan["proposal_overlay"];
  const std::vector<json> documents = {
      {{"plan", plan}, {"Result", {{"plan", plan}}}},
      {{"unrelated", {{"plan", plan}}}},
      {{"Result", {{"nested", {{"plan", plan}}}}}},
      {{"plan", {{"nested", plan}}}},
      {{"unrelated", {{"proposal_overlay", overlay}}}},
      {{"proposal_overlay", overlay}},
  };
  for (const auto& document : documents)
    EXPECT_FALSE(ParseDungeonProposalOverlay(document.dump()).ok())
        << document.dump();
  const auto duplicate = std::string("{\"plan\":") + plan.dump() +
                         ",\"plan\":" + plan.dump() + "}";
  EXPECT_FALSE(ParseDungeonProposalOverlay(duplicate).ok());
}

TEST(DungeonProposalOverlayTest,
     RejectsMalformedPlanProvenanceAndRoomMismatch) {
  using json = nlohmann::json;
  const auto good = TemplatePlanReport();
  for (const char* key :
       {"valid", "status", "write_performed", "runtime_qualified", "room_id",
        "rom_content_sha256", "template_sha256", "proposal_overlay"}) {
    auto plan = good;
    plan.erase(key);
    EXPECT_FALSE(ParseDungeonProposalOverlay(plan.dump()).ok()) << key;
  }
  for (const auto& [key, value] : std::vector<std::pair<std::string, json>>{
           {"valid", 1},
           {"status", "runtime_pass"},
           {"write_performed", true},
           {"runtime_qualified", true},
           {"room_id", 120},
           {"room_id", "119"},
           {"room_id", 296},
           {"rom_content_sha256", std::string(64, 'z')},
           {"template_sha256", "short"},
           {"proposal_overlay", json::array()}}) {
    auto plan = good;
    plan[key] = value;
    EXPECT_FALSE(ParseDungeonProposalOverlay(plan.dump()).ok()) << key;
  }
  auto multiple_rooms = good;
  multiple_rooms["proposal_overlay"]["rooms"].push_back(120);
  EXPECT_FALSE(ParseDungeonProposalOverlay(multiple_rooms.dump()).ok());
  auto malformed_overlay = good;
  malformed_overlay["proposal_overlay"]["layers"] = "not layers";
  EXPECT_FALSE(ParseDungeonProposalOverlay(malformed_overlay.dump()).ok());
}

TEST(DungeonProposalOverlayTest, RejectsInvalidDocuments) {
  const char* cases[] = {
      "not json",
      "[]",
      R"({"format":"other","version":1,"rooms":[1],"layers":[]})",
      R"({"format":"yaze-dungeon-proposal-overlay","version":2,"rooms":[1],"layers":[]})",
      R"({"format":"yaze-dungeon-proposal-overlay","version":1,"rooms":[],"layers":[]})",
      R"({"format":"yaze-dungeon-proposal-overlay","version":1,"rooms":[1,1],"layers":[]})",
      R"({"format":"yaze-dungeon-proposal-overlay","version":1,"rooms":[400],"layers":[]})",
      R"({"format":"yaze-dungeon-proposal-overlay","version":1,"rooms":["0xZZ"],"layers":[]})",
      R"({"format":"yaze-dungeon-proposal-overlay","version":1,"rooms":[1]})",
      R"({"format":"yaze-dungeon-proposal-overlay","version":1,"rooms":[1],
          "layers":[{"id":"a","shapes":[]},{"id":"a","shapes":[]}]})",
      R"({"format":"yaze-dungeon-proposal-overlay","version":1,"rooms":[1],
          "layers":[{"id":"a","color":"red","shapes":[]}]})",
  };
  for (const char* text : cases) {
    EXPECT_FALSE(ParseDungeonProposalOverlay(text).ok()) << text;
  }
}

TEST(DungeonProposalOverlayTest, RejectsInvalidShapes) {
  const std::string cases[] = {
      R"({"room":120,"type":"marker","tile":[1,1]})",   // room not listed
      R"({"type":"marker","tile":[1,1]})",              // missing room
      R"({"room":119,"type":"circle","tile":[1,1]})",   // unknown type
      R"({"room":119,"type":"marker","tile":[64,1]})",  // outside room
      R"({"room":119,"type":"marker","tile":[-1,1]})",
      R"({"room":119,"type":"marker"})",                    // missing tile
      R"({"room":119,"type":"rect","tiles":[5,5,4,6]})",    // reversed
      R"({"room":119,"type":"rect","tiles":[5,5,6]})",      // too short
      R"({"room":119,"type":"path","tiles":[[1,1]]})",      // one point
      R"({"room":119,"type":"path","tiles":[[1,1],[1]]})",  // bad point
  };
  for (const auto& shape : cases) {
    EXPECT_FALSE(ParseDungeonProposalOverlay(WithShape(shape)).ok()) << shape;
  }
  EXPECT_TRUE(
      ParseDungeonProposalOverlay(
          WithShape(R"({"room":119,"type":"rect","tiles":[0,0,63,63]})"))
          .ok());
}

TEST(DungeonProposalOverlayTest, ParsesColors) {
  EXPECT_EQ(*ParseProposalColor("#000000"), 0x000000FFu);
  EXPECT_EQ(*ParseProposalColor("#a1B2c3"), 0xA1B2C3FFu);
  EXPECT_EQ(*ParseProposalColor("#11223344"), 0x11223344u);
  EXPECT_FALSE(ParseProposalColor("112233").ok());
  EXPECT_FALSE(ParseProposalColor("#12345").ok());
  EXPECT_FALSE(ParseProposalColor("#GG0000").ok());
}

TEST(DungeonProposalOverlayTest, PreviewsActorObjectAndOrderedRouteSteps) {
  const char* text = R"json({
    "format":"yaze-dungeon-proposal-overlay", "version":1, "rooms":["$65"],
    "layers":[
      {"id":"prisoners", "shapes":[
        {"room":"$65", "type":"actor", "tile":[42,43], "offset":[2,1],
         "facing":"west", "label":"Thief", "detail":"Proposed script anchor"}]},
      {"id":"escape", "shapes":[
        {"room":"$65", "type":"object", "tile":[25,36],
         "object_id":"$FE7", "size":0, "list":1, "footprint":[4,3]},
        {"room":"$65", "type":"path", "tiles":[[26,42],[26,34],[12,34]]}]}
    ],
    "steps":[
      {"id":"wake", "label":"Wake in cell", "detail":"Talk to the thief.",
       "layers":["prisoners"]},
      {"id":"leave", "layers":["prisoners","escape"]}
    ]
  })json";
  auto overlay = ParseDungeonProposalOverlay(text);
  ASSERT_TRUE(overlay.ok()) << overlay.status();
  const auto& actor = overlay->layers[0].shapes[0];
  EXPECT_EQ(actor.type, ProposalShapeType::kActor);
  EXPECT_EQ(actor.facing, ProposalFacing::kWest);
  // Script anchors use tile origins plus pixels, not marker tile centers.
  EXPECT_EQ(ProposalFootprintToPixels(actor),
            (ProposalPixelRect{338, 345, 354, 361}));
  EXPECT_TRUE(ProposalShapeHitTest(actor, 338, 345, 0));
  EXPECT_FALSE(ProposalShapeHitTest(actor, 337, 345, 0));
  EXPECT_FALSE(ProposalShapeHitTest(actor, 354, 361, 0));
  const auto& object = overlay->layers[1].shapes[0];
  EXPECT_EQ(object.type, ProposalShapeType::kObject);
  EXPECT_EQ(object.object_id, 0xFE7);
  EXPECT_EQ(object.object_list, 1);
  EXPECT_EQ(ProposalFootprintToPixels(object),
            (ProposalPixelRect{200, 288, 232, 312}));
  EXPECT_TRUE(ProposalShapeHitTest(object, 231, 311, 0));
  EXPECT_FALSE(ProposalShapeHitTest(object, 232, 312, 0));
  ASSERT_EQ(overlay->steps.size(), 2u);
  EXPECT_EQ(overlay->steps[1].label, "leave");
  EXPECT_EQ(overlay->steps[0].detail, "Talk to the thief.");
  EXPECT_TRUE(ProposalLayerInStep(*overlay, "escape", -1));
  EXPECT_FALSE(ProposalLayerInStep(*overlay, "escape", 0));
  EXPECT_TRUE(ProposalLayerInStep(*overlay, "prisoners", 0));
  EXPECT_TRUE(ProposalLayerInStep(*overlay, "escape", 1));
  EXPECT_FALSE(ProposalLayerInStep(*overlay, "escape", 2));
  EXPECT_TRUE(ParseDungeonProposalOverlay(kValidOverlay)->steps.empty());
}

TEST(DungeonProposalOverlayTest,
     RequiresExplicitActorFacingAndValidFootprints) {
  const std::string cases[] = {
      R"({"room":119,"type":"actor","tile":[1,1]})",
      R"({"room":119,"type":"actor","tile":[1,1],"facing":"up"})",
      R"({"room":119,"type":"actor","tile":[1,1],"facing":"west","offset":[8,0]})",
      R"({"room":119,"type":"object","tile":[1,1],"object_id":128,"footprint":[0,3]})",
      R"({"room":119,"type":"object","tile":[63,63],"object_id":128,"footprint":[2,1]})",
      R"({"room":119,"type":"object","tile":[1,1],"object_id":"$140","footprint":[2,1]})",
      R"({"room":119,"type":"object","tile":[1,1],"object_id":"$FE7"})",
      R"({"room":119,"type":"object","tile":[1,1],"object_id":"$FE7","list":3,"footprint":[2,1]})",
      R"({"room":119,"type":"object","tile":[1,1],"object_id":"$FE7","size":256,"footprint":[2,1]})",
      R"({"room":119,"type":"object","tile":[1,1],"object_id":4294967424,"footprint":[2,1]})",
      R"({"room":119,"type":"actor","tile":[4294967297,1],"facing":"south"})",
  };
  for (const auto& shape : cases)
    EXPECT_FALSE(ParseDungeonProposalOverlay(WithShape(shape)).ok()) << shape;

  for (const char* facing : {"north", "east", "south", "west"}) {
    auto overlay = ParseDungeonProposalOverlay(WithShape(
        std::string(R"({"room":119,"type":"actor","tile":[0,0],"facing":")") +
        facing + "\"}"));
    ASSERT_TRUE(overlay.ok()) << overlay.status();
    EXPECT_EQ(overlay->layers[0].shapes[0].pixel_offset, (ProposalTile{0, 0}));
  }
}

TEST(DungeonProposalOverlayTest, RejectsMissingAndAmbiguousStepReferences) {
  const std::string cases[] = {
      "{}",
      R"([{"id":"a","layers":["missing"]}])",
      R"([{"id":"a","layers":[]}])",
      R"([{"id":"a","layers":[1]}])",
      R"([{"id":"a","layers":["l","l"]}])",
      R"([{"layers":["l"]}])",
      R"([{"id":"a","layers":["l"]},{"id":"a","layers":["l"]}])",
  };
  for (const auto& steps : cases) {
    const std::string text =
        R"({"format":"yaze-dungeon-proposal-overlay","version":1,
            "rooms":[119],"layers":[{"id":"l","shapes":[]}],"steps":)" +
        steps + "}";
    EXPECT_FALSE(ParseDungeonProposalOverlay(text).ok()) << steps;
  }
}

TEST(DungeonProposalOverlayTest, ConvertsInclusiveTileRectsToPixels) {
  EXPECT_EQ(ProposalTileRectToPixels({40, 16}, {48, 23}),
            (ProposalPixelRect{320, 128, 392, 192}));
  EXPECT_EQ(ProposalTileRectToPixels({3, 3}, {3, 3}),
            (ProposalPixelRect{24, 24, 32, 32}));
  EXPECT_EQ(ProposalTileRectToPixels({0, 0}, {63, 63}),
            (ProposalPixelRect{0, 0, 512, 512}));
}

TEST(DungeonProposalOverlayTest, ComputesCentersAndRoomOrigins) {
  float x = 0, y = 0;
  ProposalTileCenter({16, 44}, &x, &y);
  EXPECT_FLOAT_EQ(x, 132.0f);
  EXPECT_FLOAT_EQ(y, 356.0f);
  EXPECT_FLOAT_EQ(ProposalRoomOriginX(0, 24.0f), 0.0f);
  EXPECT_FLOAT_EQ(ProposalRoomOriginX(2, 24.0f), 1072.0f);
}

TEST(DungeonProposalOverlayTest, HitTestsShapes) {
  ProposalShape rect{ProposalShapeType::kRect, 1, {{2, 2}, {3, 3}}, "", ""};
  EXPECT_TRUE(ProposalShapeHitTest(rect, 16.0f, 16.0f, 0));
  EXPECT_TRUE(ProposalShapeHitTest(rect, 31.9f, 31.9f, 0));
  EXPECT_FALSE(ProposalShapeHitTest(rect, 32.0f, 20.0f, 0));  // half-open

  ProposalShape path{ProposalShapeType::kPath, 1, {{0, 0}, {10, 0}}, "", ""};
  EXPECT_TRUE(ProposalShapeHitTest(path, 40.0f, 6.0f, 3.0f));    // 2px off
  EXPECT_FALSE(ProposalShapeHitTest(path, 40.0f, 10.0f, 3.0f));  // 6px off

  ProposalShape marker{ProposalShapeType::kMarker, 1, {{1, 1}}, "", ""};
  EXPECT_TRUE(ProposalShapeHitTest(marker, 12.0f, 12.0f, 1.0f));
  EXPECT_FALSE(ProposalShapeHitTest(marker, 20.0f, 12.0f, 6.0f));
}

TEST(DungeonProposalOverlayTest, DistanceToSegmentClampsToEndpoints) {
  EXPECT_FLOAT_EQ(ProposalDistanceToSegment(5, 3, 0, 0, 10, 0), 3.0f);
  EXPECT_FLOAT_EQ(ProposalDistanceToSegment(-3, 4, 0, 0, 10, 0), 5.0f);
  EXPECT_FLOAT_EQ(ProposalDistanceToSegment(1, 1, 0, 0, 0, 0), std::sqrt(2.0f));
}

}  // namespace
}  // namespace yaze::editor
