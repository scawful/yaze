load("//bazel:yaze.bzl", "COPTS")
load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "imgui",
    srcs = [
        "lib/imgui/imgui.cpp",
        "lib/imgui/imgui_demo.cpp",
        "lib/imgui/imgui_draw.cpp",
        "lib/imgui/imgui_widgets.cpp",
        "lib/ImGuiColorTextEdit/TextEditor.cpp",
    ],
    hdrs = [
        "lib/imgui/imconfig.h",
        "lib/imgui/imgui.h",
        "lib/imgui/imgui_internal.h",
        "lib/imgui/imstb_rectpack.h",
        "lib/imgui/imstb_textedit.h",
        "lib/imgui/imstb_truetype.h",
        "lib/ImGuiColorTextEdit/TextEditor.h",
    ],
    copts = COPTS,
    includes = [
        "lib",
    ],
    visibility = ["//visibility:public"],
)


cc_library(
    name = "sdl2",
    hdrs = glob(["lib/SDL/include/*.h"]),
    includes = ["lib/SDL/include"], # Optional. 
    visibility = ["//visibility:public"],
)