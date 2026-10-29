// yaze build configuration for Dear ImGui.
//
// cmake/dependencies/imgui.cmake passes this file as IMGUI_USER_CONFIG on the
// ImGui target (PUBLIC), so imgui.h includes it in ImGui itself and in every
// translation unit that links ImGui. Put yaze-wide ImGui options here instead
// of defining them per file.
//
// Keep this file layout-neutral: nothing here may change the size or members
// of an ImGui type. src/ios/main.mm is compiled by Xcode without this define,
// and a layout-changing option (IMGUI_DISABLE_OBSOLETE_FUNCTIONS, ImDrawIdx,
// ImTextureID) that does not reach every translation unit makes ImGui read
// mismatched structs. Such options belong in imgui.cmake as PUBLIC definitions
// and in src/ios/project.yml.
//
// IMGUI_ENABLE_TEST_ENGINE is not layout-changing as of 1.93: it adds enum
// values and hook calls only. It must still be defined when imgui.cpp is
// compiled for the hooks to fire, so imgui.cmake sets it on the ImGui target.

#ifndef YAZE_APP_GUI_CORE_YAZE_IMCONFIG_H
#define YAZE_APP_GUI_CORE_YAZE_IMCONFIG_H

// ImVec2/ImVec4 arithmetic operators everywhere. imgui_internal.h requires
// them, and defining them per file failed whenever imgui.h was reached first
// through another header.
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#endif  // YAZE_APP_GUI_CORE_YAZE_IMCONFIG_H
