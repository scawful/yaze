#include "zelda3/resource_labels.h"

#include <string>

#include "core/project.h"
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

TEST_F(ResourceLabelsTest, ReceiptNamesIgnoreGenericItemLabels) {
  project_labels_["item"]["0x25"] = "Blue Potion";
  project_labels_["item"]["0xE0"] = "Generic High Item";
  EXPECT_EQ(GetItemLabel(0x25), "Blue Potion");
  EXPECT_EQ(GetItemReceiptLabel(0x25), "Compass");
  EXPECT_STREQ(GetItemReceiptLabelSource(0x25), "vanilla_receipt");
  EXPECT_EQ(GetItemReceiptLabel(0xE0), "Unknown item E0");
  EXPECT_STREQ(GetItemReceiptLabelSource(0xE0), "unknown");
}

TEST_F(ResourceLabelsTest, ReceiptLabelsUseDedicatedNamespaceAndKeyPriority) {
  project_labels_["item"]["37"] = "Blue Potion";
  project_labels_["item_receipt"]["0x25"] = "Big chest key";
  EXPECT_EQ(GetItemReceiptLabel(0x25), "Big chest key");
  EXPECT_STREQ(GetItemReceiptLabelSource(0x25), "project");
  project_labels_["item_receipt"]["37"] = "Decimal Receipt";
  EXPECT_EQ(GetItemReceiptLabel(0x25), "Decimal Receipt");
  project_labels_["item_receipt"]["0xE0"] = "Custom Treasure";
  EXPECT_EQ(GetItemReceiptLabel(0xE0), "Custom Treasure");
  EXPECT_STREQ(GetItemReceiptLabelSource(0xE0), "project");
  project_labels_["item_receipt"]["37"].clear();
  project_labels_["item_receipt"]["0x25"].clear();
  EXPECT_EQ(GetItemReceiptLabel(0x25), "Compass");
  EXPECT_STREQ(GetItemReceiptLabelSource(0x25), "vanilla_receipt");
}

TEST_F(ResourceLabelsTest, ExplicitReceiptContextDoesNotLeakAcrossSessions) {
  project_labels_["item_receipt"]["37"] = "Global session";
  ResourceLabelProvider::ProjectLabels first, second;
  first["item_receipt"]["37"] = "First session";
  second["item"]["37"] = "Blue Potion";
  second["item_receipt"]["0xF0"] = "Second session treasure";
  EXPECT_EQ(GetItemReceiptLabel(0x25, &first), "First session");
  EXPECT_EQ(GetItemReceiptLabel(0x25, &second), "Compass");
  EXPECT_EQ(GetItemReceiptLabel(0xF0, &second), "Second session treasure");
  EXPECT_STREQ(GetItemReceiptLabelSource(0xF0, &second), "project");
  EXPECT_EQ(GetItemReceiptLabel(0xF0, &first), "Unknown item F0");
  EXPECT_STREQ(GetItemReceiptLabelSource(0xF0, &first), "unknown");
  EXPECT_EQ(GetItemReceiptLabel(0x25, nullptr), "Compass");
  EXPECT_STREQ(GetItemReceiptLabelSource(0x25, nullptr), "vanilla_receipt");
  EXPECT_EQ(GetItemReceiptLabel(0xFF, nullptr), "Unknown item FF");
  EXPECT_STREQ(GetItemReceiptLabelSource(0xFF, nullptr), "unknown");
  EXPECT_EQ(GetItemReceiptLabel(0x25), "Global session");
}

TEST_F(ResourceLabelsTest, ZScreamChestNamesRoundTripInReceiptNamespace) {
  project_labels_["item_receipt"]["37"] = "Global session";
  ResourceLabelProvider::ProjectLabels imported;
  imported["item"]["37"] = "Blue Potion";
  imported["item"]["225"] = "Generic High Item";
  ResourceLabelProvider provider;
  provider.SetProjectLabels(&imported);
  ASSERT_TRUE(provider
                  .ImportFromZScreamFormat(
                      "[Chests Items]\n25 Big chest key\nE0 Custom Treasure\n")
                  .ok());
  EXPECT_EQ(imported.at("item_receipt").at("37"), "Big chest key");
  EXPECT_EQ(imported.at("item_receipt").at("224"), "Custom Treasure");
  EXPECT_EQ(imported.at("item").at("37"), "Blue Potion");
  const auto exported = provider.ExportToZScreamFormat();
  EXPECT_NE(exported.find("25 Big chest key\n"), std::string::npos);
  EXPECT_NE(exported.find("24 Small Key\n"), std::string::npos);
  EXPECT_NE(exported.find("E0 Custom Treasure\n"), std::string::npos);
  EXPECT_EQ(exported.find("E1 Generic High Item"), std::string::npos);
  EXPECT_EQ(exported.find("25 Global session"), std::string::npos);
  ResourceLabelProvider::ProjectLabels round_trip;
  ResourceLabelProvider reader;
  reader.SetProjectLabels(&round_trip);
  ASSERT_TRUE(reader.ImportFromZScreamFormat(exported).ok());
  EXPECT_EQ(GetItemReceiptLabel(0x25, &round_trip), "Big chest key");
  EXPECT_EQ(GetItemReceiptLabel(0xE0, &round_trip), "Custom Treasure");
  EXPECT_FALSE(round_trip.contains("item"));
}

TEST_F(ResourceLabelsTest, ZScreamDefaultChestNamesUseReceiptCatalog) {
  const auto exported = GetResourceLabels().ExportToZScreamFormat();
  EXPECT_NE(exported.find("00 Fighter Sword\n"), std::string::npos);
  EXPECT_NE(exported.find("25 Compass\n"), std::string::npos);
  EXPECT_NE(exported.find("32 Big Key\n"), std::string::npos);
  EXPECT_NE(exported.find("33 Map\n"), std::string::npos);
  EXPECT_EQ(exported.find("25 Blue Potion\n"), std::string::npos);
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

TEST_F(ResourceLabelsTest, DestroyedProjectReleasesItsLabelBindings) {
  {
    project::YazeProject project;
    GetResourceLabels().SetHackManifest(&project.hack_manifest);
    GetResourceLabels().SetProjectLabels(&project.resource_labels);
  }

  EXPECT_EQ(GetResourceLabels().hack_manifest(), nullptr);
  EXPECT_EQ(GetResourceLabels().GetAllProjectLabels(), nullptr);
  EXPECT_EQ(GetResourceLabels().GetLabel(ResourceType::kRoomTag, 0x00),
            "Nothing");
}

TEST_F(ResourceLabelsTest, DestroyedProjectKeepsOtherLabelBindings) {
  GetResourceLabels().SetHackManifest(&manifest_);
  { project::YazeProject unrelated; }

  EXPECT_EQ(GetResourceLabels().hack_manifest(), &manifest_);
  EXPECT_EQ(GetResourceLabels().GetAllProjectLabels(), &project_labels_);
}

}  // namespace yaze::zelda3
