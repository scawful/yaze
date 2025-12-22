#include "app/editor/system/editor_panel.h"
#include "app/editor/system/resource_panel.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace yaze {
namespace editor {
namespace {

// =============================================================================
// Mock Panel Implementations for Testing
// =============================================================================

/**
 * @brief Mock panel for testing EditorPanel interface
 */
class MockEditorPanel : public EditorPanel {
 public:
  MockEditorPanel(const std::string& id, const std::string& name,
                  const std::string& icon, const std::string& category)
      : id_(id), name_(name), icon_(icon), category_(category) {}

  std::string GetId() const override { return id_; }
  std::string GetDisplayName() const override { return name_; }
  std::string GetIcon() const override { return icon_; }
  std::string GetEditorCategory() const override { return category_; }

  void Draw(bool* p_open) override {
    draw_count_++;
    if (p_open && close_on_next_draw_) {
      *p_open = false;
    }
  }

  void OnOpen() override { open_count_++; }
  void OnClose() override { close_count_++; }
  void OnFocus() override { focus_count_++; }

  // Test helpers
  int draw_count_ = 0;
  int open_count_ = 0;
  int close_count_ = 0;
  int focus_count_ = 0;
  bool close_on_next_draw_ = false;

 private:
  std::string id_;
  std::string name_;
  std::string icon_;
  std::string category_;
};

/**
 * @brief Mock panel with custom category behavior
 */
class MockPersistentPanel : public MockEditorPanel {
 public:
  using MockEditorPanel::MockEditorPanel;

  PanelCategory GetPanelCategory() const override {
    return PanelCategory::Persistent;
  }
};

/**
 * @brief Mock resource panel for testing ResourcePanel interface
 */
class MockResourcePanel : public ResourcePanel {
 public:
  MockResourcePanel(int resource_id, const std::string& resource_type,
                    const std::string& category)
      : resource_id_(resource_id),
        resource_type_(resource_type),
        category_(category) {}

  int GetResourceId() const override { return resource_id_; }
  std::string GetResourceType() const override { return resource_type_; }
  std::string GetIcon() const override { return "ICON_TEST"; }
  std::string GetEditorCategory() const override { return category_; }

  void Draw(bool* p_open) override { draw_count_++; }

  void OnResourceModified() override { modified_count_++; }
  void OnResourceDeleted() override { deleted_count_++; }

  // Test helpers
  int draw_count_ = 0;
  int modified_count_ = 0;
  int deleted_count_ = 0;

 private:
  int resource_id_;
  std::string resource_type_;
  std::string category_;
};

// =============================================================================
// EditorPanel Interface Tests
// =============================================================================

class EditorPanelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    panel_ = std::make_unique<MockEditorPanel>(
        "test.panel", "Test Panel", "ICON_MD_TEST", "Test");
  }

  std::unique_ptr<MockEditorPanel> panel_;
};

TEST_F(EditorPanelTest, IdentityMethods) {
  EXPECT_EQ(panel_->GetId(), "test.panel");
  EXPECT_EQ(panel_->GetDisplayName(), "Test Panel");
  EXPECT_EQ(panel_->GetIcon(), "ICON_MD_TEST");
  EXPECT_EQ(panel_->GetEditorCategory(), "Test");
}

TEST_F(EditorPanelTest, DefaultBehavior) {
  // Default category is EditorBound
  EXPECT_EQ(panel_->GetPanelCategory(), PanelCategory::EditorBound);

  // Default enabled state is true
  EXPECT_TRUE(panel_->IsEnabled());

  // Default priority is 50
  EXPECT_EQ(panel_->GetPriority(), 50);

  // Default shortcuts and tooltips are empty
  EXPECT_TRUE(panel_->GetShortcutHint().empty());
  EXPECT_TRUE(panel_->GetDisabledTooltip().empty());
}

TEST_F(EditorPanelTest, LifecycleHooks) {
  EXPECT_EQ(panel_->open_count_, 0);
  EXPECT_EQ(panel_->close_count_, 0);
  EXPECT_EQ(panel_->focus_count_, 0);

  panel_->OnOpen();
  EXPECT_EQ(panel_->open_count_, 1);

  panel_->OnFocus();
  EXPECT_EQ(panel_->focus_count_, 1);

  panel_->OnClose();
  EXPECT_EQ(panel_->close_count_, 1);
}

TEST_F(EditorPanelTest, DrawMethod) {
  EXPECT_EQ(panel_->draw_count_, 0);

  bool is_open = true;
  panel_->Draw(&is_open);
  EXPECT_EQ(panel_->draw_count_, 1);
  EXPECT_TRUE(is_open);

  // Test closing via draw
  panel_->close_on_next_draw_ = true;
  panel_->Draw(&is_open);
  EXPECT_EQ(panel_->draw_count_, 2);
  EXPECT_FALSE(is_open);
}

TEST_F(EditorPanelTest, RelationshipDefaults) {
  EXPECT_TRUE(panel_->GetParentPanelId().empty());
  EXPECT_FALSE(panel_->CascadeCloseChildren());
}

// =============================================================================
// PanelCategory Tests
// =============================================================================

TEST(PanelCategoryTest, PersistentPanel) {
  MockPersistentPanel panel("test.persistent", "Persistent Panel",
                            "ICON_MD_PUSH_PIN", "Test");

  EXPECT_EQ(panel.GetPanelCategory(), PanelCategory::Persistent);
}

TEST(PanelCategoryTest, EditorBoundDefault) {
  MockEditorPanel panel("test.bound", "Bound Panel", "ICON_MD_LOCK", "Test");

  EXPECT_EQ(panel.GetPanelCategory(), PanelCategory::EditorBound);
}

// =============================================================================
// ResourcePanel Tests
// =============================================================================

class ResourcePanelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    panel_ = std::make_unique<MockResourcePanel>(42, "room", "Dungeon");
  }

  std::unique_ptr<MockResourcePanel> panel_;
};

TEST_F(ResourcePanelTest, ResourceIdentity) {
  EXPECT_EQ(panel_->GetResourceId(), 42);
  EXPECT_EQ(panel_->GetResourceType(), "room");
}

TEST_F(ResourcePanelTest, GeneratedId) {
  // ID should be generated as "{category}.{type}_{id}"
  EXPECT_EQ(panel_->GetId(), "Dungeon.room_42");
}

TEST_F(ResourcePanelTest, GeneratedDisplayName) {
  // Default display name is "{type} {id}"
  EXPECT_EQ(panel_->GetDisplayName(), "room 42");
}

TEST_F(ResourcePanelTest, SessionSupport) {
  // Default session is 0
  EXPECT_EQ(panel_->GetSessionId(), 0);

  // Can set session
  panel_->SetSessionId(1);
  EXPECT_EQ(panel_->GetSessionId(), 1);
}

TEST_F(ResourcePanelTest, ResourceLifecycle) {
  EXPECT_EQ(panel_->modified_count_, 0);
  EXPECT_EQ(panel_->deleted_count_, 0);

  panel_->OnResourceModified();
  EXPECT_EQ(panel_->modified_count_, 1);

  panel_->OnResourceDeleted();
  EXPECT_EQ(panel_->deleted_count_, 1);
}

TEST_F(ResourcePanelTest, AlwaysEditorBound) {
  // Resource panels are always EditorBound
  EXPECT_EQ(panel_->GetPanelCategory(), PanelCategory::EditorBound);
}

TEST_F(ResourcePanelTest, AllowMultipleInstancesDefault) {
  EXPECT_TRUE(panel_->AllowMultipleInstances());
}

// =============================================================================
// ResourcePanelLimits Tests
// =============================================================================

TEST(ResourcePanelLimitsTest, DefaultLimits) {
  EXPECT_EQ(ResourcePanelLimits::kMaxRoomPanels, 8);
  EXPECT_EQ(ResourcePanelLimits::kMaxSongPanels, 4);
  EXPECT_EQ(ResourcePanelLimits::kMaxSheetPanels, 6);
  EXPECT_EQ(ResourcePanelLimits::kMaxMapPanels, 8);
  EXPECT_EQ(ResourcePanelLimits::kMaxTotalResourcePanels, 20);
}

// =============================================================================
// Multiple Panel Types Tests
// =============================================================================

TEST(MultiplePanelTest, DifferentResourceTypes) {
  MockResourcePanel room_panel(42, "room", "Dungeon");
  MockResourcePanel song_panel(5, "song", "Music");
  MockResourcePanel sheet_panel(100, "sheet", "Graphics");

  EXPECT_EQ(room_panel.GetId(), "Dungeon.room_42");
  EXPECT_EQ(song_panel.GetId(), "Music.song_5");
  EXPECT_EQ(sheet_panel.GetId(), "Graphics.sheet_100");
}

TEST(MultiplePanelTest, SameResourceDifferentSessions) {
  MockResourcePanel session0_room(42, "room", "Dungeon");
  MockResourcePanel session1_room(42, "room", "Dungeon");

  session0_room.SetSessionId(0);
  session1_room.SetSessionId(1);

  // Same resource ID and type
  EXPECT_EQ(session0_room.GetResourceId(), session1_room.GetResourceId());
  EXPECT_EQ(session0_room.GetResourceType(), session1_room.GetResourceType());

  // But different sessions
  EXPECT_NE(session0_room.GetSessionId(), session1_room.GetSessionId());
}

// =============================================================================
// Panel Collection Tests (for future PanelManager integration)
// =============================================================================

TEST(PanelCollectionTest, PolymorphicStorage) {
  std::vector<std::unique_ptr<EditorPanel>> panels;

  panels.push_back(std::make_unique<MockEditorPanel>(
      "test.static", "Static Panel", "ICON_1", "Test"));
  panels.push_back(
      std::make_unique<MockResourcePanel>(42, "room", "Dungeon"));

  EXPECT_EQ(panels.size(), 2);
  EXPECT_EQ(panels[0]->GetId(), "test.static");
  EXPECT_EQ(panels[1]->GetId(), "Dungeon.room_42");

  // Both can be drawn polymorphically
  for (auto& panel : panels) {
    bool open = true;
    panel->Draw(&open);
  }
}

}  // namespace
}  // namespace editor
}  // namespace yaze
