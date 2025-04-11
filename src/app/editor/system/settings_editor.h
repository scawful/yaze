#ifndef YAZE_APP_EDITOR_SETTINGS_EDITOR_H
#define YAZE_APP_EDITOR_SETTINGS_EDITOR_H

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/rom.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

// Simple representation for a tree
// (this is designed to be simple to understand for our demos, not to be
// efficient etc.)
struct ExampleTreeNode {
  char Name[28];
  ImGuiID UID = 0;
  ExampleTreeNode* Parent = NULL;
  ImVector<ExampleTreeNode*> Childs;

  // Data
  bool HasData = false;  // All leaves have data
  bool DataIsEnabled = false;
  int DataInt = 128;
  ImVec2 DataVec2 = ImVec2(0.0f, 3.141592f);
};

// Simple representation of struct metadata/serialization data.
// (this is a minimal version of what a typical advanced application may
// provide)
struct ExampleMemberInfo {
  const char* Name;
  ImGuiDataType DataType;
  int DataCount;
  int Offset;
};

// Metadata description of ExampleTreeNode struct.
static const ExampleMemberInfo ExampleTreeNodeMemberInfos[]{
    {"Enabled", ImGuiDataType_Bool, 1,
     offsetof(ExampleTreeNode, DataIsEnabled)},
    {"MyInt", ImGuiDataType_S32, 1, offsetof(ExampleTreeNode, DataInt)},
    {"MyVec2", ImGuiDataType_Float, 2, offsetof(ExampleTreeNode, DataVec2)},
};

static ExampleTreeNode* ExampleTree_CreateNode(const char* name,
                                               const ImGuiID uid,
                                               ExampleTreeNode* parent) {
  ExampleTreeNode* node = IM_NEW(ExampleTreeNode);
  snprintf(node->Name, IM_ARRAYSIZE(node->Name), "%s", name);
  node->UID = uid;
  node->Parent = parent;
  if (parent) parent->Childs.push_back(node);
  return node;
}

// Create example tree data
static ExampleTreeNode* ExampleTree_CreateDemoTree() {
  static const char* root_names[] = {"Apple",     "Banana",     "Cherry",
                                     "Kiwi",      "Mango",      "Orange",
                                     "Pineapple", "Strawberry", "Watermelon"};
  char name_buf[32];
  ImGuiID uid = 0;
  ExampleTreeNode* node_L0 = ExampleTree_CreateNode("<ROOT>", ++uid, NULL);
  for (int idx_L0 = 0; idx_L0 < IM_ARRAYSIZE(root_names) * 2; idx_L0++) {
    snprintf(name_buf, 32, "%s %d", root_names[idx_L0 / 2], idx_L0 % 2);
    ExampleTreeNode* node_L1 = ExampleTree_CreateNode(name_buf, ++uid, node_L0);
    const int number_of_childs = (int)strlen(node_L1->Name);
    for (int idx_L1 = 0; idx_L1 < number_of_childs; idx_L1++) {
      snprintf(name_buf, 32, "Child %d", idx_L1);
      ExampleTreeNode* node_L2 =
          ExampleTree_CreateNode(name_buf, ++uid, node_L1);
      node_L2->HasData = true;
      if (idx_L1 == 0) {
        snprintf(name_buf, 32, "Sub-child %d", 0);
        ExampleTreeNode* node_L3 =
            ExampleTree_CreateNode(name_buf, ++uid, node_L2);
        node_L3->HasData = true;
      }
    }
  }
  return node_L0;
}

struct ExampleAppPropertyEditor {
  ImGuiTextFilter Filter;

  void Draw(ExampleTreeNode* root_node) {
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F,
                               ImGuiInputFlags_Tooltip);
    ImGui::PushItemFlag(ImGuiItemFlags_NoNavDefaultFocus, true);
    if (ImGui::InputTextWithHint("##Filter", "incl,-excl", Filter.InputBuf,
                                 IM_ARRAYSIZE(Filter.InputBuf),
                                 ImGuiInputTextFlags_EscapeClearsAll))
      Filter.Build();
    ImGui::PopItemFlag();

    ImGuiTableFlags table_flags = ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_RowBg;
    if (ImGui::BeginTable("##split", 2, table_flags)) {
      ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthStretch,
                              1.0f);
      ImGui::TableSetupColumn("Contents", ImGuiTableColumnFlags_WidthStretch,
                              2.0f);  // Default twice larger
      // ImGui::TableSetupScrollFreeze(0, 1);
      // ImGui::TableHeadersRow();

      for (ExampleTreeNode* node : root_node->Childs)
        if (Filter.PassFilter(node->Name))  // Filter root node
          DrawTreeNode(node);
      ImGui::EndTable();
    }
  }

  void DrawTreeNode(ExampleTreeNode* node) {
    // Object tree node
    ImGui::PushID((int)node->UID);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGuiTreeNodeFlags tree_flags = ImGuiTreeNodeFlags_None;
    tree_flags |=
        ImGuiTreeNodeFlags_SpanAllColumns |
        ImGuiTreeNodeFlags_AllowOverlap;  // Highlight whole row for visibility
    tree_flags |=
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick;  // Standard opening mode as we
                                               // are likely to want to add
                                               // selection afterwards
    tree_flags |=
        ImGuiTreeNodeFlags_NavLeftJumpsBackHere;  // Left arrow support
    bool node_open =
        ImGui::TreeNodeEx("##Object", tree_flags, "%s", node->Name);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextDisabled("UID: 0x%08X", node->UID);

    // Display child and data
    if (node_open)
      for (ExampleTreeNode* child : node->Childs) DrawTreeNode(child);
    if (node_open && node->HasData) {
      // In a typical application, the structure description would be derived
      // from a data-driven system.
      // - We try to mimic this with our ExampleMemberInfo structure and the
      // ExampleTreeNodeMemberInfos[] array.
      // - Limits and some details are hard-coded to simplify the demo.
      // - Text and Selectable are less high than framed widgets, using
      // AlignTextToFramePadding() we add vertical spacing to make the
      // selectable lines equal high.
      for (const ExampleMemberInfo& field_desc : ExampleTreeNodeMemberInfos) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop | ImGuiItemFlags_NoNav,
                            true);
        ImGui::Selectable(field_desc.Name, false,
                          ImGuiSelectableFlags_SpanAllColumns |
                              ImGuiSelectableFlags_AllowOverlap);
        ImGui::PopItemFlag();
        ImGui::TableSetColumnIndex(1);
        ImGui::PushID(field_desc.Name);
        void* field_ptr = (void*)(((unsigned char*)node) + field_desc.Offset);
        switch (field_desc.DataType) {
          case ImGuiDataType_Bool: {
            IM_ASSERT(field_desc.DataCount == 1);
            ImGui::Checkbox("##Editor", (bool*)field_ptr);
            break;
          }
          case ImGuiDataType_S32: {
            int v_min = INT_MIN, v_max = INT_MAX;
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragScalarN("##Editor", field_desc.DataType, field_ptr,
                               field_desc.DataCount, 1.0f, &v_min, &v_max);
            break;
          }
          case ImGuiDataType_Float: {
            float v_min = 0.0f, v_max = 1.0f;
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderScalarN("##Editor", field_desc.DataType, field_ptr,
                                 field_desc.DataCount, &v_min, &v_max);
            break;
          }
        }
        ImGui::PopID();
      }
    }
    if (node_open) ImGui::TreePop();
    ImGui::PopID();
  }
};

// Demonstrate creating a simple property editor.
static void ShowExampleAppPropertyEditor(bool* p_open) {
  ImGui::SetNextWindowSize(ImVec2(430, 450), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Example: Property editor", p_open)) {
    ImGui::End();
    return;
  }

  static ExampleAppPropertyEditor property_editor;
  static ExampleTreeNode* tree_data = ExampleTree_CreateDemoTree();
  property_editor.Draw(tree_data);

  ImGui::End();
}

class SettingsEditor : public Editor {
 public:
  explicit SettingsEditor(Rom* rom = nullptr) : rom_(rom) { 
    type_ = EditorType::kSettings; 
  }

  void Initialize() override;
  absl::Status Load() override;
  absl::Status Save() override { return absl::UnimplementedError("Save"); }
  absl::Status Update() override;
  absl::Status Cut() override { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() override { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() override { return absl::UnimplementedError("Paste"); }
  absl::Status Undo() override { return absl::UnimplementedError("Undo"); }
  absl::Status Redo() override { return absl::UnimplementedError("Redo"); }
  absl::Status Find() override { return absl::UnimplementedError("Find"); }
  
  // Set the ROM pointer
  void set_rom(Rom* rom) { rom_ = rom; }
  
  // Get the ROM pointer
  Rom* rom() const { return rom_; }

 private:
  Rom* rom_;
  void DrawGeneralSettings();
  void DrawKeyboardShortcuts();
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SETTINGS_EDITOR_H_
