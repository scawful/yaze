#include "zelda3/resource_labels.h"

#include <string>

#include "gtest/gtest.h"

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
  EXPECT_EQ(GetResourceLabels().GetLabel(ResourceType::kRoomTag, 0x00), "Nothing");
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
  EXPECT_EQ(GetResourceLabels().GetLabel(ResourceType::kRoomTag, 0x39), "Custom_Tag_Label");

  // Should still fall back for non-existent IDs
  EXPECT_EQ(GetResourceLabels().GetLabel(ResourceType::kRoomTag, 0x00), "Nothing");
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
  EXPECT_EQ(GetResourceLabels().GetLabel(ResourceType::kRoomTag, 0x39), "ProjectOverride");
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

}  // namespace yaze::zelda3
