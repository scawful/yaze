#include "app/editor/dungeon/dungeon_proposal_overlay.h"

#include <cmath>
#include <string>

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

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
