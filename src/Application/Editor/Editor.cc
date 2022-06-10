#include "Editor.h"

namespace yaze {
namespace Application {
namespace View {

void Editor::UpdateScreen() {
  const ImGuiIO& io = ImGui::GetIO();
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImVec2 dimensions(io.DisplaySize.x, io.DisplaySize.y);
  ImGui::SetNextWindowSize(dimensions, ImGuiCond_Always);
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_MenuBar;

  if (!ImGui::Begin("Main", nullptr, flags)) {
    ImGui::End();
    return;
  }
  DrawYazeMenu();

  if (ImGui::BeginTabBar("##TabBar")) {
    DrawOverworldEditor();
    ImGui::EndTabBar();
  }

  ImGui::ShowDemoWindow();

  ImGui::End();
}

void Editor::DrawYazeMenu() {
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      DrawFileMenu();
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      DrawEditMenu();
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }

  // display
  if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
    // action if OK
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
      rom.LoadFromFile(filePathName);
    }

    // close
    ImGuiFileDialog::Instance()->Close();
  }
}

void Editor::DrawFileMenu() const {
  if (ImGui::MenuItem("Open", "Ctrl+O")) {
    // TODO: Add the ability to open ALTTP ROM
    ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Open ROM",
                                            ".sfc,.smc", ".");
  }
  if (ImGui::BeginMenu("Open Recent")) {
    ImGui::MenuItem("alttp.sfc");
    // TODO: Display recently accessed files here
    ImGui::EndMenu();
  }
  if (ImGui::MenuItem("Save", "Ctrl+S")) {
    // TODO: Implement this
  }
  if (ImGui::MenuItem("Save As..")) {
    // TODO: Implement this
  }

  ImGui::Separator();

  // TODO: Make these options matter
  if (ImGui::BeginMenu("Options")) {
    static bool enabled = true;
    ImGui::MenuItem("Enabled", "", &enabled);
    ImGui::BeginChild("child", ImVec2(0, 60), true);
    for (int i = 0; i < 10; i++) ImGui::Text("Scrolling Text %d", i);
    ImGui::EndChild();
    static float f = 0.5f;
    static int n = 0;
    ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
    ImGui::InputFloat("Input", &f, 0.1f);
    ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
    ImGui::EndMenu();
  }
}

void Editor::DrawEditMenu() const {
  if (ImGui::MenuItem("Undo", "Ctrl+O")) {
    // TODO: Implement this
  }
  if (ImGui::MenuItem("Undo", "Ctrl+O")) {
    // TODO: Implement this
  }
  ImGui::Separator();
  if (ImGui::MenuItem("Cut", "Ctrl+X")) {
    // TODO: Implement this
  }
  if (ImGui::MenuItem("Copy", "Ctrl+C")) {
    // TODO: Implement this
  }
  if (ImGui::MenuItem("Paste", "Ctrl+V")) {
    // TODO: Implement this
  }
  ImGui::Separator();
  if (ImGui::MenuItem("Find Tiles", "Ctrl+F")) {
    // TODO: Implement this
  }
}

// first step would be to decompress all graphics data from the game
// (in alttp that's easy they're all located in the same location all the same
// sheet size 128x32) have a code that convert PC address to SNES and vice-versa

// 1) find the gfx pointers (you could use ZS constant file)
// 2) decompress all the gfx with your lz2 decompressor
// 3) convert the 3bpp snes data into PC 4bpp (probably the hardest part)
// 4) get the tiles32 data
// 5) get the tiles16 data
// 6) get the map32 data (they must be decompressed as well with a lz2 variant
// not the same as gfx compression but pretty similar)
// 7) get the gfx data of the map
// yeah i forgot that one and load 4bpp in a pseudo vram and use that to
// render tiles on screen
// 8) try to render the tiles on the bitmap in black & white to start
// 9) get the palettes data and try to find how they're loaded in
// the game that's a big puzzle to solve then 9 you'll have an overworld map
// viewer, in less than few hours if are able to understand the data quickly
void Editor::DrawOverworldEditor() {
  if (ImGui::BeginTabItem("Overworld")) {
    static ImVector<ImVec2> points;
    static ImVec2 scrolling(0.0f, 0.0f);
    static bool opt_enable_grid = true;
    static bool opt_enable_context_menu = true;
    static bool adding_line = false;

    ImGui::Checkbox("Enable grid", &opt_enable_grid);
    ImGui::Checkbox("Enable context menu", &opt_enable_context_menu);
    ImGui::Text(
        "Mouse Left: drag to add lines,\nMouse Right: drag to scroll, click "
        "for context menu.");

    ImVec2 canvas_p0 =
        ImGui::GetCursorScreenPos();  // ImDrawList API uses screen coordinates!
    ImVec2 canvas_sz =
        ImGui::GetContentRegionAvail();  // Resize canvas to what's available
    if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
    ImVec2 canvas_p1 =
        ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    // Draw border and background color
    const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(50, 50, 50, 255));
    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

    // This will catch our interactions
    ImGui::InvisibleButton(
        "canvas", canvas_sz,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool is_hovered = ImGui::IsItemHovered();  // Hovered
    const bool is_active = ImGui::IsItemActive();    // Held
    const ImVec2 origin(canvas_p0.x + scrolling.x,
                        canvas_p0.y + scrolling.y);  // Lock scrolled origin
    const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                     io.MousePos.y - origin.y);

    // Add first and second point
    if (is_hovered && !adding_line &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      points.push_back(mouse_pos_in_canvas);
      points.push_back(mouse_pos_in_canvas);
      adding_line = true;
    }
    if (adding_line) {
      points.back() = mouse_pos_in_canvas;
      if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) adding_line = false;
    }

    // Pan (we use a zero mouse threshold when there's no context menu)
    // You may decide to make that threshold dynamic based on whether the mouse
    // is hovering something etc.
    const float mouse_threshold_for_pan =
        opt_enable_context_menu ? -1.0f : 0.0f;
    if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right,
                                            mouse_threshold_for_pan)) {
      scrolling.x += io.MouseDelta.x;
      scrolling.y += io.MouseDelta.y;
    }

    // Context menu (under default mouse threshold)
    ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    if (opt_enable_context_menu && drag_delta.x == 0.0f && drag_delta.y == 0.0f)
      ImGui::OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);
    if (ImGui::BeginPopup("context")) {
      if (adding_line) points.resize(points.size() - 2);
      adding_line = false;
      if (ImGui::MenuItem("Remove one", NULL, false, points.Size > 0)) {
        points.resize(points.size() - 2);
      }
      if (ImGui::MenuItem("Remove all", NULL, false, points.Size > 0)) {
        points.clear();
      }
      ImGui::EndPopup();
    }

    // Draw grid + all lines in the canvas
    draw_list->PushClipRect(canvas_p0, canvas_p1, true);
    if (opt_enable_grid) {
      const float GRID_STEP = 64.0f;
      for (float x = fmodf(scrolling.x, GRID_STEP); x < canvas_sz.x;
           x += GRID_STEP)
        draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y),
                           ImVec2(canvas_p0.x + x, canvas_p1.y),
                           IM_COL32(200, 200, 200, 40));
      for (float y = fmodf(scrolling.y, GRID_STEP); y < canvas_sz.y;
           y += GRID_STEP)
        draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y),
                           ImVec2(canvas_p1.x, canvas_p0.y + y),
                           IM_COL32(200, 200, 200, 40));
    }

    for (int n = 0; n < points.Size; n += 2)
      draw_list->AddLine(
          ImVec2(origin.x + points[n].x, origin.y + points[n].y),
          ImVec2(origin.x + points[n + 1].x, origin.y + points[n + 1].y),
          IM_COL32(255, 255, 0, 255), 2.0f);

    draw_list->PopClipRect();

    ImGui::EndTabItem();
  }
}

}  // namespace View
}  // namespace Application
}  // namespace yaze