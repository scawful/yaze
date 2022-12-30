#include "music_editor.h"

#include "gui/icons.h"
#include "imgui.h"

namespace yaze {
namespace app {
namespace editor {

void MusicEditor::Update() {
  ImGui::Text("Preview:");
  DrawToolset();

  if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)9);
      ImGui::BeginChild(child_id, ImVec2(0, 90), false)) {
    DrawPianoStaff();
  }
  ImGui::EndChild();

  ImGui::Separator();
  if (ImGui::BeginTable("MusicEditorColumns", 2, toolset_table_flags_,
                        ImVec2(0, 0))) {
    ImGui::TableSetupColumn("#SongList");
    ImGui::TableSetupColumn("#EditorArea");

    ImGui::TableNextColumn();
    DrawSongList();

    ImGui::TableNextColumn();
    assembly_editor_.InlineUpdate();
    DrawPianoRoll();

    ImGui::EndTable();
  }
}

static const int NUM_KEYS = 25;
static bool keys[NUM_KEYS];

void MusicEditor::DrawPianoStaff() {
  const int NUM_LINES = 5;
  const int LINE_THICKNESS = 1;
  const int LINE_SPACING = 20;

  // Get the draw list for the current window
  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // Draw the staff lines
  ImVec2 canvas_p0 =
      ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
  for (int i = 0; i < NUM_LINES; i++) {
    auto line_start = ImVec2(canvas_p0.x, canvas_p0.y + i * LINE_SPACING);
    auto line_end = ImVec2(ImGui::GetContentRegionAvail().x, canvas_p0.y + i * LINE_SPACING);
    draw_list->AddLine(line_start, line_end, IM_COL32(255, 255, 255, 255),
                       LINE_THICKNESS);
  }

  // Draw the ledger lines
  const int NUM_LEDGER_LINES = 3;
  for (int i = -NUM_LEDGER_LINES; i <= NUM_LINES + NUM_LEDGER_LINES; i++) {
    if (i % 2 == 0) continue;  // skip every other line
    auto line_start = ImVec2(canvas_p0.x, canvas_p0.y + i * LINE_SPACING / 2);
    auto line_end =
        ImVec2(ImGui::GetContentRegionAvail().x, canvas_p0.y + i * LINE_SPACING / 2);
    draw_list->AddLine(line_start, line_end, IM_COL32(150, 150, 150, 255),
                       LINE_THICKNESS);
  }
}

void MusicEditor::DrawPianoRoll() {
  // Render the piano roll
  float key_width = ImGui::GetContentRegionAvail().x / NUM_KEYS;
  float white_key_height = ImGui::GetContentRegionAvail().y * 0.8f;
  float black_key_height = ImGui::GetContentRegionAvail().y * 0.5f;
  ImGui::Text("Click on the keys to toggle notes");
  ImGui::Separator();
  // Get the draw list for the current window
  // ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // draw_list->AddRectFilled(ImVec2(0, 0), ImGui::GetContentRegionAvail(),
  //                          IM_COL32(35, 35, 35, 255));
  // // Iterate through the keys and draw them
  // for (int i = 0; i < NUM_KEYS; i++) {
  //   // Calculate the position and size of the key
  //   ImVec2 key_pos = ImVec2(i * key_width, 0.0f);
  //   ImVec2 key_size;

  //   if (i % 12 == 1 || i % 12 == 3 || i % 12 == 6 || i % 12 == 8 ||
  //       i % 12 == 10) {
  //     // This is a black key
  //     key_size = ImVec2(key_width * 0.6f, black_key_height);
  //     ImVec2 dest;
  //     dest.x = key_pos.x + key_size.x;
  //     dest.y = key_pos.y + key_size.y;
  //     draw_list->AddRectFilled(key_pos, dest, IM_COL32(0, 0, 0, 255));
  //   } else {
  //     // This is a white key
  //     ImVec2 dest;
  //     dest.x = key_pos.x + key_size.x;
  //     dest.y = key_pos.y + key_size.y;
  //     key_size = ImVec2(key_width, white_key_height);
  //     draw_list->AddRectFilled(key_pos, dest, IM_COL32(255, 255, 255,
  //     255));
  //   }
  // }

  for (int i = 0; i < NUM_KEYS; i++) {
    // Calculate the position and size of the key
    ImVec2 key_pos = ImVec2(i * key_width, 0.0f);
    ImVec2 key_size;
    if (i % 12 == 1 || i % 12 == 3 || i % 12 == 6 || i % 12 == 8 ||
        i % 12 == 10) {
      // This is a black key
      key_size = ImVec2(key_width * 0.6f, black_key_height);
    } else {
      // This is a white key
      key_size = ImVec2(key_width, white_key_height);
    }

    ImGui::PushID(i);

    if (ImGui::Button(kSongNotes[i].data(), key_size)) {
      keys[i] ^= 1;
    }
    ImVec2 button_pos = ImGui::GetItemRectMin();
    ImVec2 button_size = ImGui::GetItemRectSize();
    if (keys[i]) {
      ImVec2 dest;
      dest.x = button_pos.x + button_size.x;
      dest.y = button_pos.y + button_size.y;
      ImGui::GetWindowDrawList()->AddRectFilled(button_pos, dest,
                                                IM_COL32(200, 200, 255, 255));
    }
    ImGui::PopID();
    ImGui::SameLine();
  }
}

void MusicEditor::DrawSongList() const {
  static int current_song = 0;
  ImGui::BeginChild("Song List", ImVec2(250, 0));
  ImGui::Text("Select a song to edit:");
  ImGui::ListBox(
      "##songs", &current_song,
      [](void* data, int idx, const char** out_text) {
        *out_text = kGameSongs[idx].data();
        return true;
      },
      nullptr, 30, 30);
  ImGui::EndChild();
}

void MusicEditor::DrawToolset() {
  static bool is_playing = false;
  if (ImGui::BeginTable("DWToolset", 3, toolset_table_flags_, ImVec2(0, 0))) {
    ImGui::TableSetupColumn("#play");
    ImGui::TableSetupColumn("#rewind");
    ImGui::TableSetupColumn("#fastforward");

    ImGui::TableNextColumn();
    if (ImGui::Button(is_playing ? ICON_MD_STOP : ICON_MD_PLAY_ARROW)) {
      is_playing = !is_playing;
    }

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_FAST_REWIND);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_FAST_FORWARD);

    ImGui::EndTable();
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
