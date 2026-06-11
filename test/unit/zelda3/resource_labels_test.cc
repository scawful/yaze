#include "zelda3/resource_labels.h"

#include <string>

#include "gtest/gtest.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/zelda3_labels.h"

namespace yaze::zelda3 {

class ResourceLabelsTest : public ::testing::Test {
 protected:
  ResourceLabelProvider::ProjectLabels project_labels_;
  core::HackManifest manifest_;

  void SetUp() override {
    // Reset the provider before each test with stable backing storage (avoid
    // dangling pointers across tests).
    project_labels_.clear();
    auto& provider = GetResourceLabels();
    provider.SetProjectLabels(&project_labels_);
    provider.SetHackManifest(nullptr);
  }

  void TearDown() override {
    auto& provider = GetResourceLabels();
    provider.SetHackManifest(nullptr);
    provider.SetProjectLabels(nullptr);
  }
};

TEST_F(ResourceLabelsTest, ResolvesVanillaLabelsByDefault) {
  // Sprite 0 is "Raven" in vanilla ALTTP (US)
  EXPECT_EQ(GetResourceLabels().GetLabel(ResourceType::kSprite, 0x00), "Raven");

  // Tag 0 is "Nothing" or similar
  EXPECT_EQ(GetResourceLabels().GetLabel(ResourceType::kRoomTag, 0x00),
            "Nothing");
}

TEST_F(ResourceLabelsTest, ResolvesHackManifestLabels) {
  const std::string json = R"json(
  {
    "manifest_version": 2,
    "hack_name": "Test Hack",
    "room_tags": {
      "tags": [
        {"tag_id":"0x39","address":"0x000000","name":"Custom_Tag_Label"}
      ]
    }
  }
  )json";

  ASSERT_TRUE(manifest_.LoadFromString(json).ok());
  GetResourceLabels().SetHackManifest(&manifest_);

  // Should resolve from manifest
  EXPECT_EQ(GetResourceLabels().GetLabel(ResourceType::kRoomTag, 0x39),
            "Custom_Tag_Label");

  // Should still fall back for non-existent IDs
  EXPECT_EQ(GetResourceLabels().GetLabel(ResourceType::kRoomTag, 0x00),
            "Nothing");
}

TEST_F(ResourceLabelsTest, PriorityLabels) {
  const std::string json = R"json(
  {
    "manifest_version": 2,
    "room_tags": {
      "tags": [
        {"tag_id":"0x39","address":"0x000000","name":"ManifestLabel"}
      ]
    }
  }
  )json";
  ASSERT_TRUE(manifest_.LoadFromString(json).ok());

  project_labels_["room_tag"]["57"] = "ProjectOverride";  // 0x39 = 57

  GetResourceLabels().SetHackManifest(&manifest_);

  // Project labels take priority over manifest
  EXPECT_EQ(GetResourceLabels().GetLabel(ResourceType::kRoomTag, 0x39),
            "ProjectOverride");
}

TEST_F(ResourceLabelsTest, ResolvesProjectLabelsWithHexPrefixedKeys) {
  project_labels_["room"]["0x33"] = "Shrine of Courage";

  EXPECT_EQ(GetResourceLabels().GetLabel(ResourceType::kRoom, 0x33),
            "Shrine of Courage");
  EXPECT_TRUE(GetResourceLabels().HasProjectLabel(ResourceType::kRoom, 0x33));
}

TEST_F(ResourceLabelsTest, PrefersDecimalLabelsOverHexFallback) {
  project_labels_["room"]["51"] = "Decimal Room Label";  // 0x33 decimal
  project_labels_["room"]["0x33"] = "Hex Room Label";

  EXPECT_EQ(GetResourceLabels().GetLabel(ResourceType::kRoom, 0x33),
            "Decimal Room Label");
}

TEST_F(ResourceLabelsTest, ConvenienceFunctionsWork) {
  const std::string json = R"json(
  {
    "manifest_version": 2,
    "room_tags": {
      "tags": [
        {"tag_id":"0x40","address":"0x000000","name":"TestTag"}
      ]
    }
  }
  )json";
  ASSERT_TRUE(manifest_.LoadFromString(json).ok());
  GetResourceLabels().SetHackManifest(&manifest_);

  EXPECT_EQ(GetRoomTagLabel(0x40), "TestTag");
}

TEST_F(ResourceLabelsTest, RoomObjectLabelsUseCanonicalDungeonNames) {
  const auto& type1 = Zelda3Labels::GetType1RoomObjectNames();
  ASSERT_GT(type1.size(), 0xD6u);
  EXPECT_EQ(type1[0x47], GetObjectName(0x47));
  EXPECT_EQ(type1[0x48], "Waterfall B ↔");
  EXPECT_EQ(type1[0xD3], "Wall moved check A (logic)");

  const auto& type2 = Zelda3Labels::GetType2RoomObjectNames();
  ASSERT_GT(type2.size(), 0x36u);
  EXPECT_EQ(type2[0x2B], GetObjectName(0x12B));
  EXPECT_EQ(type2[0x2C], "Rightwards 6x3 decor");
  EXPECT_EQ(type2[0x35], "Water-hop stairs A");

  const auto& type3 = Zelda3Labels::GetType3RoomObjectNames();
  ASSERT_GT(type3.size(), 0x55u);
  EXPECT_EQ(type3[0x0D], GetObjectName(0xF8D));
  EXPECT_EQ(type3[0x37], "4x4 decor A ↔");
  EXPECT_EQ(type3[0x55], "Utility 3x5 decor");
}

TEST_F(ResourceLabelsTest, ResourceExportIncludesRoomObjectLabels) {
  const auto labels = Zelda3Labels::ToResourceLabels();

  ASSERT_TRUE(labels.contains("room_object"));
  EXPECT_EQ(labels.at("room_object").at(std::to_string(0x47)),
            GetObjectName(0x47));
  EXPECT_EQ(labels.at("room_object").at(std::to_string(0x12B)),
            GetObjectName(0x12B));
  EXPECT_EQ(labels.at("room_object").at(std::to_string(0xF8D)),
            GetObjectName(0xF8D));

  ASSERT_TRUE(labels.contains("room_object_type2"));
  EXPECT_EQ(labels.at("room_object_type2").at(std::to_string(0x35)),
            "Water-hop stairs A");
}

}  // namespace yaze::zelda3
